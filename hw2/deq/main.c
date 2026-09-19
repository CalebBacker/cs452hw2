// main.c -- test suite for the Queue<Anon> module.
//
// Author: Caleb Backer
// Class:  CS 452, HW1
// Date:   September 2026
//
// Each test prints pass or FAIL; the exit status is 0 iff all pass.
// The six warnings on stderr are expected: they are the out-of-range
// tests.

#ifndef _GNU_SOURCE          // the course makefile may define it
#define _GNU_SOURCE           // asprintf()
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deq.h"

static int tests=0, fails=0;

static void check(int ok, const char *what) {
  tests++;
  if (!ok) fails++;
  printf("  %s %s\n",ok ? "pass" : "FAIL",what);
}

// Check a queue's head-to-tail contents, and its length, at once.
static void checkq(Deq q, const char *want, const char *what) {
  Str got=deq_str(q,0);
  int n=0; for (const char *p=want; *p; p++) if (*p==' ') n++;
  if (*want) n++;                       // words == spaces+1, unless ""
  int ok = !strcmp(got,want) && deq_len(q)==n;
  tests++;
  if (!ok) {
    fails++;
    printf("  FAIL %s: want \"%s\" (len %d), got \"%s\" (len %d)\n",
           what,want,n,got,deq_len(q));
  } else
    printf("  pass %s: \"%s\"\n",what,got);
  free(got);
}

// a DeqStrF: brackets a datum, in freshly allocated memory
static Str bracket(Data d) {
  char *s; asprintf(&s,"[%s]",(char *)d);
  return s;
}

// a DeqMapF: records map order
static char maplog[256];
static void logger(Data d) { strcat(maplog,(char *)d); }

// a DeqMapF: frees a datum, counting it
static int freed=0;
static void freeing(Data d) { free(d); freed++; }

static void test_new(void) {
  printf("new and empty:\n");
  Deq q=deq_new();
  check(deq_len(q)==0,                  "a new queue is empty");
  checkq(q,"",                          "a new queue prints as nothing");
  check(deq_head_get(q)==0,             "head_get of empty is 0");
  check(deq_tail_get(q)==0,             "tail_get of empty is 0");
  check(deq_head_rem(q,"x")==0,         "head_rem of empty is 0");
  check(deq_tail_rem(q,"x")==0,         "tail_rem of empty is 0");
  printf("  (two warnings expected next:)\n");
  check(deq_head_ith(q,0)==0,           "head_ith of empty is 0, with a warning");
  check(deq_tail_ith(q,0)==0,           "tail_ith of empty is 0, with a warning");
  deq_del(q,0);
}

static void test_put(void) {
  printf("put:\n");
  Deq q=deq_new();
  deq_head_put(q,"b");
  checkq(q,"b",                         "head_put onto empty");
  deq_head_put(q,"a");
  checkq(q,"a b",                       "head_put prepends");
  deq_tail_put(q,"c");
  checkq(q,"a b c",                     "tail_put appends");
  deq_del(q,0);

  q=deq_new();
  deq_tail_put(q,"b");
  checkq(q,"b",                         "tail_put onto empty");
  deq_tail_put(q,"c");
  deq_head_put(q,"a");
  checkq(q,"a b c",                     "puts at both ends interleave");
  deq_del(q,0);

  // the ends build mirror images of each other
  Deq h=deq_new(), t=deq_new();
  static char *w[]={"1","2","3","4"};
  for (int i=0; i<4; i++) {
    deq_head_put(h,w[i]);
    deq_tail_put(t,w[i]);
  }
  checkq(h,"4 3 2 1",                   "head_put reverses");
  checkq(t,"1 2 3 4",                   "tail_put preserves");
  deq_del(h,0);
  deq_del(t,0);
}

// "a b c d e", for the ith/get/rem tests
static Deq abcde(void) {
  Deq q=deq_new();
  static char *w[]={"a","b","c","d","e"};
  for (int i=0; i<5; i++)
    deq_tail_put(q,w[i]);
  return q;
}

static void test_ith(void) {
  printf("ith:\n");
  Deq q=abcde();
  check(!strcmp(deq_head_ith(q,0),"a"),  "head_ith(0) is the head");
  check(!strcmp(deq_head_ith(q,2),"c"),  "head_ith counts from the head");
  check(!strcmp(deq_head_ith(q,4),"e"),  "head_ith(len-1) is the tail");
  check(!strcmp(deq_tail_ith(q,0),"e"),  "tail_ith(0) is the tail");
  check(!strcmp(deq_tail_ith(q,2),"c"),  "tail_ith counts from the tail");
  check(!strcmp(deq_tail_ith(q,4),"a"),  "tail_ith(len-1) is the head");
  printf("  (four warnings expected next:)\n");
  check(deq_head_ith(q,5)==0,            "head_ith past the end is 0");
  check(deq_head_ith(q,-1)==0,           "head_ith of a negative index is 0");
  check(deq_tail_ith(q,5)==0,            "tail_ith past the end is 0");
  check(deq_tail_ith(q,-1)==0,           "tail_ith of a negative index is 0");
  checkq(q,"a b c d e",                  "ith leaves the queue alone");
  deq_del(q,0);
}

