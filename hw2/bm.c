// bm.c -- a general-purpose bitmap.
//
// (Provided module; documentation added, code unchanged.)
//
// Layout: one mmalloc()ed region holding a size_t, the number of bits,
// followed by the bits themselves, 8 per byte.  A BM points just past
// the size_t, at the bits; the count is found by stepping back one
// size_t.  Bit i lives in byte i/8, at position i%8 within it.
//
// Note: the pointer arithmetic on void * (b+i/bitsperbyte) is a GNU C
// extension, treating void * like char *.  And bmprt() passes a plain
// char to %02x: where char is signed (x86), a byte of 0x80 or more
// prints sign-extended, as ffffff80.

#include <stdlib.h>
#include <string.h>

#include "bm.h"
#include "utils.h"

// Return the number of bits, stored just before the bitmap.
static size_t bmbits(BM b) { size_t *bits=b; return *--bits; }

// Return the number of bytes holding the bits.
static size_t bmbytes(BM b) { return bits2bytes(bmbits(b)); }

// Exit, with a message, unless bit i is in range.
static void ok(BM b, size_t i) {
  if (i<bmbits(b))
    return;
  fprintf(stderr,"bitmap index out of range\n");
  exit(1);
}         

// mmalloc() returns -1, like mmap(), on failure.  mmap()ed memory is
// already zeroed, but the memset() makes that explicit.
extern BM bmcreate(size_t bits) {
  size_t bytes=bits2bytes(bits);
  size_t *p=mmalloc(sizeof(size_t)+bytes);
  if ((long)p==-1)
    return 0;
  *p=bits;
  BM b=++p;
  memset(b,0,bytes);
  return b;
}

// Step back to the size_t, to unmap the whole region.
extern void bmdelete(BM b) {
  size_t *p=b;
  p--;
  mmfree(p,sizeof(size_t)+bits2bytes(*p));
}

// Each operation checks i, then acts on bit i%8 of byte i/8.
extern void bmset(BM b, size_t i) {
  ok(b,i); bitset(b+i/bitsperbyte,i%bitsperbyte);
}

extern void bmclr(BM b, size_t i) {
  ok(b,i); bitclr(b+i/bitsperbyte,i%bitsperbyte);
}

extern int bmtst(BM b, size_t i) {
  ok(b,i); return bittst(b+i/bitsperbyte,i%bitsperbyte);
}

// Bytes are printed from the highest down, separated by spaces.
extern void bmprt(BM b) {
  for (int byte=bmbytes(b)-1; byte>=0; byte--)
    printf("%02x%s",((char *)b)[byte],(byte ? " " : "\n"));
}
