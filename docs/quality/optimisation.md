# What each optimisation level costs {#qa_optimisation}

One optimisation level for a whole library is a guess that suits some of it. This page is the
measurement, so a module that names its own level has a reason on record rather than a preference.

Reproduce it with:

```
python tools/dev_env/sizes.py -O0 -O1 -Os -O2 -O3
```

The tool reads its compiler options out of `build/compile_commands.json` rather than keeping a copy,
so it cannot drift from `CMakeLists.txt`. It takes out the level, because that is what varies, and
link time optimisation, because an LTO object holds intermediate form rather than instructions and
its size says nothing about what would reach a target. Sections are read rather than file lengths -
an object also carries relocations, symbol tables and debug records that never get flashed.

## Size

`.text`, in bytes, per translation unit.

| translation unit           |   -O0 |   -O1 |       -Os |   -O2 |   -O3 |
| -------------------------- | ----: | ----: | --------: | ----: | ----: |
| `cellularum_laboro`        | 17536 |  8704 |  **5936** |  8736 | 11472 |
| `verba_scribo`             | 14784 |  6240 |  **5280** |  7712 | 12144 |
| `memoria_operor`           |  3968 |  1040 |   **912** |  1424 |  1728 |
| `confinium`                |  3856 |  1392 |  **1120** |  1536 |  2048 |
| `numeros_scribo`           |  3488 |  1536 |      1536 |  1536 |  1536 |
| `octetus_introitus_exitus` |  3296 |   448 |   **400** |   560 |   560 |
| `occultum_custodiae`       |  1984 |   944 |   **608** |  1088 |  1408 |
| `proximus_operor`          |  1584 |   320 |   **224** |   336 |   816 |
| `clarus_custodiae`         |  1104 |   640 |   **480** |   768 |   960 |
| `fractio`                  |   960 |   192 |       192 |   240 |   240 |
| `endian`                   |   848 |   336 |   **192** |   320 |   352 |
| `bitorum_introitus_exitus` |   416 |   176 |   **160** |   208 |   208 |
| `spatium`                  |    96 |    32 |    **32** |    32 |    32 |
| **total**                  | 53920 | 22000 | **17072** | 24496 | 33504 |

Four of those moved for reasons that are not the optimiser.

`spatium` was 464 bytes at -O2 and is 32. Eleven of its twelve entries had no caller anywhere in the
library — they were names on field reads, and the one module that writes through a span reads the
fields directly. What is left is `from`.

`octetus_introitus_exitus` was 720 and is 560, because a write that does not fit is a contract now
rather than a branch: the caller has the buffer and the field width in front of it, so `MMGR_ASSERT`
says it for nothing in a shipping build and an abort in `checks`. `clarus_custodiae` and
`occultum_custodiae` shed the per-worker indexing.

`verba_scribo` went the other way, 5360 to 7712. That is the decimal engine it now inlines, and both
of its render entries were wrong without it. `verba.fixed` was 15.87% wrong below about 1e-41 and
`verba.g` failed to name its own value back 87.07% of the time; both are 0.0000% now. 1264 bytes for
the first and 1088 for the second is what that costs. See @ref qa_numeric.

Constants barely move: 2576 bytes at -O2 against 2608 at -O3. All of the growth is instructions.

## Speed

Cycles. `find`, `len` and `copy` are per byte; `parse` and `render` are per call. The measuring
harness is always built at -O2 so only the library moves between rows.

| level |    `find` |     `len` | `to_double` | `verba.g` | `memor.cpy` |
| ----- | --------: | --------: | ----------: | --------: | ----------: |
| -O0   |     7.457 |     3.179 |       251.7 |    1051.2 |       3.410 |
| -O1   |     1.128 |     0.559 |        70.9 |     704.1 |       0.703 |
| -Os   |     1.136 | **0.405** |        71.1 |    1073.7 |       0.699 |
| -O2   |     0.985 |     0.559 |        62.8 |     453.1 |   **0.258** |
| -O3   | **0.974** |     0.555 |    **53.9** | **441.3** |   **0.258** |

## What the numbers say

**-O1 to -O2 is where the speed is.** `memor.cpy` goes 0.703 to 0.258, a 2.7 times step, for 2,432
bytes across the library. That is the one jump that clearly pays.

**-O2 to -O3 buys about 2% on the scans for 8,256 bytes.** `find` moves 0.985 to 0.974. `len` and
`copy` do not move at all. The scanning entries are already the shape they want to be: there is no
loop left for -O3 to unroll into something better, so it inlines and unrolls anyway and the code
gets bigger for nothing.

The exception is `to_double` at -14%, which is real - it has an actual loop over the table.

**-Os is not uniformly slow.** It is the smallest by a distance, 31% under -O2, and `len` is the
fastest of any level there while `copy` is within 1% of -O2. What falls off a cliff is `verba.g`,
at 1073 against 453 - worse than -O1.

## Where a module names its own level

`mmgr_add_module` takes `OPTIMIZE`, which appends after the build's own flags. The last `-O` on the
command line is the one that counts, so nothing has to be removed for it to take effect.

| module            | level | why                                                                                                                                                      |
| ----------------- | ----- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `verba_scribo`    | -O2   | -O3 costs 4432 bytes, 57% more than the whole module at -O2, for 1.5% on a render. The digit loops are short and already shaped.                        |
| `proximus_operor` | -O2   | -O3 more than doubles it, 336 bytes to 816, and moves nothing measurable. The entries are single loads and stores that are already one instruction each. |

`cellularum_laboro` is left at the build's level: -O3 costs it 2,736 bytes and returns 14% on
`to_double`, which is a real workload rather than a microbenchmark artefact.