static void test_get(void) {
  printf("get:\n");
  Deq q=abcde();
  check(!strcmp(deq_head_get(q),"a"),    "head_get returns the head");
  checkq(q,"b c d e",                    "head_get removes the head");
  check(!strcmp(deq_tail_get(q),"e"),    "tail_get returns the tail");
  checkq(q,"b c d",                      "tail_get removes the tail");
  deq_head_get(q); deq_head_get(q);
  checkq(q,"d",                          "one datum left");
  check(!strcmp(deq_tail_get(q),"d"),    "the last datum comes out either end");
  checkq(q,"",                           "the queue is empty again");
  check(deq_head_get(q)==0,              "head_get of a drained queue is 0");
  deq_head_put(q,"z");
  checkq(q,"z",                          "a drained queue can be refilled");
  deq_del(q,0);
}

static void test_rem(void) {
  printf("rem:\n");
  Deq q=abcde();
  check(!strcmp(deq_head_rem(q,"c"),"c"),"head_rem returns what it found");
  checkq(q,"a b d e",                    "head_rem removes from the middle");
  check(!strcmp(deq_head_rem(q,"a"),"a"),"head_rem finds the head");
  check(!strcmp(deq_tail_rem(q,"e"),"e"),"tail_rem finds the tail");
  checkq(q,"b d",                        "both ends can be remmed away");
  check(deq_head_rem(q,"zz")==0,         "head_rem of an absent datum is 0");
  check(deq_tail_rem(q,"zz")==0,         "tail_rem of an absent datum is 0");
  checkq(q,"b d",                        "a failed rem changes nothing");

  char same[]="b";                       // equal contents, different address
  check(deq_head_rem(q,same)==0,         "rem compares by ==, not by value");
  deq_del(q,0);

  // with one pointer at both ends, the two rems pick opposite occurrences
  char *dup="d";
  q=deq_new();
  deq_tail_put(q,dup); deq_tail_put(q,"x"); deq_tail_put(q,dup);
  deq_head_rem(q,dup);
  check(deq_head_ith(q,0)!=dup && deq_tail_ith(q,0)==dup,
                                         "head_rem takes the occurrence nearest the head");
  deq_head_put(q,dup);
  deq_tail_rem(q,dup);
  check(deq_head_ith(q,0)==dup && deq_tail_ith(q,0)!=dup,
                                         "tail_rem takes the occurrence nearest the tail");
  deq_del(q,0);
}

static void test_map_str(void) {
  printf("map and str:\n");
  Deq q=abcde();

  maplog[0]='\0';
  deq_map(q,logger);
  check(!strcmp(maplog,"abcde"),         "map visits every datum, head to tail");

  Str s=deq_str(q,bracket);
  check(!strcmp(s,"[a] [b] [c] [d] [e]"),"str applies its function");
  free(s);
  checkq(q,"a b c d e",                  "map and str leave the queue alone");

  Deq e=deq_new();
  maplog[0]='x'; maplog[1]='\0';
  deq_map(e,logger);
  check(!strcmp(maplog,"x"),             "map of an empty queue does nothing");
  s=deq_str(e,bracket);
  check(!strcmp(s,""),                   "str of an empty queue is empty");
  free(s);
  deq_del(e,0);
  deq_del(q,0);
}

static void test_del(void) {
  printf("del:\n");
  Deq q=deq_new();
  for (int i=0; i<3; i++) {
    char *d; asprintf(&d,"n%d",i);
    deq_tail_put(q,d);
  }
  checkq(q,"n0 n1 n2",                   "a queue of heap data");
  freed=0;
  deq_del(q,freeing);
  check(freed==3,                        "del maps over every datum before freeing");

  // data taken back out is the caller's; del must not free it again
  q=deq_new();
  char *d; asprintf(&d,"solo");
  deq_head_put(q,d);
  free(deq_head_get(q));
  deq_del(q,freeing);
  check(1,                               "del of an emptied queue is clean");
}

int main() {
  setvbuf(stdout,0,_IOLBF,0);            // interleave with stderr warnings
  test_new();
  test_put();
  test_ith();
  test_get();
  test_rem();
  test_map_str();
  test_del();
  printf("\n%d tests, %d failures\n",tests,fails);
  return fails ? 1 : 0;
}
