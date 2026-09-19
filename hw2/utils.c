// utils.c -- small helpers shared by the Buddy System modules.
//
// Author: Caleb Backer
// Class:  CS 452, HW2
// Date:   September 2026
//
// Three groups: mmap()-backed memory (the allocator's only source of
// memory), size arithmetic on powers of two, and single-bit operations
// on a byte.  See utils.h for each function's contract.

#include <sys/mman.h>

#include "utils.h"

// Map size bytes of fresh, zeroed, private memory.  Like mmap(), and
// unlike malloc(), failure returns MMFAILED, not 0: bm.c relies on it.
extern void *mmalloc(size_t size) {
  return mmap(0,size,
              PROT_READ|PROT_WRITE,
              MAP_PRIVATE|MAP_ANONYMOUS,
              -1,0);
}

// Unmap memory from mmalloc(); size must match the mmalloc() request.
extern void mmfree(void *p, size_t size) { munmap(p,size); }

// Return n/d, rounded up: the number of d-sized pieces holding n.
extern size_t divup(size_t n, size_t d) { return n/d+(n%d!=0); }

// Return the number of bytes holding bits bits.
extern size_t bits2bytes(size_t bits) { return divup(bits,bitsperbyte); }

// Return 2^e: the size of a block of "order" e.
extern size_t e2size(int e) { return (size_t)1<<e; }

// Return the smallest e with 2^e >= size: the order of the smallest
// block holding size bytes.  size2e(0) and size2e(1) are 0.
extern int size2e(size_t size) {
  int e=0;
  while (e2size(e)<size)
    e++;
  return e;
}

// Bit operations on the byte at p; bit is 0 (least significant) to 7.
extern void bitset(void *p, int bit) { *(unsigned char *)p |=  (1<<bit); }
extern void bitclr(void *p, int bit) { *(unsigned char *)p &= ~(1<<bit); }
extern void bitinv(void *p, int bit) { *(unsigned char *)p ^=  (1<<bit); }
extern int  bittst(void *p, int bit) { return (*(unsigned char *)p>>bit)&1; }