## Against the libc it would replace

`tools/dev_env/against_libc.py` weighs this library against newlib for the same core at the same
level. newlib because it is the libc an embedded target actually ships, it is a static archive with
one object per entry so a single function can be weighed, and it is built by the same compiler
family. Only `.text` is counted: an archive member also carries relocations and symbol tables that
never reach flash, and the linker pulls whole members, so a member is the unit whether or not every
entry in it is called.

cortex-m4, -Os, newlib from armv7e-m:

| family                     | MMgr |    newlib |       |
| -------------------------- | ---: | --------: | ----: |
| moving and comparing bytes |  736 |       924 | 1.26x |
| searching and parsing text | 5084 |     15816 | 3.11x |
| rendering numbers and text | 7256 |     21364 | 2.94x |
| **total**                  | **13076** | **38104** | **2.91x** |

**25,028 bytes of flash**, for the same set of jobs.

Every entry on both sides is bounded, which is why the libc column names `strnlen` and `strncmp`
rather than their unbounded twins, and `snprintf` and `vsnprintf` rather than `printf`. A bounded
string constructor is what `verba` is; comparing it against something that writes until it is
finished would be comparing two different contracts.

The families are drawn where the code actually is rather than where the header names suggest.
`cellularum_laboro` holds the bounded scans and the decimal parser in one translation unit, which on
the newlib side is `str*` plus the whole `strtod`, `dtoa` and `mprec` apparatus. Every newlib member
is named once across all three families - `dtoa` and `mprec` serve both parsing and printing there,
and counting them twice would flatter this library by six kilobytes.

Where the gap comes from is worth being precise about.

Parsing is 3.11x because newlib spends 10,272 bytes on `strtod` + `dtoa` + `mprec`, and `mprec` is
an arbitrary-precision bignum. It is exact for every input by carrying however many limbs the input
needs. This library is exact for every input by carrying 128 bits and never growing, because 128
bits is enough to decide a rounding and the rest of the expansion is never looked at. That is a
real difference in approach and not a trick of accounting, so the obvious question is why the other
side does not do the same. Three answers, and the third is the one that matters here.

**The result is newer than the code.** newlib's is David Gay's `dtoa` and `mprec`, which date from
1990 and are the reference implementation everyone inherited. That a fixed-width intermediate always
suffices — that you need enough bits to decide guard, round and sticky and never the full expansion
— is Grisu in 2010, Ryu in 2018, Eisel-Lemire in 2020. Twenty to thirty years later. Gay's code is
not naive; it is correct, and it predates the result that makes it unnecessary.

**A bignum serves every width with one body.** `mprec` is exact for `double`, `long double` and
whatever else the target has, because limbs do not care. The table here is 360 bytes of powers of
five sized for binary64 and nothing else. libc has to answer for all of them; this library answers
for one, and bought the difference by narrowing what it promises.

**And a bignum has to put its limbs somewhere.** `mprec.o` in this newlib carries undefined
references to `malloc` and `_calloc_r`. On a target with a heap that is a cost. On a target without
one it is not a trade at all — the entry does not link. So the table is not a smaller way of doing
what libc does. It is the only way available to a library that must not allocate, and it happens to
also be smaller. See @ref qa_numeric.

Rendering is 2.94x and the `FILE` members are counted, which needs saying plainly: newlib's
`snprintf` is built on its `FILE` machinery. It constructs a fake stream over the caller's buffer
and goes through `vfprintf`, so linking `snprintf` links `fvwrite`, `findfp`, `fflush`, `makebuf`
and `wsetup` whether or not a stream is ever opened. That is what a caller pays for the entry they
called, so it is counted - not because printing to a stream is a wider job. The rest of the gap is
the format string: newlib parses one at run time, and `verba` has none, because a call names the
entry it wants.

Nothing stops this library sitting under a file layer once the pieces below it are correct. That is
a separate concern and not what this table is about.

Bytes is 1.26x, which is the honest number for a fair fight: `memcpy` against `memcpy` is nearly a
wash, because there is not much room in either.

### And the speed, on a libc that can be measured

Size is measured cross-compiled for a cortex-m4, which cannot be run here. Speed is measured on the
host against glibc — a different target and a different libc, so the two tables are about different
things and neither transfers to the other.

gcc 13.3, glibc 2.39, x86-64. Same corpus, same order, best of fifteen:

| entry              |     MMgr | glibc              |           |
| ------------------ | -------: | ------------------ | --------: |
| `cellul.to_double` | 179.2 ns | `strtod` 292.1 ns  | **1.63x** |
| `verba.g`          | 397.5 ns | `snprintf` 954.2 ns | **2.40x** |

Correctness on the same run, because a speed number for a wrong answer is not a number: `to_double`
differs from glibc's `strtod` on **0 of 2000** random bit patterns, and `verba.g` rendered at
seventeen digits and read back by glibc names its own value on **0 of 2000**.

So the comparison is identical accuracy, fewer bytes, and fewer cycles, on both directions of the
conversion. Not a trade.

Two things that number is not. It is one host, and msvcrt's `strtod` — hand-written x86 assembly —
is faster than both. And the parser has not had the treatment the rest of the library has had:
`cellularum_laboro` is still a single translation unit holding the scans and the decimal parser
together, and it is the one module that has not been cut back to the shape the others are in. The
1.63x is what it does before that, not after.

## The honest caveat

These are one compiler on one target - gcc 13.2 on x86-64. The shape of the argument holds
elsewhere: code that is already minimal does not benefit from being unrolled. The specific numbers
do not. Rerun the tool on the target that matters before quoting them at anybody.
