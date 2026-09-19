// freelist.c -- the Buddy System's free lists and buddy bitmaps.
//
// Author: Caleb Backer
// Class:  CS 452, HW2
// Date:   September 2026
//
// For each order e, l <= e <= u, a Level holds:
//
//   head   a singly linked list of the free blocks of order e.  The
//          link is stored in the first word of each free block, so the
//          lists cost no memory beyond the free blocks themselves, and
//          allocated blocks hold no management data at all.
//
//   free   a buddy-pair bitmap, flipped whenever either buddy of a pair
//          enters or leaves list e.  So a pair's bit is 1 iff exactly
//          one buddy is on the list.  When freeing a block, which is not
//          on the list, the bit says whether its buddy is: the Linux
//          kernel's trick for deciding, without a search, to merge.
//
//   alloc  a buddy-pair bitmap, set when either buddy is allocated at
//          order e, and cleared when the pair merges.  An allocated
//          block's size is not stored anywhere; freelistsize() recovers
//          it as the smallest order whose alloc bit covers the address.
//
// Freeing a block whose buddy is not free leaves the pair's alloc bit
// set, because the buddy might be allocated at the same order.  If the
// buddy was split instead, the bit is merely stale: freelistsize()
// never looks above an allocated block's own order, and merging the
// pair later clears the bit.

#include "freelist.h"
#include "bbm.h"
#include "utils.h"

typedef struct {
  void *head;                   // first free block, or 0
  BBM free;                     // pair bit: exactly one buddy on list
  BBM alloc;                    // pair bit: a buddy allocated here
} Level;

typedef struct Rep {
  void *base;                   // the pool
  size_t size;                  // its bytes, a multiple of 2^l
  int l, u;                     // smallest and largest orders
  Level lv[];                   // lv[e-l], for l <= e <= u
} *Rep;

static size_t repsize(int l, int u) {
  return sizeof(struct Rep)+(u-l+1)*sizeof(Level);
}

// Return order e's Level.
static Level *lv(Rep r, int e) { return &r->lv[e-r->l]; }

// Return the free block after b on its list.
static void *next(void *b) { return *(void **)b; }

// Flip the bit for mem's buddy pair of order e.  (bbm has no inverse.)
static void flip(BBM b, void *base, void *mem, int e) {
  if (bbmtst(b,base,mem,e))
    bbmclr(b,base,mem,e);
  else
    bbmset(b,base,mem,e);
}

// Put block b, of order e, at the front of list e.
static void push(Rep r, int e, void *b) {
  Level *v=lv(r,e);
  *(void **)b=v->head;
  v->head=b;
  flip(v->free,r->base,b,e);
}

// Take the front block off list e; return it, or 0 if the list is empty.
static void *pop(Rep r, int e) {
  Level *v=lv(r,e);
  void *b=v->head;
  if (b) {
    v->head=next(b);
    flip(v->free,r->base,b,e);
  }
  return b;
}

// Take block b off list e, wherever it is on it.
static void unlist(Rep r, int e, void *b) {
  Level *v=lv(r,e);
  for (void **pp=&v->head; *pp; pp=*pp)
    if (*pp==b) {
      *pp=next(b);
      flip(v->free,r->base,b,e);
      return;
    }
}

// Return 1 iff block b is on list e.
static int onlist(Rep r, int e, void *b) {
  for (void *p=lv(r,e)->head; p; p=next(p))
    if (p==b)
      return 1;
  return 0;
}

// Return 1 iff mem is where a block of order e could begin.
static int aligned(Rep r, void *mem, int e) {
  return !((size_t)(mem-r->base)&(e2size(e)-1));
}

// Take a block of order e off the lists and return it, splitting a
// larger block if list e is empty; return 0 if there is none to split.
// A split keeps the lower half and frees the upper half, its buddy.
static void *take(Rep r, int e) {
  if (e>r->u)
    return 0;
  void *b=pop(r,e);
  if (b)
    return b;
  b=take(r,e+1);
  if (b)
    push(r,e,baddrset(r->base,b,e));
  return b;
}

extern void freelistdelete(FreeList f) {
  Rep r=f;
  for (int e=r->l; e<=r->u; e++) {
    Level *v=lv(r,e);
    if (v->free)  bbmdelete(v->free);   // 0 if freelistcreate() failed
    if (v->alloc) bbmdelete(v->alloc);
  }
  mmfree(r,repsize(r->l,r->u));
}

extern FreeList freelistcreate(void *base, size_t size, int l, int u) {
  Rep r=mmalloc(repsize(l,u));  // zeroed: lists empty, bitmaps 0
  if (r==MMFAILED)
    return 0;
  r->base=base;
  r->size=size;
  r->l=l;
  r->u=u;
  for (int e=l; e<=u; e++) {
    Level *v=lv(r,e);
    v->free=bbmcreate(size,e);
    v->alloc=bbmcreate(size,e);
    if (!v->free || !v->alloc) {
      freelistdelete(r);
      return 0;
    }
  }
  // Tile the pool with the largest aligned blocks of order <= u: as
  // many of order u as fit, then one block per 1-bit of the remainder,
  // largest first.  Pushing from the top of the pool down leaves each
  // list in address order.  A remainder block's buddy lies (at least
  // partly) past the pool, so pushing it leaves its pair's free bit 1:
  // the buddy is never free, and the block never merges.
  size_t top=e2size(u), off=size;
  for (int e=l; e<u; e++)
    if (size%top & e2size(e))
      push(r,e,base+(off-=e2size(e)));
  while (off)
    push(r,u,base+(off-=top));
  return r;
}

