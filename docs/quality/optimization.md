# What each optimization level costs {#qa_optimization}

One optimization level for a whole library is a guess that suits some of it. This page is the
measurement, so a module that names its own level has a reason on record rather than a preference.

Reproduce it with:

```
python tools/dev_env/sizes.py -O0 -O1 -Os -O2 -O3
```

The tool reads its compiler options out of `build/compile_commands.json` rather than keeping a copy,
so it cannot drift from `CMakeLists.txt`. It takes out the level, because that is what varies, and
link time optimization, because an LTO object holds intermediate form rather than instructions and
its size says nothing about what would reach a target. Sections are read rather than file lengths -
an object also carries relocations, symbol tables and debug records that never get flashed.

## Size

`.text`, in bytes, per translation unit. Each unit compiled on its own, no LTO and no link, so a row
is that unit alone:

```
gcc -std=c11 -I src -O<level> -c <unit>.c -o <unit>.o && size <unit>.o
```

| translation unit                 |    -O0 |   -O1 |       -Os |   -O2 |   -O3 |
| -------------------------------- | -----: | ----: | --------: | ----: | ----: |
| `verba_scribo`                   |  42464 | 10080 |  **7104** |  9920 | 12192 |
| `cellularum_laboro`              |  21328 | 12576 | **10560** | 13168 | 13360 |
| `transformo`                     |   8736 |  2800 |  **2240** |  2576 |  3872 |
| `confinium_exclusivum_infinitas` |   7280 |  2128 |  **1952** |  2256 |  2720 |
| `carceribus`                     |   6160 |  1232 |  **1056** |  1456 |  1568 |
| `verbum_scrutor`                 |   5216 |  1424 |  **1392** |  1632 |  1632 |
| `memoria_operor`                 |   3328 |  2272 |  **2080** |  2464 |  2720 |
| `octetus_introitus_exitus`       |   3296 |  1312 |  **1184** |  1456 |  1488 |
| `numeros_scribo`                 |   3200 |  1424 |  **1152** |  1536 |  1536 |
| `spatium`                        |   2224 |   432 |   **384** |   496 |   496 |
| `endian`                         |   1648 |   880 |   **576** |   704 |   704 |
| `proximus_operor`                |   1120 |   256 |   **240** |   352 |  1040 |
| `bitorum_introitus_exitus`       |    944 |   352 |   **320** |   400 |   416 |
| `clz`                            |    704 |   288 |       288 |   288 |   288 |
| `fractio`                        |    464 |    96 |        96 |   144 |   144 |
| `ascii_persona_bitorum`          |    160 |    64 |        64 |    64 |    64 |
| `impensa_ancorae_acus_*`         |     64 |    16 |        16 |    16 |    16 |
| **total**                        | 108592 | 37696 | **30768** | 38992 | 44320 |

The five `impensa_ancorae_acus_*` units are one row because they are alternatives, not additions — a
build links exactly one cost table and they all define the same symbol. The total counts one.

**-Os is the smallest and -O2 is not the middle.** -O1 comes in under -O2 by 1296 bytes, so a build
that wants small and does not want to think about it should ask for -Os and stop there. -O3 costs
5328 bytes over -O2 across the library, and @ref ref_performance is where to look before paying it.

Two units carry most of it. `verba_scribo` and `cellularum_laboro` are three fifths of the total at
every level, which is what a decimal engine and a string module cost. `verba_scribo` is also the one
unit where the size is buying correctness rather than speed: both of its render entries were wrong
before it inlined that engine — `verba.fixed` by 15.87% below about 1e-41, and `verba.g` failing to
name its own value back 87.07% of the time. Both are 0.0000% now. See @ref qa_numeric.

`proximus_operor` more than quadruples from -Os to -O3, 240 to 1040, which is the widest spread in
the table. It is small enough that this does not matter to the total, but it is the unit to look at
first if a target is tight and -O3 is on.

`cellularum_laboro` and `memoria_operor` are the two units the on-device work changed, and both grew
at -O2: the walks stopped rebuilding an extent mask and a lane index on every word, and the region
moves were unrolled to four words. That is size spent to hold libc's rate on the parts the library
actually ships to, and @ref ref_performance carries what it bought.

@note These are a fresh measurement of the tree as it stands. The per-module deltas that used to be
here compared against a table taken before the module split, and its build settings are not recorded
anywhere, so those comparisons were dropped rather than carried forward against numbers that cannot
be reproduced.

## Speed

Cycles. `find`, `len` and `copy` are per byte; `parse` and `render` are per call. The measuring
harness is always built at -O2 so only the library moves between rows.

@warning This table is a host measurement, on x86-64, and it answers one question only: which
optimization level to hand a module. It is not a figure for how fast the library is. A desktop libc
answers the same calls with SSE or AVX, reading 16 to 48 bytes per instruction, and no target in the
list has anything of the kind - a comparison drawn here measures the vector unit. @ref ref_performance
carries the on-device counts, taken on the parts the library ships to, and those are the ones that
decide anything.

@warning The `find`, `len` and `memor.cpy` rows predate the walk rework and understate all three.
Those walks stopped rebuilding an extent mask and a lane index on every word, and the region moves
were unrolled, after this table was taken. They are left as they stand rather than guessed at: the
sweep across five levels was an ad-hoc run with no tool behind it, so there is nothing to reproduce
it with, and a number typed in by hand here would be worth less than a stale one that says where it
came from.

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
finished would be comparing two different jobs.

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
