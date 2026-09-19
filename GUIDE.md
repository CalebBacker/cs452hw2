# HW2 Guide: the Buddy System, and how this allocator is built

This is a study guide for HW2 (the code is in `hw2/`). It explains the
ideas first, then how each module uses them, then how the code is tested.
Every memory map in it is real output from the test suite.

Sections 1–3 are the background: what an allocator has to do, how the
Buddy System does it, and the binary arithmetic that makes it fast. The
rest of the guide is about this particular implementation.

---

## 1. What an allocator does, and why it's hard

### 1.1 The job

A running program keeps asking for memory in pieces: 24 bytes for a
list node, 100 for a string, 4000 for a buffer. It gives each piece back
when it's done, in whatever order it likes.

To the allocator, its memory is one big array of bytes, called the
**pool**. Its whole job is bookkeeping: remembering which ranges of that
array are in use.

- `balloc(p, n)` finds an unused range of at least `n` bytes, marks it
  used, and returns a pointer to its start.
- `bfree(p, m)` marks the range starting at `m` unused again.

That sounds easy, but three facts make it hard:

1. **A block can never move once it's handed out.** The caller holds
   pointers to it, maybe in many places, and the allocator can't find or
   update them. (Languages with a compacting garbage collector *can*
   move objects, because they know where every pointer is. C can't.)
2. **Requests and frees come in any order, at any size.** The allocator
   can't plan ahead. It has to decide where each block goes the moment
   it's asked.
3. **It has to be fast and cheap.** `malloc` may be called millions of
   times. Slow searches, or big bookkeeping per block, add up.

### 1.2 Fragmentation

Facts 1 and 2 together lead to **fragmentation**: memory that is free
but can't be used. There are two kinds.

**External fragmentation:** there's enough free memory in total, but
it's scattered in pieces that are each too small. Take a 100-byte pool,
allocate ten 10-byte blocks, then free every other one:

```
offset  0    10   20   30   40   50   60   70   80   90   100
        | A  |    | A  |    | A  |    | A  |    | A  |    |
                (A = allocated, blank = free)
```

50 bytes are free, but a 20-byte request fails: there's no 20-byte gap.
And since nothing can move (fact 1), the gaps can't be squeezed together.

**Internal fragmentation:** the allocator hands out *more* than was
asked for, and the extra is wasted inside the block. If every block must
be a power of two, a 65-byte request gets 128 bytes and 63 of them do
nothing.

Every allocator design is a trade-off between these two kinds of waste,
speed, and how much bookkeeping it needs.

### 1.3 A few designs, and where the Buddy System fits

| design | how it works | good | bad |
|---|---|---|---|
| **bump** | a pointer moves forward through the pool | fastest possible | can't free a single block |
| **fixed-size blocks** | every block is the same size; free blocks sit on a list | fast, no external fragmentation | terrible for mixed sizes |
| **general free list** (first fit, best fit) | any size; split a free gap to fit; on free, merge with free neighbors | little waste | merging must find a block's neighbors and their sizes, usually through a header in *every* block, allocated ones included |
| **Buddy System** | sizes are powers of two; blocks come only from halving | fast split and merge, tiny bookkeeping, no headers | internal fragmentation |

The Buddy System gives up some memory (internal fragmentation) in
exchange for speed and simplicity. Every block has exactly one possible
merge partner, and a little arithmetic finds it. No searching, and no
headers.

It's old (Knowlton, 1965; Knuth covers it in *The Art of Computer
Programming*, vol. 1, §2.5) and still in use. The Linux kernel
allocates physical memory pages with it (the reading in the spec, from
Gorman, is about exactly this). Linux's `kmalloc` then carves those
pages into small objects.

### 1.4 Our interface

```c
Balloc p = bcreate(4096, 4, 12);   // a 4096-byte pool; blocks 2^4..2^12 bytes
void *m = balloc(p, 100);          // get a 128-byte block
bsize(p, m);                       // 128: the block's size, not the request's
bfree(p, m);                       // give it back
bdelete(p);                        // unmap the whole pool
```

The three `bcreate` arguments:

- **`size`**: how many bytes the pool holds in total.
- **`l`** (lower): the smallest block is `2^l` bytes. With `l = 4`, even
  `balloc(p, 1)` gets 16 bytes. Smaller blocks would mean more
  bookkeeping, and (section 4) a free block has to be big enough to hold
  a pointer.
- **`u`** (upper): the largest block is `2^u` bytes. With `u = 12`,
  `balloc(p, 5000)` fails, returning 0.

`bsize` reports the **block's** size, not the request's: after
`balloc(p, 100)` it says 128. The wrapper's `realloc` relies on this: it
copies `bsize` bytes, which is safe because the block really is that big.

