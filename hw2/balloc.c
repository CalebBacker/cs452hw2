// balloc.c -- a Buddy System allocator: the public interface.
//
// Author: Caleb Backer
// Class:  CS 452, HW2
// Date:   September 2026
//
// A pool is an mmap()ed region, of size rounded up to whole blocks of
// 2^l bytes, managed by a FreeList (freelist.c), which does the Buddy
// algorithm.  This module validates arguments, rounds requests up to an
// order, and reports misuse.  All of a pool's memory, including its
// bookkeeping, is mapped by bcreate() and unmapped by bdelete(); no
// other call maps memory, and nothing here calls malloc().

#include "balloc.h"
#include "freelist.h"
#include "utils.h"
#include "error.h"

typedef struct {
  void *base;                   // the pool's memory
  size_t size;                  // its bytes, a multiple of 2^l
  int l, u;                     // smallest and largest orders
  FreeList fl;
} *Rep;

// bbm.c builds its masks with int shifts, which stop at 2^30.
static const int maxu=30;

// A free block must hold its free-list link.
static int minl(void) { return size2e(sizeof(void *)); }

// Convert pool to a Rep, warning (on behalf of who) if it is 0.
static Rep rep(Balloc pool, const char *who) {
  if (!pool)
    WARN("%s: zero pool",who);
  return pool;
}

extern Balloc bcreate(unsigned int size, int l, int u) {
  if (!size) {
    WARN("bcreate: size is 0");
    return 0;
  }
  if (l<minl()) {
    WARN("bcreate: l=%d is too small: a free block holds a pointer, "
         "so l must be at least %d",l,minl());
    return 0;
  }
  if (u<l || u>maxu) {
    WARN("bcreate: u=%d must be in [l,%d], with l=%d",u,maxu,l);
    return 0;
  }
  size_t bytes=divup(size,e2size(l))*e2size(l);
  Rep r=mmalloc(sizeof(*r));
  if (r==MMFAILED) {
    WARN("bcreate: mmap() failed");
    return 0;
  }
  r->base=mmalloc(bytes);
  if (r->base==MMFAILED) {
    WARN("bcreate: mmap() of %zu bytes failed",bytes);
    mmfree(r,sizeof(*r));
    return 0;
  }
  r->fl=freelistcreate(r->base,bytes,l,u);
  if (!r->fl) {
    WARN("bcreate: mmap() failed");
    mmfree(r->base,bytes);
    mmfree(r,sizeof(*r));
    return 0;
  }
  r->size=bytes;
  r->l=l;
  r->u=u;
  return r;
}

extern void bdelete(Balloc pool) {
  Rep r=pool;
  if (!r)
    return;
  freelistdelete(r->fl);
  mmfree(r->base,r->size);
  mmfree(r,sizeof(*r));
}

// A request smaller than 2^l gets 2^l bytes; one larger than 2^u, or
// one the pool cannot satisfy, gets 0, as from malloc(): not an error.
extern void *balloc(Balloc pool, unsigned int size) {
  Rep r=rep(pool,"balloc");
  if (!r)
    return 0;
  int e=size2e(size);
  if (e<r->l)
    e=r->l;
  return e<=r->u ? freelistalloc(r->fl,e) : 0;
}

// Freeing 0 does nothing, as with free().
extern void bfree(Balloc pool, void *mem) {
  if (!mem)
    return;
  Rep r=rep(pool,"bfree");
  if (!r)
    return;
  int e=freelistsize(r->fl,mem);
  if (e<0) {
    WARN("bfree: %p is not an allocated block of pool %p",mem,pool);
    return;
  }
  freelistfree(r->fl,mem,e);
}

extern unsigned int bsize(Balloc pool, void *mem) {
  Rep r=rep(pool,"bsize");
  if (!r)
    return 0;
  int e=freelistsize(r->fl,mem);
  if (e<0) {
    WARN("bsize: %p is not an allocated block of pool %p",mem,pool);
    return 0;
  }
  return e2size(e);
}

extern void bprint(Balloc pool) {
  Rep r=rep(pool,"bprint");
  if (!r)
    return;
  printf("pool %p: %zu bytes at %p, orders %d..%d (%zu..%zu bytes)\n",
         pool,r->size,r->base,r->l,r->u,e2size(r->l),e2size(r->u));
  freelistprint(r->fl);
}
