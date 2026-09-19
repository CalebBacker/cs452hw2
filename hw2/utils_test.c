// utils_test.c -- tests for utils, and for the provided bm and bbm.
//
// Author: Caleb Backer
// Class:  CS 452, HW2
// Date:   September 2026

#include <string.h>

#include "test.h"
#include "utils.h"
#include "bm.h"
#include "bbm.h"

extern void test_utils(void) {
  printf("utils:\n");
  check(divup(0,4)==0 && divup(1,4)==1 && divup(4,4)==1 && divup(5,4)==2,
        "divup rounds up");
  check(bits2bytes(0)==0 && bits2bytes(1)==1 && bits2bytes(8)==1 &&
        bits2bytes(9)==2,
        "bits2bytes rounds up to whole bytes");
  check(e2size(0)==1 && e2size(4)==16 && e2size(30)==1u<<30,
        "e2size is 2^e");
  check(size2e(0)==0 && size2e(1)==0 && size2e(2)==1 && size2e(3)==2 &&
        size2e(16)==4 && size2e(17)==5,
        "size2e is the smallest order that fits");
  int ok=1;
  for (int e=2; e<32; e++)
    ok&=size2e(e2size(e))==e && size2e(e2size(e)-1)==e &&
        size2e(e2size(e)+1)==e+1;
  check(ok,"size2e inverts e2size, and rounds up around it");

  unsigned char byte=0;
  bitset(&byte,0); bitset(&byte,7);
  check(byte==0x81,"bitset sets the named bits");
  check(bittst(&byte,0)==1 && bittst(&byte,7)==1 && bittst(&byte,3)==0,
        "bittst is 0 or 1");
  bitclr(&byte,0);
  check(byte==0x80,"bitclr clears only the named bit");
  bitinv(&byte,7); bitinv(&byte,1);
  check(byte==0x02,"bitinv flips the named bits");

  size_t n=10000;
  unsigned char *p=mmalloc(n);
  check(p!=MMFAILED,"mmalloc maps memory");
  int zero=1;
  for (size_t i=0; i<n; i++)
    zero&=!p[i];
  check(zero,"mmalloc'd memory is zeroed");
  memset(p,0xab,n);
  check(p[0]==0xab && p[n-1]==0xab,"all of it is writable");
  mmfree(p,n);
  check(mmalloc((size_t)-1)==MMFAILED,"an impossible mmalloc is MMFAILED");
}

static void bmpast(void *b) { bmset(b,20); }

extern void test_bm(void) {
  printf("bm:\n");
  BM b=bmcreate(20);
  check(b!=0,"bmcreate makes a bitmap");
  int zero=1;
  for (int i=0; i<20; i++)
    zero&=!bmtst(b,i);
  check(zero,"a new bitmap is all 0");
  bmset(b,0); bmset(b,9); bmset(b,19);
  check(bmtst(b,0) && bmtst(b,9) && bmtst(b,19) &&
        !bmtst(b,1) && !bmtst(b,8) && !bmtst(b,10) && !bmtst(b,18),
        "bmset sets only the named bits, across bytes");
  printf("  bmprt, bits 0, 9 and 19 set (expect 08 02 01): ");
  bmprt(b);
  bmclr(b,9);
  check(!bmtst(b,9) && bmtst(b,0) && bmtst(b,19),
        "bmclr clears only the named bit");
  expect(1);
  check(dies(bmpast,b),"an index past the end is fatal");
  bmdelete(b);
}

extern void test_bbm(void) {
  printf("bbm:\n");
  char pool[256];
  void *base=pool;
  check(baddrinv(base,base+0,4)==base+16 && baddrinv(base,base+16,4)==base,
        "order-4 buddies are 16 bytes apart, and each other's buddy");
  check(baddrinv(base,base+64,5)==base+96 && baddrinv(base,base+128,7)==base,
        "higher orders' buddies are 2^e apart");
  check(baddrclr(base,base+48,4)==base+32 && baddrset(base,base+32,4)==base+48,
        "baddrclr gives the lower buddy; baddrset gives the upper");
  check(baddrclr(base,base+32,4)==base+32 && baddrset(base,base+48,4)==base+48,
        "the lower buddy's clr, and the upper buddy's set, are itself");
  check(!baddrtst(base,base+32,4) && baddrtst(base,base+48,4),
        "baddrtst is nonzero only for the upper buddy");
  void *odd=pool+3;
  check(baddrinv(odd,odd+16,4)==odd,
        "addresses are relative to base, which need not be aligned");

  BBM m=bbmcreate(256,4);       // 16 blocks: 8 pairs
  bbmset(m,base,base+16,4);
  check(bbmtst(m,base,base+0,4) && bbmtst(m,base,base+16,4),
        "buddies share a bit");
  check(bbmtst(m,base,base+8,4) && bbmtst(m,base,base+31,4),
        "any address inside either buddy finds the pair's bit");
  check(!bbmtst(m,base,base+32,4) && !bbmtst(m,base,base+240,4),
        "other pairs are independent");
  bbmset(m,base,base+192,4);
  printf("  bbmprt, pairs 0 and 6 set (expect 41): ");
  bbmprt(m);
  bbmclr(m,base,base+0,4);
  check(!bbmtst(m,base,base+16,4) && bbmtst(m,base,base+192,4),
        "bbmclr clears only the pair");
  bbmdelete(m);

  m=bbmcreate(112,4);           // 7 blocks: 4 pairs, the last unpaired
  bbmset(m,base,base+96,4);
  check(bbmtst(m,base,base+96,4),
        "a pool of an odd number of blocks has a bit for the last one");
  bbmdelete(m);
}
