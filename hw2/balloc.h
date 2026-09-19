// A Buddy System memory allocator.
//
// A pool manages size bytes (rounded up to a multiple of 2^l) of
// mmap()ed memory, handed out in blocks of 2^e bytes, l <= e <= u.
// See README for the design and error handling.

#ifndef BALLOC_H
#define BALLOC_H

typedef void *Balloc;

// Create a pool of size bytes, whose smallest block is 2^l bytes and
// largest is 2^u.  2^l must hold a pointer (l >= 3 on a 64-bit
// machine) and l <= u <= 30.  A size that is not a power of two, or is
// larger than 2^u, is tiled with the largest blocks that fit.  Returns
// 0, with a warning, for bad arguments or a failed mmap().
extern Balloc bcreate(unsigned int size, int l, int u);
// Unmap a pool, and everything allocated from it.  Deleting 0 is a no-op.
extern void   bdelete(Balloc pool);

// Return a block of at least size bytes (at least 2^l), or 0 if size
// exceeds 2^u or the pool has no block large enough.
extern void *balloc(Balloc pool, unsigned int size);
// Return a block to its pool.  Freeing 0 is a no-op; freeing anything
// that is not an allocated block of pool warns and does nothing.
extern void  bfree(Balloc pool, void *mem);

// Return the size of the block (not the request) at mem, or 0, with a
// warning, if mem is not an allocated block of pool.
extern unsigned int bsize(Balloc pool, void *mem);
// Print a pool's blocks, free lists and bitmaps to stdout.
extern void bprint(Balloc pool);

#endif
