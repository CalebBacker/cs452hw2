// The Buddy System's free lists: one list of free blocks per order e,
// l <= e <= u, plus two buddy-pair bitmaps per order.  This module is
// the Buddy algorithm itself (splitting, coalescing, and finding a
// block's order); balloc.c is the public interface on top of it.
//
// The pool is size bytes at base, obtained by the caller.  A block of
// order e holds 2^e bytes and begins at an offset (from base) that is
// a multiple of 2^e.
//
// This interface differs from the one handed out: the FreeList records
// base, l and u itself, so callers cannot pass inconsistent values.

#ifndef FREELIST_H
#define FREELIST_H

#include <stdio.h>

typedef void *FreeList;

// Build the lists for the pool, and fill them with the largest aligned
// blocks (of order <= u) that tile it.  size must be a multiple of 2^l,
// and 2^l must hold a pointer.  Returns 0 if mmalloc() fails.
extern FreeList freelistcreate(void *base, size_t size, int l, int u);
extern void     freelistdelete(FreeList f);

// Allocate a block of order e (l <= e <= u), splitting a larger block
// if need be; return 0 if none is available.
extern void *freelistalloc(FreeList f, int e);

// Free mem, an allocated block of order e (from freelistsize()),
// merging it with its buddy, repeatedly, while the buddy is free.
extern void  freelistfree(FreeList f, void *mem, int e);

// Return the order of the allocated block beginning at mem, or -1 if
// mem is not the beginning of an allocated block.
extern int freelistsize(FreeList f, void *mem);

// Print the lists and bitmaps to stdout, a debugging aid.
extern void freelistprint(FreeList f);

// The to-string: write a map of the pool into buf (at most len bytes,
// always terminated), one "offset:A|Fsize" entry per block, in address
// order, e.g. "0:A16 16:F16 32:F32".  Returns buf.
extern char *freeliststr(FreeList f, char *buf, size_t len);

#endif