An application can have **several pools**, each with its own `size`,
`l` and `u`. This is realistic: a kernel may manage regions of memory
with different properties (speeds, or which devices can reach them) as
separate pools.

### 1.5 Where the memory comes from: `mmap`

The pool's memory comes from `mmap()`, which asks the kernel for fresh
pages directly:

```c
mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
```

- `0`: let the kernel choose the address.
- `PROT_READ|PROT_WRITE`: we can read and write it.
- `MAP_ANONYMOUS`, with fd `-1`: plain memory, not a view of a file.
- `MAP_PRIVATE`: not shared with other processes.

What comes back is **page-aligned** (its address is a multiple of the
page size: 4096 bytes on most Linux machines, 16384 on Apple Silicon)
and **filled with zeros**. The kernel always hands out whole pages, so
even a 100-byte request really takes a page. `utils.c` wraps this call
as `mmalloc`, and `munmap` as `mmfree`.

We **can't use `malloc()`**, for two reasons:

1. An allocator built on another allocator only moves the problem somewhere else.
2. In the last step of the assignment, `wrapper.c` *replaces* `malloc()`
   with our allocator. If we called `malloc()`, we'd be calling
   ourselves, forever.

The spec also says `mmap` is called **only in `bcreate`**. Every piece of
memory the allocator will ever use, bookkeeping included, is mapped when
the pool is made. After that, `balloc` and `bfree` just rearrange what's
already there. That's how a kernel allocator behaves: there's a fixed
amount of physical memory, and it can't ask anyone for more. So a pool
never grows; when it's full, `balloc` returns 0.

---

## 2. The Buddy System, in depth

### 2.1 Two rules

The whole system follows from two restrictions:

1. **Sizes are powers of two.** Every block is `2^e` bytes for some `e`
   between `l` and `u`. A request is rounded up to the next power of two.
2. **Blocks come only from halving.** The only way to make a smaller
   block is to cut a bigger one exactly in half. The only way to make a
   bigger block is to glue those two halves back together.

The rest of this section shows what these two rules buy you.

### 2.2 Orders, and rounding a request

A block of `2^e` bytes is said to have **order e**. For the examples
here, the pool is 256 bytes with `l = 4` and `u = 8`:

| order e | block size 2^e |
|---|---|
| 4 (= l) | 16 |
| 5 | 32 |
| 6 | 64 |
| 7 | 128 |
| 8 (= u) | 256 |

A request for `n` bytes needs the smallest order whose block holds `n`:
the smallest `e` with `2^e ≥ n`. That's `size2e(n)` in `utils.c`. Then
it's raised to at least `l`:

| request | order | block | wasted |
|---|---|---|---|
| 1 | 4 (raised from 0) | 16 | 15 |
| 16 | 4 | 16 | 0 |
| 17 | 5 | 32 | 15 |
| 100 | 7 | 128 | 28 |
| 129 | 8 | 256 | 127 |
| 257 | 9 > u | fails | – |

The "wasted" column is internal fragmentation (1.2). A request just
over a power of two wastes almost half its block.

### 2.3 The pool is a binary tree

Picture every block that could *ever* exist in the 256-byte pool. Rule 2
says they all come from halving, so they form a binary tree:

```
order 8                         [0,256)
                      ┌────────────┴────────────┐
order 7           [0,128)                    [128,256)
               ┌─────┴─────┐              ┌─────┴─────┐
order 6     [0,64)      [64,128)      [128,192)   [192,256)
            ┌─┴─┐        ┌─┴─┐          ┌─┴─┐       ┌─┴─┐
order 5    0   32       64   96       128  160    192  224
order 4   0 16 32 48  64 80 96 112 128 144 ...                (16 leaves)
```

