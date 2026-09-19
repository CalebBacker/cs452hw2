// freelist_test.c -- tests for the free lists: the Buddy algorithm.
//
// Author: Caleb Backer
// Class:  CS 452, HW2
// Date:   September 2026
//
// Most tests compare the pool's map, from freeliststr(), with the exact
// layout the algorithm should produce: "offset:A|Fsize" per block.

#include <string.h>

#include "test.h"
#include "freelist.h"
#include "utils.h"

static char buf[1024];

static char *map(FreeList f) { return freeliststr(f,buf,sizeof(buf)); }

// A pool of size bytes, and its lists.
typedef struct { void *base; size_t size; FreeList f; } Pool;

static Pool mk(size_t size, int l, int u) {
  Pool p={mmalloc(size),size,0};
  p.f=freelistcreate(p.base,size,l,u);
  return p;
}

static void rm(Pool p) {
  freelistdelete(p.f);
  mmfree(p.base,p.size);
}

static void test_tile(void) {
  printf("freelist tiling:\n");
  Pool p=mk(256,4,8);
  checkstr(map(p.f),"0:F256","a power-of-two pool is one block");
  rm(p);
  p=mk(256,4,6);
  checkstr(map(p.f),"0:F64 64:F64 128:F64 192:F64",
           "a pool larger than 2^u is blocks of 2^u");
  rm(p);
  p=mk(208,4,8);
  checkstr(map(p.f),"0:F128 128:F64 192:F16",
           "any other pool is its binary digits, largest first");
  rm(p);
  p=mk(208,4,6);
  checkstr(map(p.f),"0:F64 64:F64 128:F64 192:F16","both at once");
  rm(p);
}

static void test_split_merge(void) {
  printf("freelist split and merge:\n");
  Pool p=mk(256,4,8);
  void *a=freelistalloc(p.f,4);
  check(a==p.base,"the first block is at the bottom of the pool");
  checkstr(map(p.f),"0:A16 16:F16 32:F32 64:F64 128:F128",
           "allocating splits the pool, freeing each upper half");
  void *b=freelistalloc(p.f,4);
  check(b==p.base+16,"a free block of the right order is used first");
  void *c=freelistalloc(p.f,6);
  checkstr(map(p.f),"0:A16 16:A16 32:F32 64:A64 128:F128",
           "two 16s and a 64");
  check(freelistsize(p.f,a)==4 && freelistsize(p.f,b)==4 &&
        freelistsize(p.f,c)==6,
        "freelistsize finds each block's order");
  check(freelistsize(p.f,p.base+32)==-1,"a free block has no size");
  check(freelistsize(p.f,a+8)==-1 && freelistsize(p.f,c+16)==-1,
        "neither does the middle of a block");
  check(freelistsize(p.f,p.base+256)==-1 && freelistsize(p.f,p.base-16)==-1,
        "nor anything outside the pool");

  freelistfree(p.f,a,4);
  checkstr(map(p.f),"0:F16 16:A16 32:F32 64:A64 128:F128",
           "a block whose buddy is allocated does not merge");
  freelistfree(p.f,b,4);
  checkstr(map(p.f),"0:F64 64:A64 128:F128",
           "a block merges with its free buddy, repeatedly");
  check(freelistsize(p.f,a)==-1 && freelistsize(p.f,b)==-1,
        "freed blocks, now inside a larger free block, have no size");
  freelistfree(p.f,c,6);
  checkstr(map(p.f),"0:F256","freeing everything restores the pool");
  check(!freelistalloc(p.f,3) && !freelistalloc(p.f,9),
        "orders outside [l,u] are refused");
  rm(p);
}

static void test_exhaust(void) {
  printf("freelist exhaustion:\n");
  Pool p=mk(256,4,8);
  void *m[16];
  int ok=1;
  for (int i=0; i<16; i++) {
    m[i]=freelistalloc(p.f,4);
    ok&=m[i]==p.base+16*i;
  }
  check(ok,"sixteen 16s fill the pool, in address order");
  check(!freelistalloc(p.f,4) && !freelistalloc(p.f,5),
        "then nothing is left");
  static const int order[]={5,0,15,8,3,12,1,9,14,6,2,11,4,13,7,10};
  for (int i=0; i<16; i++)
    freelistfree(p.f,m[order[i]],4);
  checkstr(map(p.f),"0:F256","freed in any order, they all merge");
  rm(p);
}

static void test_orphans(void) {
  printf("freelist blocks without buddies:\n");
  Pool p=mk(208,4,8);
  void *a=freelistalloc(p.f,4);
  check(a==p.base+192,"a 16 comes from the pool's 16-byte remainder");
  freelistfree(p.f,a,4);
  checkstr(map(p.f),"0:F128 128:F64 192:F16",
           "freed, it cannot merge: its buddy lies past the pool");
  void *b=freelistalloc(p.f,6), *c=freelistalloc(p.f,6);
  checkstr(map(p.f),"0:A64 64:F64 128:A64 192:F16",
           "the 64 remainder is used before splitting the 128");
  freelistfree(p.f,c,6);
  freelistfree(p.f,b,6);
  checkstr(map(p.f),"0:F128 128:F64 192:F16","and the tiling is restored");
  rm(p);
}

static void test_stale(void) {
  printf("freelist alloc bits:\n");
  Pool p=mk(256,4,8);
  void *a=freelistalloc(p.f,6);  // 0
  void *b=freelistalloc(p.f,4);  // 64, splitting a's buddy
  freelistfree(p.f,a,6);
  checkstr(map(p.f),"0:F64 64:A16 80:F16 96:F32 128:F128",
           "a 64 whose buddy is split does not merge");
  check(freelistsize(p.f,b)==4,"the split buddy's block has its size");
  freelistfree(p.f,b,4);
  checkstr(map(p.f),"0:F256","freeing it merges everything");
  void *c=freelistalloc(p.f,7);
  check(c==p.base && freelistsize(p.f,c)==7,
        "a merge clears the pair's alloc bit: a new 128 at 0 is a 128");
  rm(p);
}

static void test_misc(void) {
  printf("freelist odds and ends:\n");
  void *m=mmalloc(4096);
  void *base=m+8;               // a pool at an unaligned address
  FreeList f=freelistcreate(base,256,4,8);
  void *a=freelistalloc(f,4), *b=freelistalloc(f,5);
  check(a==base && b==base+32,
        "blocks are aligned relative to the pool, wherever it begins");
  freelistfree(f,a,4);
  freelistfree(f,b,5);
  checkstr(map(f),"0:F256","and merge the same way");

  a=freelistalloc(f,4);
  char small[10];
  freeliststr(f,small,sizeof(small));
  check(strlen(small)==9 && !strncmp(small,"0:A16 16:",9),
        "freeliststr truncates to fit, and terminates");
  printf("  freelistprint of 0:A16 in a 256-byte pool:\n");
  freelistprint(f);
  freelistdelete(f);
  mmfree(m,4096);
}

extern void test_freelist(void) {
  test_tile();
  test_split_merge();
  test_exhaust();
  test_orphans();
  test_stale();
  test_misc();
}
