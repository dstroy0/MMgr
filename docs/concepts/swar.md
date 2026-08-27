# Doing eight bytes at a time {#concept_swar}

SWAR — SIMD Within A Register. Ordinary integer operations, used to process every byte of a word at
once, branch-free.

## The idea

A `uint64_t` holds eight bytes. An `AND`, an `XOR` or a subtract on that word operates on all eight
simultaneously, on any CPU, with no vector unit and no intrinsics. The whole technique is arranging
for the answer you want to fall out of arithmetic that was going to happen anyway.

Three constants set it up, all derived from the carrier width rather than tabulated, so they are
correct at 16, 32 and 64 bits without a `#if`:

```c
#define MMGR_SWAR_ONES           (((mmgr_word)~(mmgr_word)0) / 0xFFu)  /* 0x0101...01 */
#define MMGR_VERBUM_SCRUTOR_HIGH (MMGR_SWAR_ONES * 0x80u)              /* 0x8080...80 */
#define MMGR_SWAR_LOW7           (MMGR_SWAR_ONES * 0x7Fu)              /* 0x7F7F...7F */
```

`ONES` is the broadcast multiplier — multiply a byte by it and you get that byte in every lane.
`HIGH` is where an answer lands. `LOW7` is the room a lane has to carry into before it disturbs the
answer bit.

## Worked example: is any byte zero

`lane.has_zero` answers "does this word contain a zero byte" in five operations, and every predicate
in the module is built on it.

```c
~(((w & MMGR_SWAR_LOW7) + MMGR_SWAR_LOW7) | w) & MMGR_VERBUM_SCRUTOR_HIGH
```

Read it a lane at a time:

- `w & LOW7` drops the top bit of every lane, so no lane can carry into its neighbour.
- `+ LOW7` adds `0x7F` to each. A lane whose low seven bits held anything at all carries into bit 7;
  a lane that held zero in those bits does not.
- `| w` puts the top bits back, so bit 7 of each lane is now set exactly when the lane was nonzero —
  either because the add carried, or because the byte was `0x80` or above to begin with.
- `~` inverts it, and `& HIGH` keeps one bit per lane. What is left is the answer: a set bit for
  every lane that held zero.

No branch, no loop, eight bytes answered. `strlen` over a long string becomes one of these per word
plus a tail.

The `mask` table on top of it turns "some lane matched" into "which lane": `lane.count` says how
many, `lane.first` and `lane.last` say where, `mask.drop_first` and `mask.drop_last` walk them in
order, and `mask.tail` trims the last word to the caller's length. That is what a `find` needs.

Called through the table:

```c
const mmgr_word w = MMGR_CALL(word.load,     ScrutWordCfg, .at = p);
const mmgr_word m = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w);
if (m != 0)
{
    at += MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
}
```

**Use `word.load_al` in a walk, not `word.load`.** `load` takes any address, which it pays for:
`mmgr_proxim_word_t` carries `MMGR_ALIGN(1)`, and on a target with no unaligned word load the
compiler assembles each one out of byte loads and shifts — twelve instructions on Xtensa, eleven on
RISC-V, one on ARMv7-M. A walk steps to the first word boundary once and reads the body through
`load_al`, which is a single instruction everywhere. That change alone took `cellul.len` from 3.788
cycles per byte to 3.055. `load` is for the address that genuinely is arbitrary — verifying a
candidate mid-haystack, say.

## The carrier is the machine word

Not a knob. A narrower carrier is never faster: a 16-bit load on a 32-bit machine moves the same
cache line, occupies the same load port, and then answers for half the lanes.

`mmgr_config.h` `#undef`s `MMGR_SWAR_BITS` and redefines it to `MMGR_WORD_BITS`, unconditionally,
before anything else can look at it. That is not belt and braces — it is the one place where a
stale definition from somewhere else could have desynced the scanner from the word, and the
`#undef` removes the possibility.

What *is* a knob is `MMGR_WORD_BITS` itself. Set it and the carrier and the machine word move
together, which is how the `word32` and `word16` environments run a narrow machine's scan path on a
64-bit host. See @ref concept_width.

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

Link-time optimization is load-bearing, and it is worth being blunt about how much. These entries are
generated in `verbum_scrutor.c` by `GENERIC_ENTRY` and called by name from every other module, so no
translation unit can inline them on its own. `mmgr_memor_chr` over 512 bytes measured **610 cycles**
with them as out-of-line calls and **187** when the linker was allowed to inline them. On an
ESP32-S3, `cellul.len` measured **26.6 cycles/byte without LTO against 5.0 with it** — worse than the
byte-at-a-time ROM `strnlen` it is compared to. `MMGR_LTO` defaults to `ON` for that reason.

@warning A toolchain that cannot do link-time optimization does not give a slower build of the same
library; it gives a different one. ESP-IDF appends `-fno-lto` to every link unconditionally, so an
IDF project has to take that flag back out — see `test/performance_benching` for how.