(`[a,b)` means bytes `a` up to, but not including, `b`. Lower rows show
only each block's starting offset.)

Every block is a **node** of this tree. At any moment, the pool is
covered by a set of nodes that don't overlap and leave no gaps. Each of
those nodes is either **allocated** or **free**. Every node above them
has been **split**.

- **Splitting** a block replaces a node by its two children.
- **Merging** replaces two children by their parent.

Here's the pool after one 16-byte allocation. The 256-byte root was split
four times to get down to a 16-byte block:

```
[0,256)                  split
 ├─ [0,128)              split
 │   ├─ [0,64)           split
 │   │   ├─ [0,32)       split
 │   │   │   ├─ [0,16)   ALLOCATED   ← returned to the caller
 │   │   │   └─ [16,32)  free
 │   │   └─ [32,64)      free
 │   └─ [64,128)         free
 └─ [128,256)            free
```

Two children of the same node are **buddies**. Every block except the
root has exactly one buddy: its sibling in the tree. `[0,16)` and
`[16,32)` are buddies. So are `[0,64)` and `[64,128)`.

**Alignment follows for free.** Halving `[0,256)` gives children that
start at 0 and 128. Halving those gives 0, 64, 128, 192. At every level,
a block of order e starts at a multiple of `2^e`. You never have to
enforce alignment; it's a consequence of rule 2. Section 3 shows why this
matters so much.

### 2.4 Buddies are not just neighbors

This point is easy to get wrong. Two blocks that are **the same size and
next to each other are not necessarily buddies**.

`[16,32)` and `[32,48)` are both order 4 and they touch. But their
parents are different: `[0,32)` and `[32,64)`. If we glued them together,
we'd get `[16,48)`: a 32-byte block starting at 16, which isn't a
multiple of 32. That block isn't a node in the tree. The pool would stop
being a tree, and rule 2 would break.

So a block merges **only with its buddy**, and never with any other
neighbor. That's also why the partner is always exactly one block: the
allocator never has to choose, or search.

### 2.5 Free lists

The allocator keeps **one free list per order**. List e holds every free
block of order e. So finding a free 32-byte block isn't a search through
the pool: it's "take the first block off list 5."

Right after the allocation in 2.3, the lists are:

```
order 8: (empty)
order 7: 128
order 6: 64
order 5: 32
order 4: 16
```

### 2.6 Allocating, step by step

To get a block of order e:

1. If list e has a block, take it off the list and return it. Done.
2. Otherwise, get a block of order e+1, **by these same steps**
   (recursion). If there's none (we've gone past `u`), fail.
3. Split that order-(e+1) block into two halves of order e. Keep the
   **lower** half, and put the **upper** half on list e.
4. Return the lower half.

Keeping the lower half is a convention; either half would work. Keeping
the lower one means allocations pack toward the bottom of the pool.

Here is `balloc(p, 16)` on a fresh 256-byte pool (so only list 8 has a
block):

```
need order 4: list 4 empty → need order 5
need order 5: list 5 empty → need order 6
need order 6: list 6 empty → need order 7
need order 7: list 7 empty → need order 8
need order 8: take [0,256) off list 8
back at 7: split [0,256):  keep [0,128),  put [128,256) on list 7
back at 6: split [0,128):  keep [0,64),   put [64,128)  on list 6
back at 5: split [0,64):   keep [0,32),   put [32,64)   on list 5
back at 4: split [0,32):   keep [0,16),   put [16,32)   on list 4
return [0,16)
```

A second `balloc(p, 16)` finds `[16,32)` on list 4 right away: no
splitting at all. The first allocation's splits left pieces for later
requests.

**Cost:** at most one split per order, so at most `u − l` steps. That
doesn't depend on how many blocks are allocated.

### 2.7 Freeing, step by step

To free a block of order e:

1. Find its buddy (section 3 does this with one XOR).
2. If the buddy is **free, as a whole block of order e**, meaning it's
   on list e: take the buddy off list e and merge. The merged block is
   their parent, which starts at the lower of the two. It has order e+1.
   Go back to step 1 with the parent.
3. Otherwise, put the block on list e. Done.
4. A block of order `u` has no buddy to merge with, so it just goes on
   list u.

"Free as a whole block" matters in step 2. Say we free `[0,64)`, and
its buddy `[64,128)` has been split into `[64,80)` allocated and
`[80,96)`, `[96,128)` free. Most of the buddy's memory is free, but not
all of it, so it can't merge. The test is "is the buddy on list e,"
not "is the buddy's memory unused."

**Merges cascade.** Freeing one small block can merge all the way up.
From the state in 2.3 (`[0,16)` allocated, everything else free):

```
free [0,16), order 4: buddy [16,32)  on list 4 → merge → [0,32),  order 5
                order 5: buddy [32,64)  on list 5 → merge → [0,64),  order 6
                order 6: buddy [64,128) on list 6 → merge → [0,128), order 7
                order 7: buddy [128,256) on list 7 → merge → [0,256), order 8
                order 8 = u: no buddy; put [0,256) on list 8
```

This is exactly the reverse of the allocation in 2.6.

**Cost:** again at most `u − l` merges.

### 2.8 A longer trace

A 256-byte pool, `l = 4`, `u = 8`. Each line shows the pool as
`offset:A|Fsize` (Allocated or Free), in address order. This is the same
format `freeliststr()` produces, and these exact strings are checked by
the tests:

```
start                             0:F256
alloc 16  (split 256→128→64→32→16) 0:A16 16:F16 32:F32 64:F64 128:F128
alloc 16  (16:F16 was on list 4)  0:A16 16:A16 32:F32 64:F64 128:F128
alloc 64  (64:F64 was on list 6)  0:A16 16:A16 32:F32 64:A64 128:F128
free  0   (buddy 16 is allocated) 0:F16 16:A16 32:F32 64:A64 128:F128
free  16  (merge 0+16→32, then 0+32→64; buddy 64 is allocated)
                                  0:F64 64:A64 128:F128
free  64  (merge 0+64→128, then 0+128→256)
                                  0:F256
```

