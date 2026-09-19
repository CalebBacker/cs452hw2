// Small helpers shared by the Buddy System modules: mmap()-backed
// memory, power-of-two size arithmetic, and bit operations on a byte.

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

static const int bitsperbyte=8;

// mmalloc() returns this on failure, as mmap() does (MAP_FAILED).
#define MMFAILED ((void *)-1)

// Map (unmap) size bytes of fresh, zeroed memory via mmap() (munmap()).
// These are the allocator's only source of memory: never malloc().
extern void *mmalloc(size_t size);
extern void mmfree(void *p, size_t size);

// n/d rounded up; the number of bytes holding bits bits.
extern size_t divup(size_t n, size_t d);
extern size_t bits2bytes(size_t bits);

// A block of order e holds 2^e bytes.  e2size(e) is 2^e; size2e(size)
// is the smallest e with 2^e >= size (the order a request needs).
extern size_t e2size(int e);
extern int size2e(size_t size);

// Set, clear, invert, or test (0 or 1) bit number bit (0..7, 0 is least
// significant) of the byte at p.
extern void bitset(void *p, int bit);
extern void bitclr(void *p, int bit);
extern void bitinv(void *p, int bit);
extern int  bittst(void *p, int bit);

#endif
