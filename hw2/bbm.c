// bbm.c -- a buddy-pair bitmap, and buddy address arithmetic.
//
// (Provided module; documentation added, code unchanged.)
//
// A BBM is just a BM (bm.c) whose bits are indexed by buddy pair
// instead of by number: this module converts an address to a pair's
// bit number.  All address arithmetic is on offsets from base, since
// the pool itself may begin anywhere.

#include "bbm.h"
#include "bm.h"
#include "utils.h"

// Return the number of order-e buddy pairs in size bytes: the number
// of order-e blocks, rounded up, halved and rounded up again.
static size_t mapsize(size_t size, int e) {
  size_t blocksize=e2size(e);
  size_t blocks=divup(size,blocksize);
  size_t buddies=divup(blocks,2);
  return buddies;
}

// Return the bit number of mem's order-e pair: the offset of the pair's
// lower buddy, in units of the pair's size, 2^(e+1).
static size_t bitaddr(void *base, void *mem, int e) {
  size_t addr=baddrclr(base,mem,e)-base;
  size_t blocksize=e2size(e);
  return addr/blocksize/2;
}

extern BBM bbmcreate(size_t size, int e) {
  return bmcreate(mapsize(size,e));
}

extern void bbmdelete(BBM b) {
  bmdelete(b);
}

extern void bbmset(BBM b, void *base, void *mem, int e) {
  bmset(b,bitaddr(base,mem,e));
}

extern void bbmclr(BBM b, void *base, void *mem, int e) {
  bmclr(b,bitaddr(base,mem,e));
}

extern int bbmtst(BBM b, void *base, void *mem, int e) {
  return bmtst(b,bitaddr(base,mem,e));
}

extern void bbmprt(BBM b) { bmprt(b); }

// The upper buddy: set bit e of the offset.
extern void *baddrset(void *base, void *mem, int e) {
  unsigned int mask=1<<e;
  return base+((mem-base)|mask);
}

// The lower buddy: clear bit e of the offset.
extern void *baddrclr(void *base, void *mem, int e) {
  unsigned int mask=~(1<<e);
  return base+((mem-base)&mask);
}

// The buddy: flip bit e of the offset.
extern void *baddrinv(void *base, void *mem, int e) {
  unsigned int mask=1<<e;
  return base+((mem-base)^mask);
}

// Nonzero iff bit e of the offset is set: mem is the upper buddy.
extern int baddrtst(void *base, void *mem, int e) {
  unsigned int mask=1<<e;
  return (mem-base)&mask;
}