Notice the line after `free 0`. Block 0 is free and 32 is free, and
they're right next to each other, but they don't merge. 0's buddy is
16 (allocated), and 32's buddy is the whole `[0,32)`, which isn't a free
block. That's 2.4 in action.

### 2.9 The invariant: always fully merged

After every `bfree`, **no two buddies are ever both on the same free
list**. Why:

- `bfree` is the only thing that adds a block that could break this, and
  it checks exactly that block's buddy. If both are free, it merges
  them, then checks the parent the same way, one level up.
- `balloc` splits a block into two halves, puts the upper one on the
  list and hands the lower one out. So the two halves are never both
  free at that moment.

Two things follow:

- If an aligned region of the pool is entirely free, it is *one* free
  block, not a pile of small ones. Big requests succeed whenever the
  memory really is there in buddy-sized pieces.
- Section 5's bitmap trick relies on this. Since a pair never has both
  buddies on a list, one bit per pair is enough to say whether one of
  them is there.

### 2.10 What it costs

**Internal fragmentation.** Up to just under half of a block is wasted,
as the table in 2.2 shows. If request sizes are spread evenly, about a
quarter of the memory is wasted on average.

**External fragmentation** still happens, in a buddy-specific form. Fill
a 256-byte pool with sixteen 16-byte blocks, then free every other one.
Half the pool (128 bytes) is free, but every free block's buddy is
allocated, so nothing merges and a 32-byte request fails. The tests
include exactly this case.

**What you get in return:** allocation and freeing each take at most
`u − l` steps. There's no searching the pool, no headers on allocated
blocks, and the bookkeeping is a few bits per block (section 5). For a
kernel, that trade is worth it.

---

## 3. Buddy arithmetic: flip one bit

Section 2 kept saying "find the buddy" and "the merged block starts at
the lower one." This section shows that each of those is **a single bit
operation** on the block's offset. That speed is the reason for the
power-of-two rules.

### 3.1 An offset is a path through the tree

Write offsets in binary. For the 256-byte pool, offsets run from 0 to
255: 8 bits, numbered 7 (highest, worth 128) down to 0 (worth 1).

Take a block of order e. Its offset is a multiple of `2^e` (2.3), so
**its lowest e bits are all 0**. The bits above those say where the
block is in the tree. Read them from the top down, and each bit is one
step from the root: **0 = go to the lower half, 1 = go to the upper
half**.

