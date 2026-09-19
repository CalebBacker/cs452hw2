// balloc_test.c -- tests for the allocator's public interface.
//
// Author: Caleb Backer
// Class:  CS 452, HW2
// Date:   September 2026
//
// A Balloc is opaque, so these tests observe it only through the
// interface: addresses, sizes, what fits, and what memory holds.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "balloc.h"

static void test_create(void) {
  printf("bcreate and bdelete:\n");
  expect(5);
  check(!bcreate(0,4,12),    "a size of 0 is refused");
  check(!bcreate(4096,2,12), "an l too small to hold a pointer is refused");
  check(!bcreate(4096,5,4),  "a u below l is refused");
  check(!bcreate(4096,4,31), "a u above 30 is refused");
  check(!bcreate(4096,-1,4), "a negative l is refused");
  Balloc p=bcreate(4096,4,12);
  check(p!=0,"a 4096-byte pool, of 16- to 4096-byte blocks");
  bdelete(p);
  bdelete(0);
  check(1,"bdelete, and bdelete(0)");
  p=bcreate(100,4,4);
  int n=0;
  while (balloc(p,1))
    n++;
  check(n==7,"a pool of 100 is rounded up to whole 16s: seven of them");
  bdelete(p);
}

static void test_sizes(void) {
  printf("balloc and bsize:\n");
  Balloc p=bcreate(4096,4,12);
  void *m0=balloc(p,0), *m1=balloc(p,1), *m16=balloc(p,16),
       *m17=balloc(p,17), *m100=balloc(p,100), *m1000=balloc(p,1000);
  check(bsize(p,m0)==16 && bsize(p,m1)==16 && bsize(p,m16)==16,
        "requests up to 2^l get 2^l bytes");
  check(bsize(p,m17)==32 && bsize(p,m100)==128 && bsize(p,m1000)==1024,
        "larger requests get the next power of two");
  check(!balloc(p,4097),"a request larger than 2^u fails");
  check(!balloc(p,4096),"so does one no free block can hold");
  void *all[]={m0,m1,m16,m17,m100,m1000};
  int ok=1;
  for (int i=0; i<6; i++) {
    ok&=(uintptr_t)all[i]%bsize(p,all[i])==0;
    memset(all[i],i+1,bsize(p,all[i]));
  }
  check(ok,"each block is aligned to its size");
  for (int i=0; i<6; i++)
    for (unsigned j=0; j<bsize(p,all[i]); j++)
      ok&=((unsigned char *)all[i])[j]==i+1;
  check(ok,"blocks are writable, and do not overlap");
  for (int i=5; i>=0; i--)
    bfree(p,all[i]);
  void *big=balloc(p,4096);
  check(big==m0,"freeing everything restores a single 4096-byte block");
  bfree(p,big);
  bdelete(p);
}

static void test_exhaust(void) {
  printf("balloc exhaustion and fragmentation:\n");
  Balloc p=bcreate(4096,4,12);
  static void *m[256];
  int ok=1;
  for (int i=0; i<256; i++)
    ok&=(m[i]=balloc(p,16))!=0;
  check(ok,"256 16-byte blocks fill a 4096-byte pool");
  check(!balloc(p,1),"and then even 1 byte fails");
  for (int i=0; i<256; i+=2)
    bfree(p,m[i]);
  check(!balloc(p,17),
        "freeing every other one frees half, but no two buddies");
  for (int i=1; i<256; i+=2)
    bfree(p,m[i]);
  check(balloc(p,4096)!=0,"freeing the rest merges them all");
  bdelete(p);
}

static void test_odd(void) {
  printf("pools that are not a power of two:\n");
  Balloc p=bcreate(6000,4,12);  // 4096+1024+512+256+64+32+16
  unsigned sizes[]={4096,1024,512,256,64,32,16};
  int ok=1;
  for (int i=0; i<7; i++)
    ok&=bsize(p,balloc(p,sizes[i]))==sizes[i];
  check(ok,"6000 bytes are tiled as 4096+1024+512+256+64+32+16");
  check(!balloc(p,1),"and all of them are usable, and nothing more");
  bdelete(p);

  p=bcreate(3*4096,4,12);
  ok=balloc(p,4096) && balloc(p,4096) && balloc(p,4096);
  check(ok && !balloc(p,4096),"a pool of three 2^u blocks holds three");
  bdelete(p);

  p=bcreate(1000,4,12);         // 1008: 512+256+128+64+32+16
  check(!balloc(p,1000) && balloc(p,500),
        "a 1000-byte pool has no 1024-byte block, but has a 512");
  bdelete(p);
}

static void test_errors(void) {
  printf("misuse:\n");
  Balloc p=bcreate(4096,4,12), q=bcreate(4096,4,12);
  void *a=balloc(p,64), *b=balloc(p,64), *c=balloc(q,64);
  int local;
  bfree(p,0);
  check(1,"bfree of 0 is a quiet no-op");
  expect(8);
  bfree(p,a+16);                // the middle of a block
  bfree(p,&local);              // not from the pool
  bfree(p,c);                   // from another pool
  bfree(p,b);
  bfree(p,b);                   // twice
  check(bsize(p,b)==0,"bsize of a freed block is 0");
  check(bsize(p,a+1)==0,"bsize of a block's middle is 0");
  check(!balloc(0,16),"balloc of a zero pool is 0");
  bfree(0,a);
  check(bsize(p,a)==64,"after all that, the pool is intact");
  bfree(p,a);
  check(balloc(p,4096)!=0,"and it still merges completely");
  bfree(q,c);
  check(balloc(q,4096)!=0,"pools are independent");
  bdelete(p);
  bdelete(q);
}

// Random allocations and frees, checking that no block is disturbed
// while it is allocated, and that everything merges at the end.
static void test_stress(void) {
  printf("stress:\n");
  enum { Max=500, Ops=50000 };
  static unsigned char *m[Max];
  static unsigned want[Max];
  Balloc p=bcreate(1<<16,4,16);
  srand(452);
  int n=0, ok=1, fits=1, allocs=0, fulls=0;
  for (int op=0; op<Ops; op++) {
    if (n<Max && rand()%2) {
      unsigned size=1+rand()%(rand()%8 ? 256 : 8192);
      unsigned char *b=balloc(p,size);
      if (!b) {
        fulls++;
        continue;
      }
      allocs++;
      fits&=bsize(p,b)>=size && bsize(p,b)<2*size+16;
      memset(b,n,size);
      m[n]=b;
      want[n++]=size;
    } else if (n) {
      int i=rand()%n;
      for (unsigned j=0; j<want[i]; j++)
        ok&=m[i][j]==(unsigned char)i;
      bfree(p,m[i]);
      n--;
      if (i!=n) {               // move the last block into slot i
        m[i]=m[n];
        want[i]=want[n];
        memset(m[i],i,want[i]);
      }
    }
  }
  printf("  (%d allocations, %d refused for lack of space)\n",allocs,fulls);
  check(fits,"every block fits its request, in the next power of two");
  check(ok,"no block was disturbed while allocated");
  while (n)
    bfree(p,m[--n]);
  check(balloc(p,1<<16)!=0,"freeing everything merges the whole pool");
  bdelete(p);
}

static void test_print(void) {
  printf("bprint, after allocating 16, 64 and 16 from a 256-byte pool:\n");
  Balloc p=bcreate(256,4,8);
  balloc(p,16);
  balloc(p,64);
  balloc(p,16);
  bprint(p);
  bdelete(p);
}

extern void test_balloc(void) {
  test_create();
  test_sizes();
  test_exhaust();
  test_odd();
  test_errors();
  test_stress();
  test_print();
}
