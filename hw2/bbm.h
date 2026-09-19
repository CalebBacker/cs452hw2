// A baddr bitmap, for the Buddy System.
//
// (Provided module; documentation added, code unchanged.)
//
// Buddy addresses: in a pool at base, a block of order e (2^e bytes)
// begins at an offset, mem-base, that is a multiple of 2^e.  Its buddy
// is the other half of the order-(e+1) block containing it, and their
// offsets differ in exactly bit e: the lower buddy has bit e clear, the
// upper buddy has it set.
//
// A BBM is a bitmap with one bit per buddy PAIR of order e: the pair
// holding mem has bit number (mem-base)/2^(e+1).  Both buddies share
// the bit, and mem may be any address inside either buddy.  The bit's
// meaning is up to the caller.  The masks are unsigned int, built with
// int shifts, so offsets must stay below 2^32 and e below 31.

#ifndef BBM_H
#define BBM_H

#include <stdio.h>

typedef void *BBM;

// Create a bitmap with a bit for every order-e buddy pair in a pool of
// size bytes, all 0; return 0 if mmap() fails.  Unmap it.
extern BBM  bbmcreate(size_t size, int e);
extern void bbmdelete(BBM b);

// Set, clear, or test the bit for mem's order-e buddy pair.
extern void bbmset(BBM b, void *base, void *mem, int e);
extern void bbmclr(BBM b, void *base, void *mem, int e);
extern  int bbmtst(BBM b, void *base, void *mem, int e);

// Print the bitmap, as bmprt() does.
extern void bbmprt(BBM b);

// Bit e of mem's offset: set it (the upper buddy), clear it (the lower
// buddy, and the start of the order-(e+1) block holding both), invert
// it (mem's buddy), or test it (nonzero iff mem is the upper buddy).
extern void *baddrset(void *base, void *mem, int e);
extern void *baddrclr(void *base, void *mem, int e);
extern void *baddrinv(void *base, void *mem, int e);
extern int   baddrtst(void *base, void *mem, int e);

#endif
