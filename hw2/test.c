// test.c -- the test harness, and main(), for the Buddy allocator.
//
// Author: Caleb Backer
// Class:  CS 452, HW2
// Date:   September 2026
//
// Each module has its own tests, in <module>_test.c, run here from the
// bottom of the module stack up: a failure low down explains failures
// above it.  Each test prints pass or FAIL; the exit status is 0 iff
// all pass.  Warnings on stderr are expected only where announced.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "test.h"

static int tests=0, fails=0;

extern void check(int ok, const char *what) {
  tests++;
  if (!ok) fails++;
  printf("  %s %s\n",ok ? "pass" : "FAIL",what);
}

extern void checkstr(const char *got, const char *want, const char *what) {
  int ok=!strcmp(got,want);
  check(ok,what);
  if (!ok)
    printf("    want \"%s\"\n     got \"%s\"\n",want,got);
}

extern void expect(int n) {
  printf("  (%d message%s expected on stderr next:)\n",n,n==1 ? "" : "s");
}

extern int dies(void (*f)(void *arg), void *arg) {
  fflush(0);                    // or the child reprints our buffers
  pid_t pid=fork();
  if (pid<0)
    return 0;
  if (!pid) {
    f(arg);
    _exit(0);                   // f survived
  }
  int status;
  waitpid(pid,&status,0);
  return WIFEXITED(status) && WEXITSTATUS(status)==1;
}

int main() {
  setvbuf(stdout,0,_IOLBF,0);   // interleave with stderr warnings
  test_utils();
  test_bm();
  test_bbm();
  test_freelist();
  test_balloc();
  printf("\n%d tests, %d failures\n",tests,fails);
  return fails ? 1 : 0;
}