extern void *freelistalloc(FreeList f, int e) {
  Rep r=f;
  if (e<r->l || e>r->u)
    return 0;
  void *b=take(r,e);
  if (b)
    bbmset(lv(r,e)->alloc,r->base,b,e);
  return b;
}

extern void freelistfree(FreeList f, void *mem, int e) {
  Rep r=f;
  // mem is not on list e, so its pair's free bit is 1 iff its buddy is.
  // Order-u blocks have no buddy: they never merge.
  for ( ; e<r->u && bbmtst(lv(r,e)->free,r->base,mem,e); e++) {
    unlist(r,e,baddrinv(r->base,mem,e));  // absorb the free buddy
    bbmclr(lv(r,e)->alloc,r->base,mem,e); // neither buddy is allocated
    mem=baddrclr(r->base,mem,e);          // the merged block: lower half
  }
  push(r,e,mem);
}

// An allocated block of order c is not on a list and has its alloc bit
// set, and every smaller block inside it is neither on a list nor
// allocated, so every pair inside it has both bits 0.  Scan upward from
// l: pairs inside a real block show 0s until its own order.  A pointer
// into a split or free block, or into the middle of a block, fails.
extern int freelistsize(FreeList f, void *mem) {
  Rep r=f;
  if (mem<r->base || mem>=r->base+r->size)
    return -1;
  for (int e=r->l; e<=r->u; e++) {
    Level *v=lv(r,e);
    if (bbmtst(v->alloc,r->base,mem,e))
      return aligned(r,mem,e) && !onlist(r,e,mem) ? e : -1;
    if (bbmtst(v->free,r->base,mem,e))
      return -1;                // mem is in a split block
  }
  return -1;
}

// Return the order of the free block beginning at b, or -1.
static int freeorder(Rep r, void *b) {
  for (int e=r->l; e<=r->u; e++)
    if (onlist(r,e,b))
      return e;
  return -1;
}

// Visit every block of the pool, in address order, with its offset,
// order, and kind: 'A'llocated, 'F'ree, or '?' (corrupt: a 2^l step).
typedef void (*Visit)(void *ctx, size_t off, int e, char kind);

static void walk(Rep r, Visit visit, void *ctx) {
  for (size_t off=0; off<r->size; ) {
    char kind='A';
    int e=freelistsize(r,r->base+off);
    if (e<0) { kind='F'; e=freeorder(r,r->base+off); }
    if (e<0) { kind='?'; e=r->l; }
    visit(ctx,off,e,kind);
    off+=e2size(e);
  }
}

typedef struct { char *buf; size_t len, n; } Str;

static void strblock(void *ctx, size_t off, int e, char kind) {
  Str *s=ctx;
  if (s->n>=s->len)             // truncated already
    return;
  s->n+=snprintf(s->buf+s->n,s->len-s->n,"%s%zu:%c%zu",
                 s->n ? " " : "",off,kind,e2size(e));
}

extern char *freeliststr(FreeList f, char *buf, size_t len) {
  Str s={buf,len,0};
  if (len)
    buf[0]='\0';
  walk(f,strblock,&s);
  return buf;
}

static void prtblock(void *ctx, size_t off, int e, char kind) {
  int *n=ctx;
  printf("%s %zu:%c%zu",(*n && *n%8==0) ? "\n " : "",off,kind,e2size(e));
  (*n)++;
}

// Print bitmap b's bits, one per order-e pair, in address order.
// (Clearer than bbmprt()'s hex bytes, highest first.)
static void prtpairs(Rep r, BBM b, int e) {
  size_t pairs=divup(divup(r->size,e2size(e)),2);
  for (size_t i=0; i<pairs; i++)
    printf("%s%d",(i && i%8==0) ? " " : "",
           bbmtst(b,r->base,r->base+i*e2size(e+1),e));
  printf("\n");
}

extern void freelistprint(FreeList f) {
  Rep r=f;
  int n=0;
  printf(" blocks (offset:Allocated/Free size):\n ");
  walk(r,prtblock,&n);
  printf("\n");
  for (int e=r->u; e>=r->l; e--) {
    Level *v=lv(r,e);
    printf(" order %d (%zu bytes), free list:",e,e2size(e));
    if (!v->head)
      printf(" empty");
    for (void *b=v->head; b; b=next(b))
      printf(" %zu",(size_t)(b-r->base));
    printf("\n   free  pair bits: ");
    prtpairs(r,v->free,e);
    printf("   alloc pair bits: ");
    prtpairs(r,v->alloc,e);
  }
}
