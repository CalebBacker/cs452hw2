// A general-purpose bitmap.
//
// (Provided module; documentation added, code unchanged.)
//
// A BM is an array of bits, numbered from 0, all initially 0.  Its
// memory comes from mmalloc(), so creating one maps (and deleting one
// unmaps) at least a page.

#ifndef BM_H
#define BM_H

#include <stdio.h>

typedef void *BM;

// Create a bitmap of bits bits, all 0; return 0 if mmap() fails.
extern BM   bmcreate(size_t bits);
// Unmap a bitmap.
extern void bmdelete(BM b);

// Set, clear, or test (0 or nonzero) bit i.  An i out of range is a
// fatal error: it is reported to stderr and the program exits.
extern void bmset(BM b, size_t i);
extern void bmclr(BM b, size_t i);
extern int  bmtst(BM b, size_t i);

// Print the bitmap to stdout as hex bytes, highest-numbered byte first,
// so bit 0 is the least significant bit of the rightmost byte.
extern void bmprt(BM b);

#endif