Example: the order-4 block at offset 96. In binary, 96 = `0110 0000`.
Bits 3..0 are zero (it's order 4). Bits 7..4 are `0 1 1 0`:

```
bit 7 = 0:  [0,256)  → lower half [0,128)
bit 6 = 1:  [0,128)  → upper half [64,128)
bit 5 = 1:  [64,128) → upper half [96,128)
bit 4 = 0:  [96,128) → lower half [96,112)   ← the block
```

So for a block of order e:

| bits of the offset | tell you |
|---|---|
| above bit e | which parent the block belongs to |
| **bit e** | whether it's the **lower (0)** or **upper (1)** child of that parent |
| below bit e | nothing: they're all 0 |

### 3.2 The four operations

Every buddy operation follows from that table.

**The buddy.** It has the same parent (same bits above e), but it's the
*other* child (bit e is flipped). So:

```
buddy = off XOR 2^e
```

**The parent**, which is where a merged pair starts. The parent is at
the lower child's offset, so clear bit e:

```
parent = off AND NOT 2^e
```

This also gives "the lower buddy" of a pair, from either one.

**The upper half**, given away when splitting. After splitting a block
at offset `off` into two order-e halves, the lower half is `off` itself
and the upper half sets bit e:

```
upper = off OR 2^e
```

**Am I the upper half?** Test bit e:

```
off AND 2^e    (nonzero means upper)
```

Worked examples at order 4 (`2^4` = 16 = `0001 0000`):

```
off = 96  = 0110 0000
  buddy   = 0111 0000 = 112     (flip bit 4)
  parent  = 0110 0000 = 96      (bit 4 already 0: 96 is the lower one)
off = 112 = 0111 0000
  buddy   = 0110 0000 = 96      (flip back: the buddy of the buddy is you)
  parent  = 0110 0000 = 96      (both buddies give the same parent)
  upper?  = 0001 0000 ≠ 0       (yes)
```

The merge cascade from 2.7 in these terms, starting from offset 0 at
order 4:

```
order 4: buddy = 0  ^ 16  = 16,   parent = 0
order 5: buddy = 0  ^ 32  = 32,   parent = 0
order 6: buddy = 0  ^ 64  = 64,   parent = 0
order 7: buddy = 0  ^ 128 = 128,  parent = 0
```

And starting from 112 instead:

```
order 4: buddy = 112 ^ 16  = 96,   parent = 96
order 5: buddy = 96  ^ 32  = 64,   parent = 64
order 6: buddy = 64  ^ 64  = 0,    parent = 0
order 7: buddy = 0   ^ 128 = 128,  parent = 0
```

### 3.3 Why XOR, and not "plus or minus"?

The buddy is either `off + 2^e` (if you're the lower half) or
`off − 2^e` (if you're the upper). You could write that with an `if`.
XOR does both at once: if bit e is 0 it sets it (adds `2^e`), and if
it's 1 it clears it (subtracts `2^e`). This works only because of the
alignment in 2.3. The halves differ in bit e *and nowhere else*.

### 3.4 Testing alignment

"Is `off` where an order-e block could start?" is the same as "are its
lowest e bits all 0?":

```
off AND (2^e − 1) == 0
```

`2^e − 1` is e one-bits (for e = 4, `0000 1111`). `freelistsize()` uses
this to reject a pointer into the middle of a block. For example,
40 = `0010 1000`, and `40 AND 15 = 8`, so 40 can't be the start of a
16-byte block.

### 3.5 Offsets, not addresses

All of this uses **offsets from the start of the pool**, not raw memory
addresses. Why not just use addresses?

`mmap` returns a page-aligned address: a multiple of 4096 (or 16384).
That's fine for orders up to 12, but a pool with `u = 16` needs 65536-byte
blocks at multiples of 65536, and the pool itself may not start at one.
Subtracting the base first makes the pool "start at 0," so every order
works, wherever the pool happens to be. (One of the tests deliberately
puts a pool at an odd address, `base + 8`, to prove it.)

The provided `bbm.c` does this in every function: subtract base, apply
the bit operation, add base back.

| `bbm.c` function | code | operation |
|---|---|---|
| `baddrinv(base,mem,e)` | `base+((mem-base)^mask)` | the buddy |
| `baddrclr(base,mem,e)` | `base+((mem-base)&~mask)` | the parent / lower buddy |
| `baddrset(base,mem,e)` | `base+((mem-base)\|mask)` | the upper half |
| `baddrtst(base,mem,e)` | `(mem-base)&mask` | am I the upper half? |

where `mask` is `1<<e`, i.e. `2^e`. (Arithmetic like `mem-base` on
`void *` pointers is a GCC extension that treats them like `char *`,
counting in bytes.)

### 3.6 Numbering pairs

Two buddies share one parent, so a **pair** of buddies is really just
their parent node. We can number the pairs at order e by dropping the
low e+1 bits of the offset:

```
pair number = off >> (e+1)      (the same as off / 2^(e+1))
```

Both buddies get the same number, because they differ only in bit e,
which gets dropped. For order 4 in a 256-byte pool:

| buddies (offsets) | pair number |
|---|---|
| 0, 16 | 0 |
| 32, 48 | 1 |
| 64, 80 | 2 |
| 96, 112 | 3 |
| ... | ... |
| 224, 240 | 7 |

In `bbm.c`, `bitaddr()` computes this: it clears bit e (`baddrclr`),
then divides by `2^e` and by 2. (Clearing bit e first doesn't change the
result, since the division drops it anyway.)

`mapsize()` counts the pairs: the number of order-e blocks, rounded up,
halved and rounded up again. The rounding covers pools that aren't a
power of two, where the last block may have no buddy (section 6).

This numbering is what lets the allocator keep **one bit per pair**
instead of one per block. Section 5 explains what those bits mean.

### 3.7 Limits in the provided code

The masks in `bbm.c` are built as `1<<e`, where `1` is an `int`:

- `1<<31` overflows a signed 32-bit int (undefined behavior), so `e`
  must be at most 30. That's why `bcreate` rejects `u > 30`.
- The mask is stored in an `unsigned int` (32 bits), but `mem-base` is
  64 bits. In `baddrclr`, `~mask` becomes `0x00000000FFFFFFEF`
  (for e = 4) when widened, which also clears the high 32 bits of the
  offset. So offsets must stay below `2^32`. The pool size is an
  `unsigned int`, so they always do.

### 3.8 Try it

Answers are at the bottom of this section.

1. What is the buddy of the order-5 block at offset 160? Its parent?
2. Is offset 48 a valid start for an order-5 block?
3. Which pair number does offset 200 belong to at order 3? At order 6?
4. You free the order-4 block at 240 in an otherwise empty 256-byte
   pool. List every buddy it merges with, in order.

<details><summary>Answers</summary>

1. 160 = `1010 0000`. Flip bit 5 (32): `1000 0000` = 128. Parent: clear
   bit 5: 128.
2. No. 48 = `0011 0000`, and `48 AND 31 = 16 ≠ 0`.
3. Order 3: `200 >> 4` = 12. Order 6: `200 >> 7` = 1.
4. 224 (order 4), 192 (order 5), 128 (order 6), 0 (order 7): the merged
   block ends as `[0,256)`.

</details>

---

## 4. Where the bookkeeping lives

The spec makes two rules:

- **(a) No management data in allocated blocks.** Many malloc
  implementations put a small header (a size field) before every block.
  For a buddy allocator that is very wasteful: a 64-byte request plus an
  8-byte header no longer fits in 64 bytes and takes 128.
- **(b) Management data in free blocks.** A free block isn't holding
  anything, so we can use its memory. The first word of each free block
  holds a pointer to the next free block of the same order. That's an
  *intrusive* singly linked list: the list nodes *are* the free blocks,
  and the lists cost no memory of their own.

(b) is why `2^l` must be at least `sizeof(void *)` (8 bytes, so `l ≥ 3`):
a free block must hold one pointer. `bcreate` rejects a smaller `l`.

(a) raises a question: **when `bfree(p, m)` is called, how big is `m`?**
Nothing in `m` says. The answer is the bitmaps.

---

## 5. The two bitmaps

Each order e has two bitmaps in `freelist.c`, each with **one bit per
buddy pair**:

### 5a. The `free` bitmap: "is my buddy free?"

**Rule:** flip the pair's bit every time *either* buddy is put on, or
taken off, the order-e free list.

So the bit is the *parity* of how many of the two buddies are on the
list:

| lower on list? | upper on list? | bit |
|---|---|---|
| no | no | 0 |
| yes | no | 1 |
| no | yes | 1 |
| yes | yes | 0, and this never lasts: they'd have merged |

Now free a block. It is allocated, so **it is not on the list**, and the
pair's bit is 1 exactly when **the buddy is on the list**. One bit test
replaces a search of the free list. This is the Linux kernel's trick
(Gorman, ch. 6) and it's the whole merge decision in `freelistfree()`:

```c
for ( ; e<r->u && bbmtst(lv(r,e)->free,r->base,mem,e); e++) {
  unlist(r,e,baddrinv(r->base,mem,e));  // absorb the free buddy
  bbmclr(lv(r,e)->alloc,r->base,mem,e); // neither buddy is allocated
  mem=baddrclr(r->base,mem,e);          // the merged block: lower half
}
push(r,e,mem);
```

The flipping happens inside `push`, `pop` and `unlist`, the only three
functions that change a list. The rule can't be forgotten anywhere else.

### 5b. The `alloc` bitmap: "what size was this block?"

**Rule:** when a block is allocated at order e, set its pair's bit.
When a pair merges, clear its bit.

To find the order of an allocated block `m`, **scan upward from `l`**
and return the first order whose alloc bit (for the pair containing
`m`) is set. That's `freelistsize()`.

**Why the first set bit is the right one.** Say `m` was allocated at
order c. Every pair *below* order c that contains `m` lies entirely
inside `m`'s block. Nothing inside an allocated block is ever allocated
or on a free list, so those bits are all 0. At order c the bit is set,
because `m` itself set it. The scan can't stop early, and it stops at c.

**Why "either buddy" and not "this buddy"?** It's one bit per pair. So
if `m` is allocated at order c and its buddy is too, the bit covers
both. When `m` is freed and its buddy is still allocated, we must leave
the bit set. We can't cheaply tell "buddy is allocated at order c" from
"buddy was split into smaller pieces", so in the second case the bit
stays set even though it's no longer needed: it's **stale**.

**Stale bits are harmless:**
- They can't mislead a size lookup. A lookup for a real allocated block
  never looks above that block's own order (above).
- They don't last. The pair can only become part of a bigger block by
  merging, and merging clears the bit. The test "a merge clears the
  pair's alloc bit" checks exactly this.

### 5c. Using both bitmaps to reject bad pointers

`bfree` and `bsize` get a pointer from the user, which might be wrong:
freed twice, from the middle of a block, from another pool, or from the
stack. `freelistsize()` returns -1 unless every check passes:

1. `m` is inside the pool.
2. While scanning up, below the candidate order every pair containing
   `m` has **both** bits 0. (A real block's insides are empty.) A free
   bit of 1 means `m` is inside a block that was split, so it's not a
   block start.
3. At the candidate order, `m` is **aligned** to that order and **not on
   the free list**.

Check 2 is what catches a double free after merging. Once `m` has merged
into a bigger free block, it's no longer a block start. A pointer into a
split block always shows a 1 somewhere on the way up: at the bottom
order, blocks can't be split any further, and two free buddies would
already have merged.

---

## 6. Pools that aren't a power of two

Requirement 3 says a pool whose size isn't a power of two, or is larger
than `2^u`, still gets "reasonably sized blocks." `freelistcreate()`
tiles the pool with the **largest aligned blocks that fit**:

- as many blocks of `2^u` as fit, then
- one block for each 1-bit of the remainder, largest first.

That's the size written in binary. (`bcreate` first rounds the size up to
a multiple of `2^l`, so no piece is smaller than `2^l`.)

```
256,  u=8 → 0:F256
256,  u=6 → 0:F64 64:F64 128:F64 192:F64         (larger than 2^u)
208,  u=8 → 0:F128 128:F64 192:F16               (208 = 128+64+16)
6000, u=12 → 4096+1024+512+256+64+32+16
```

**Orphans.** Look at `192:F16` in a 208-byte pool. Its buddy would be at
`208`, past the end of the pool. It must never merge. The free bitmap
handles this with no special case. The block is pushed on the list at
startup, which flips its pair's bit to 1, and the buddy never exists to
flip it back. Whenever this block is freed later, its pair's bit is 0
("buddy isn't free"), so it doesn't merge. The same holds for `128:F64`,
whose buddy `192..256` runs off the end.

Blocks of order `u` never merge either: the `freelistfree` loop stops at
`e == u`.

The carving loop pushes from the top of the pool down, so each list ends
up in address order and the first allocation comes from the bottom:

```c
size_t top=e2size(u), off=size;
for (int e=l; e<u; e++)
  if (size%top & e2size(e))
    push(r,e,base+(off-=e2size(e)));
while (off)
  push(r,u,base+(off-=top));
```

---

## 7. The modules

```
  balloc     public interface: validate, round size→order, report misuse
    │
  freelist   the algorithm: split, merge, find order; lists + 2 bitmaps/order
    │
  bbm        bitmap indexed by buddy pair; buddy address arithmetic  (provided)
    │
  bm         plain bitmap: set/clr/tst bit i                         (provided)
    │
  utils      mmap wrappers, powers of two, single-bit ops
```

Each module uses only the ones below it, and each hides its
representation behind a `void *` handle (`Balloc`, `FreeList`, `BBM`,
`BM`), the same pattern as HW1's `Deq`.

**utils.** `mmalloc` returns `(void *)-1` (`MMFAILED`) on failure, like
`mmap`, not 0 like `malloc`. That's because the provided `bm.c` checks
`(long)p==-1`. `size2e(n)` is the smallest `e` with `2^e ≥ n`: the order
a request needs.

**bm (provided).** A bitmap with its length stored one `size_t`
*before* the pointer you're handed. `bmdelete` steps back to find it.
The same trick lets `free()` in many mallocs find a block's size.

**bbm (provided).** Turns an address into a pair number
(`off / 2^(e+1)`) and calls `bm`. It also has the four `baddr*` bit
operations from section 3.

**freelist.** One `Level` per order: a list head and two BBMs. The only
functions that change a list are `push`, `pop` and `unlist`, and they
also flip the free bit, so the invariant can't get out of sync.

**balloc.** Thin: checks arguments, turns a size into an order, turns
"-1" into a warning, and owns the pool's memory.

**Why the freelist interface changed.** The handout's version passes
`base`, `l` and `u` to almost every call (`freelistalloc(f, base, e, l)`),
yet `freelistalloc` has no `u`, so it can't tell when to stop looking for
a larger block. Storing `base`, `l`, `u` in the `FreeList` fixes that, and
a caller can no longer pass values that don't match. The spec allows
changing these interfaces. `freeliststr()` was added as the "to-string"
the spec recommends.

---

## 8. The algorithm, as code

**Allocation** (`take` in `freelist.c`) is recursive:

```c
static void *take(Rep r, int e) {
  if (e>r->u)                     // nothing larger exists
    return 0;
  void *b=pop(r,e);               // a free block of order e?
  if (b)
    return b;
  b=take(r,e+1);                  // no: get one twice as big
  if (b)
    push(r,e,baddrset(r->base,b,e));  // keep lower half, free upper
  return b;
}
```

`freelistalloc` calls `take`, then sets the alloc bit at order e.

**Freeing** is the loop in section 5a. **Size lookup** is the scan in
5b and 5c. **Merging** needs `unlist`, a walk down a singly linked list
with a pointer-to-pointer, so removing the head needs no special case:

```c
for (void **pp=&v->head; *pp; pp=*pp)
  if (*pp==b) { *pp=next(b); flip(...); return; }
```

---

## 9. Errors

HW1's rule: fatal if the program can't go on, warn if the caller made a
mistake we can survive, and quietly return 0 if nothing is wrong.

| situation | response |
|---|---|
| bad `bcreate` arguments, `mmap` failed | `WARN`, return 0 |
| `bfree`/`bsize` of a non-block (middle, freed, foreign, stack) | `WARN`, do nothing / return 0 |
| zero pool | `WARN` |
| request > `2^u`, or pool full | return 0, **no** message, like `malloc` |
| `bfree(p, 0)` | nothing, like `free(0)` |
| bitmap index out of range (inside provided `bm.c`) | message, `exit(1)` |

Why no message when the pool is full? Under the wrapper, the allocator
*is* `malloc`. Running out of memory is a normal result, and the caller
decides what to do about it (the deque calls `ERROR`).

---

## 10. Testing

- **Bottom up.** utils, then bm, bbm, freelist, balloc. If a low module
  breaks, its own tests fail first, and you don't end up debugging the
  allocator for a bitmap bug.
- **Exact maps.** `freeliststr()` gives the whole pool state as one
  string, so a test can check *precisely* what should happen, e.g.
  `"0:F64 64:A64 128:F128"` after a two-level merge. That's much stronger
  than "balloc didn't return 0."
- **Misuse** is tested on purpose, and each expected message is announced
  first.
- **Fatal errors** are tested in a child process (`dies()` uses `fork`),
  so the suite survives `bm.c`'s `exit(1)`.
- **Stress.** 50,000 random operations. Each block is filled with a byte
  pattern and checked before it's freed, so any overlap between two
  blocks would corrupt one and be caught. At the end everything is freed
  and a whole-pool allocation must succeed, which proves every merge
  happened.
- **Sanitizers.** On a Mac there's no valgrind, but
  `gcc -fsanitize=address,undefined` catches the same kinds of bugs
  (bad pointers, undefined shifts). Run `make valgrind` on onyx.

### The wrapper, and symbol interposition

`deq/wrapper.c` *defines* `malloc`, `free` and `realloc`. On Linux, the
dynamic linker resolves every call to `malloc`, **including calls inside
libc itself** (from `strdup`, `asprintf`, and stdio's buffer setup), to
the first definition it finds, and the program's own comes first. So
the unchanged HW1 deque and its tests run entirely on our pool.

Two consequences, both found by simulating it:

1. **It needs Linux + gcc.** `wrapper.c` defines `min()` inside
   `realloc()`, a GCC-only nested function. And macOS links libc calls
   directly to its own malloc, so libc wouldn't use ours anyway. Test it
   on onyx.
2. **stdout's buffer lives in the pool.** glibc `malloc`s it on the first
   `printf`, sized to the output device: about 1024 bytes for a
   terminal, but 4096 for a file or pipe. That's the *entire* 4096-byte
   pool, leaving nothing for the deque. The first `deq_new` then fails.
   So run `./deq` on a terminal. The simulation passed all 54 deque
   tests with buffers up to 2048 bytes and ended with the pool fully
   merged (`0:F4096`): the deque frees everything it allocates.

---

## 11. Costs and trade-offs

| operation | time |
|---|---|
| `balloc` | O(u−l): at most one split per order |
| `bfree` | O(u−l) bit tests + O(list length) for each `unlist` and for the double-free check |
| `bsize` | O(u−l) + O(list length) |

The list-length term comes from **singly** linked lists: removing a
buddy from the middle means walking to it. A doubly linked list would
make removal O(1), but it needs two pointers per free block, so
`2^l ≥ 16`. The spec asks for "a pointer," so we use one.

**Memory overhead.** Order e has `size / 2^(e+1)` pairs, with 2 bits
each. Summed over all orders, that's under `2·size/2^l` bits. For
`l = 4` that's about 1.6% of the pool. Each bitmap is its own `mmap`, so
it really takes at least a page. That's fine for a homework, but a real
kernel packs them together.

**Internal fragmentation.** Up to half of each block can be wasted
(a 2^k+1-byte request takes 2^(k+1) bytes). **External
fragmentation:** free memory that isn't in buddy-aligned pieces can't be
merged. The "free every other block" test shows half the pool free and
no 32-byte block available.

---

## 12. Check yourself

1. What is the buddy of the order-5 block at offset 96? (96 ^ 32 = 64.)
   Of the order-6 block at 64? (0.)
2. Why must `2^l ≥ sizeof(void *)`?
3. A block is being freed. Why does "the free bit is 1" mean "the buddy
   is free", and why is that only true *because the block being freed is
   not on the list*?
4. Why can't a stale alloc bit make `bsize` return the wrong size for a
   real allocated block?
5. In a 208-byte pool with `u = 8`, why does the block `192:F16` never
   merge, even though the code never checks for "past the end"?
6. Why does the first allocation come from the bottom of the pool?
7. What would go wrong if `bcreate` called `malloc` for its descriptor
   while `wrapper.c` is linked in?
