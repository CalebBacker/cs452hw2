# Handoff note for Claude on the Windows PC

**Delete this file when you've read it.** `git rm HANDOFF.md && git commit -m "Remove handoff note"`.
It exists only to carry context from the Mac session that wrote this code.

## What this repo is

CS 452 (Fall 2026) HW2: a Buddy System memory allocator in C. Graded on
**onyx** (Linux, gcc, valgrind). The assignment is `hw.pdf`; the grading
breakdown is `rubric`.

Layout mirrors HW1 (`github.com/CalebBacker/CS452_hw1`): the course
makefile sits at the repo root, and `hw2/GNUmakefile` includes
`../GNUmakefile`. **Keep that parent/child structure when copying to
onyx**, or nothing builds.

## State: code complete, not yet verified on onyx

Written and passing on macOS/clang: **93 tests, 0 failures**, also clean
under AddressSanitizer and UBSan. Nothing has ever been compiled with
real gcc or run under valgrind, because the Mac had neither.

### Still to do
1. **Run on onyx** (see commands below) and fix anything gcc or valgrind
   reports.
2. **Save `test.out` and `valgrind.out`** from those runs and commit
   them, as HW1 did.
3. **Write the AI Use statement** at the bottom of `hw2/README`. It's a
   TODO placeholder. Claude wrote this implementation; Caleb writes that
   statement himself.

### Commands on onyx
```sh
cd hw2
make clean; make        # watch for gcc warnings; there were none on clang
make run                # expect "93 tests, 0 failures"; exit 0
make valgrind           # expect 0 errors, no leaks
cd deq; make clean; make run    # expect "54 tests, 0 failures"
```

**Run `./deq` with stdout on a terminal, never redirected.** glibc
allocates stdout's buffer from the wrapper's 4096-byte pool, sized by
where stdout goes: ~1024 bytes for a terminal, but 4096 for a file or
pipe, which is the entire pool, so the deque's first malloc fails. Use
`script -q -c ./deq deq.out` if you need to capture it.

## Design, in brief

Modules stack: `utils` → `bm` → `bbm` → `freelist` → `balloc`.

- `bm.[hc]` and `bbm.[hc]` were **provided by the instructor**. The
  assignment says document them but don't change them. Comments were
  added; the code is byte-identical apart from comments. **Don't
  "improve" them.**
- `freelist.c` is the algorithm: one free list per order, the link
  stored in the first word of each free block (so allocated blocks carry
  no header, as the spec demands), plus two buddy-pair bitmaps per
  order:
  - `free`: flipped whenever either buddy enters or leaves the list, so
    it's 1 iff exactly one buddy is on it. A block being freed isn't on
    the list, so the bit says whether its buddy is: the merge decision,
    no search.
  - `alloc`: set when either buddy is allocated at that order, cleared
    when the pair merges. A block's order is the smallest order whose
    alloc bit covers it, since every pair inside an allocated block has
    both bits 0. This is also how bad pointers are rejected.
- `freelist.h`'s interface **differs from the handout's** on purpose:
  the FreeList stores base, l and u. The handout's `freelistalloc` isn't
  passed `u`, so it can't know when to stop looking for a bigger block.
  The spec explicitly allows changing it; `hw2/README` explains why.
- A freed block whose buddy isn't free leaves the pair's alloc bit set.
  If the buddy was split, that bit is stale but harmless. `GUIDE.md`
  §5 has the full argument. Don't "fix" it without reading that.

`GUIDE.md` is a study guide for Caleb, not part of the submission. It
teaches the whole thing from scratch. Don't reference it from files
inside `hw2/`, since that directory is what gets submitted.

## Conventions to follow

Match the existing style: the instructor's dense C (`e2size(e)`, no
spaces around `=`, short names), comments that explain why rather than
what, and HW1's `WARN`/`ERROR` macros from `error.h`. Keep the README in
HW1's plain-text sections (Files, Building, Design, Semantics, Errors,
Testing, AI Use). No fancy extras: Caleb explicitly asked that the
submission do exactly what the assignment asks and nothing more.

## Unrelated, in case it comes up

The Mac can't reach the school VPN (GlobalProtect). Login succeeds, then
the portal returns an empty config ("Invalid portal"). It's a
server-side problem: the same account connects from this Windows PC, and
reinstalling, certificates, and the system extension were all ruled out.
It's with the help desk. Not a code issue, and the VPN may not even be
needed for onyx.
