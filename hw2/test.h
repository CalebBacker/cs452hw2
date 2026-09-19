// The test harness, and each module's tests.

#ifndef TEST_H
#define TEST_H

// Record and print one test: pass iff ok.
extern void check(int ok, const char *what);

// Pass iff got and want are equal strings; print both if not.
extern void checkstr(const char *got, const char *want, const char *what);

// Announce n warnings or errors about to appear on stderr, on purpose.
extern void expect(int n);

// Return 1 iff f(arg), run in a child process, exits with status 1:
// how a fatal error is tested without ending the test suite.
extern int dies(void (*f)(void *arg), void *arg);

extern void test_utils(void);
extern void test_bm(void);
extern void test_bbm(void);
extern void test_freelist(void);
extern void test_balloc(void);

#endif
