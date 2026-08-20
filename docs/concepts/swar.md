# Doing eight bytes at a time {#concept_swar}

SWAR — SIMD Within A Register. Ordinary integer operations, used to process every byte of a word at
once, branch-free.

## The idea

A `uint64_t` holds eight bytes. An `AND`, an `XOR` or a subtract on that word operates on all eight
simultaneously, on any CPU, with no vector unit and no intrinsics. The whole technique is arranging
for the answer you want to fall out of arithmetic that was going to happen anyway.

Two constants set it up, both derived from the carrier width:

```c
MMGR_SWAR_ONES    /* 0x0101...01 - a 1 in the low bit of every lane */
MMGR_SWAR_HIGHS   /* 0x8080...80 - a 1 in the high bit of every lane */
```

## Worked example: is any byte zero

The classic. `scrut.has_zero(w)` answers "does this word contain a zero byte" in three operations.

```c
(w - MMGR_SWAR_ONES) & ~w & MMGR_SWAR_HIGHS
```

Read it a lane at a time:

- `w - ONES` subtracts 1 from every lane at once. A lane holding `0x00` borrows, and the borrow sets
  that lane's high bit. A lane holding anything from `0x01` upward does not.
- `& ~w` discards lanes whose high bit was already set in `w` — those would otherwise look like a
  borrow when they are just a byte ≥ `0x80`.
- `& HIGHS` keeps only the high bit of each lane, so the result is non-zero exactly when some lane
  held zero.

No branch, no loop, eight bytes answered. `strlen` over a long string becomes one of these per word
plus a tail.

The lane-index helpers on top of it (`lane_lo`, `lane_first`, `drop_*`) turn "some lane matched"
into "which lane", which is what a `find` needs.

## The carrier is the machine word

Not a knob, and this is stated flatly in the header:

> The carrier is the machine word. Always, on every target, with no choice about it.

A narrower carrier is never faster. A 16-bit load on a 32-bit machine moves the same cache line,
occupies the same load port, and then answers for a quarter of the lanes. `MMGR_SWAR_BITS` is
`#undef`ed and redefined to `MMGR_WORD_BITS` in `mmgr_config.h` precisely so nobody can set it to
something else and quietly make the library slower.

## No builtins

There are no `__builtin_ctz`, `__builtin_popcount` or intrinsics anywhere in `src/`. That is a
measured decision, not an aesthetic one:

- An open-coded trailing-zero fold measured **3.609 cycles** against **3.680** for
  `__builtin_ctzll` — the builtin was not faster.
- `__builtin_popcountll` compiles to a **call to `__popcountdi2`** on baseline x86-64, because
  `popcnt` is not in the base ISA. A libgcc call in the middle of a branch-free scan is worse than
  the fold it replaced.

Avoiding them also means there is no `#ifdef` ladder per compiler in the scan path, which is the
second reason: every compiler conditional in the library lives in one file
(`mmgr_compiler_directives.h`), and the scanner is not allowed to add to it.

## What it is worth

Measured on gcc 13.2 `-O2`, x86-64. `cycles_per_byte` is `cycles_per_call / MMGR_SWAR_BYTES`, which
is the column the design claim lives in: a wider carrier answers for more bytes per register
operation, so cycles-per-byte should fall as the carrier widens.

| case       | 16-bit | 32-bit | 64-bit | 16 → 64 |
| ---------- | -----: | -----: | -----: | ------: |
| `load`     |   4.70 |   2.03 |   0.89 |    5.3x |
| `has_zero` |   8.80 |   3.32 |   1.36 |    6.5x |
| `le`       |   7.78 |   3.14 |   1.43 |    5.4x |
| `spread`   |   7.74 |   2.89 |   1.33 |    5.8x |
| `eq`       |  10.09 |   4.30 |   2.32 |    4.4x |

It does fall — roughly halving per doubling. `eq` sits about 11% above `has_zero`, which is the
`movzbl`, `imul` and `xor` it adds. Nothing to explain.

Read these as a comparison **between rows of one build**, never against a number from another
machine. @ref ref_performance explains how to run them and how to tell a result from noise.

## Where LTO comes in

The hot entries are `MMGR_INLINE` in their headers, so the compiler can inline them without help.
For the cross-module calls that are not, link-time optimization is still load-bearing: `mmgr_memor_chr`
over 512 bytes measured **610 cycles** with the SWAR entries as out-of-line calls and **187** when
the linker was allowed to inline them. `MMGR_LTO` defaults to `ON` for that reason.
