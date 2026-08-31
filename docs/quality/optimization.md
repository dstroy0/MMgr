# What each optimization level costs {#qa_optimization}

One optimization level for a whole library is a guess that suits some of it. This page is the
measurement, so a module that names its own level has a reason on record rather than a preference.

Reproduce it with:

```
python tools/dev_env/target_sizes.py
```

That compiles each unit with the toolchains the library ships to - 32-bit Xtensa, RISC-V and ARM - at
each level, on its own, no LTO and no link. Sections are read rather than file lengths, because an
object also carries relocations, symbol tables and debug records that never get flashed.

@warning `tools/dev_env/sizes.py` measures the same thing on the host. It reads its options out of
`build/compile_commands.json`, so its figures are x86-64 code size, and this page used to be written
from them. They do not merely differ from the target by a constant: on the host `-O1` comes in under
`-O2`, and on **all three** targets it comes in over. A level picked off the host table is picked off
the wrong machine. Keep sizes.py for what it is good for - checking a change did not blow a unit up
while you are working on the desktop - and decide with the table below.

## Size

`.text`, in bytes, per translation unit. Each unit compiled on its own, no LTO and no link, so a row
is that unit alone:

```
<target>-gcc -std=c11 -I src -O<level> -c <unit>.c -o <unit>.o && <target>-size <unit>.o
```

ARM is measured `-mcpu=cortex-m4 -mthumb`, because `arm-none-eabi-gcc` otherwise defaults to a core
nothing in the target list is, and the wide ARM encoding reports a code size no target flashes.

Xtensa (esp32s3, gcc 14.2):

| translation unit           |    -O0 |   -O1 |       -Os |   -O2 |   -O3 |
| -------------------------- | -----: | ----: | --------: | ----: | ----: |
| `verba_scribo`             |  35026 | 10438 |  **6359** |  9078 | 12966 |
| `cellularum_laboro`        |  18786 |  9633 |  **7228** |  8550 |  9330 |
| `transformo`               |  17473 |  3594 |  **2898** |  3794 |  5936 |
| `memoria_anularis`         |  10404 |  3411 |  **3279** |  3391 |  3439 |
| `memoria_operor`           |   2707 |  1394 |  **1182** |  1294 |  1350 |
| `verbum_scrutor`           |   3856 |  1144 |  **1144** |  1152 |  1152 |
| `numeros_scribo`           |   2265 |  1000 |   **892** |   935 |   935 |
| `octetus_introitus_exitus` |   3102 |   851 |   **749** |   854 |   858 |
| `locus_carcerum`           |   3703 |   870 |   **778** |   834 |   878 |
| `endian`                   |   2341 |   729 |   **413** |   657 |   657 |
| `clz`                      |   1740 |   502 |   **244** |   502 |   502 |
| `proximus_operor`          |   1332 |   492 |   **475** |   492 |   614 |
| `bitorum_introitus_exitus` |    798 |   344 |   **268** |   344 |   340 |
| `spatium`                  |   1611 |   298 |       306 |   302 |   302 |
| `impensa_ancorae_acus_*`   |    302 |   276 |       276 |   276 |   276 |
| `ascii_persona_bitorum`    |    276 |   212 |       212 |   212 |   212 |
| `fractio`                  |    522 |   101 |       101 |   101 |   101 |
| **total**                  | 106244 | 35289 | **26804** | 32768 | 39848 |

The other two, totals only - the shape is the same on all three:

| target                        |    -O0 |   -O1 |       -Os |   -O2 |   -O3 |
| ----------------------------- | -----: | ----: | --------: | ----: | ----: |
| Xtensa (esp32s3)              | 106244 | 35289 | **26804** | 32768 | 39848 |
| RISC-V (esp32c6)              | 116040 | 38904 | **30144** | 35970 | 42572 |
| ARM (cortex-m4, thumb)        | 100138 | 30014 | **25534** | 28418 | 34690 |
| _host (x86-64), for contrast_ | 108592 | 37696 |     30768 | 38992 | 44320 |

`memoria_externa` and `memoriam_praetereo` compile to nothing and are left out. The five
`impensa_ancorae_acus_*` units are one row because they are alternatives, not additions — a build
links exactly one cost table and they all define the same symbol. The total counts one.

**-Os is the smallest, and -O1 is not second.** On all three targets -O1 lands _above_ -O2 — by 2521
bytes on Xtensa, 2934 on RISC-V and 1596 on ARM — so the order is -Os, -O2, -O1, -O3. A build that
wants small and does not want to think about it should ask for -Os and stop there; there is no reason
to reach for -O1. -O3 costs 7080 bytes over -O2 on Xtensa, and @ref ref_performance is where to look
before paying it.

@note That ordering is the reason this page is measured on the targets. The host row is in the table
only to show the disagreement: it puts -O1 under -O2, which reads as a sensible ladder and is not the
one any shipping target walks. Three instruction sets agree with each other and none of them agrees
with the desktop.

Two units carry most of it. `verba_scribo` and `cellularum_laboro` are half the total at every level,
which is what a decimal engine and a string module cost. `verba_scribo` is also the one unit where
the size is buying correctness rather than speed: both of its render entries were wrong before it
inlined that engine — `verba.fixed` by 15.87% below about 1e-41, and `verba.g` failing to name its
own value back 87.07% of the time. Both are 0.0000% now. See @ref qa_numeric.

`transformo` has the widest spread that matters, 2898 at -Os against 5936 at -O3 on Xtensa. It is the
unit to look at first if a target is tight and -O3 is on.

`cellularum_laboro` and `memoria_operor` are the two units the on-device work changed, and both grew
for it: 6742 to 8550 on Xtensa and 1246 to 1294. The scans stopped rebuilding an extent mask and a
lane index on every word, they read through the aligned load rather than one assembled from byte
loads, `len` takes two words a pass, a one or two byte needle is settled by a mask chain instead of a
sieve, and the region moves were unrolled to four words. @ref ref_performance carries what each of
those bought, measured on the parts themselves - `len` went 5.040 cycles per byte to 2.547.

Whether that trade is right is a judgement about the target rather than a fact. The section below on
newlib is where it shows up as flash.

@note These are a fresh measurement of the tree as it stands. The per-module deltas that used to be
here compared against a table taken before the module split, and its build settings are not recorded
anywhere, so those comparisons were dropped rather than carried forward against numbers that cannot
be reproduced.

## Speed

There is no speed table on this page any more, and that is deliberate.

The one that used to be here was a host measurement, taken on x86-64, and it is what sent an entire
round of optimization work down the wrong road: it made the scans look within a few percent of libc,
so nobody looked, while on the parts this library ships to they were three to sixteen times off. A
desktop libc answers `strlen`, `memcmp` and `memchr` with SSE or AVX, reading 16 to 48 bytes per
instruction. No target in the list has anything of the kind. A comparison drawn on the host measures
the vector unit and reports it as a fact about this library.

Speed lives in @ref ref_performance, measured on silicon against the target's own libc, and nowhere
else. If a number here would have changed a decision, it belongs there instead.

Size is still measured here because size is a property of the emitted code and can be read straight
off an object file for each target - which is what the table above does.

## Where a module names its own level

`mmgr_add_module` takes `OPTIMIZE`, which appends after the build's own flags. The last `-O` on the
command line is the one that counts, so nothing has to be removed for it to take effect.

Costs below are Xtensa, with ARM in parentheses where the two disagree enough to matter.

| module            | level | why                                                                                                                                              |
| ----------------- | ----- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `verba_scribo`    | -O2   | -O3 costs 3888 bytes, 43% more than the whole module at -O2, and 5700 on ARM, which is 75%. The digit loops are short and already shaped.        |
| `transformo`      | -O2   | -O3 costs 2142 bytes, 56% more than the module. On ARM it costs nothing at all, which is the clearest case on this page for deciding per target. |
| `proximus_operor` | -O2   | -O3 adds 122 bytes to 492 for entries that are single loads and stores, already one instruction each. Proportionally worse on ARM, 188 to 308.   |

`cellularum_laboro` is left at the build's level: -O3 costs it 780 bytes on Xtensa and 356 on ARM,
against a module of 8550 and 7760, which is not enough either way to override the build.

@note These are size arguments, and only size arguments. The speed half of each of these rows used to
cite the host bench, and one of them credited `cellularum_laboro` with 14% on `to_double`, which is
not even its entry. Whether a level is worth it for speed is a question for @ref ref_performance, on
the part in question - there is no per-level sweep on silicon yet, so no such claim is made here.

## Against the libc it would replace

`tools/dev_env/against_libc.py` weighs this library against newlib for the same core at the same
level. newlib because it is the libc an embedded target actually ships, it is a static archive with
one object per entry so a single function can be weighed, and it is built by the same compiler
family. Only `.text` is counted: an archive member also carries relocations and symbol tables that
never reach flash, and the linker pulls whole members, so a member is the unit whether or not every
entry in it is called.

cortex-m4, -Os, newlib from armv7e-m:

| family                     |      MMgr |    newlib |           |
| -------------------------- | --------: | --------: | --------: |
| moving and comparing bytes |      1368 |       924 | **0.68x** |
| searching and parsing text |      7734 |     15816 |     2.04x |
| rendering numbers and text |      7326 |     21364 |     2.92x |
| **total**                  | **16428** | **38104** | **2.32x** |

**21,676 bytes of flash**, for the same set of jobs.

@warning Moving and comparing bytes is _larger_ than newlib's, 1368 against 924, where before the
on-device work it was 736 and smaller. Searching and parsing text has come down from 3.11x to 2.04x
over the same stretch. Both are the same trade: `memor.cpy` and `memor.set` moved one word an
iteration and lost to ROM `memcpy` by half again, so both were unrolled; the scans stopped rebuilding
masks per word, took the aligned load, and gained a mask chain for short needles. What it bought is
in @ref ref_performance - `cellul.len` at 0.28 against ROM `strnlen`, `memor.chr` at 0.47, `cpy` and
`set` level with ROM assembly. Whether roughly 3,350 bytes is the right price is a judgement about
the target, not a fact, and it is the number on this page most worth arguing with.

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

Parsing is 2.04x because newlib spends 10,272 bytes on `strtod` + `dtoa` + `mprec`, and `mprec` is
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

Rendering is 2.92x and the `FILE` members are counted, which needs saying plainly: newlib's
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

| entry              |     MMgr | glibc               |           |
| ------------------ | -------: | ------------------- | --------: |
| `cellul.to_double` | 179.2 ns | `strtod` 292.1 ns   | **1.63x** |
| `verba.g`          | 397.5 ns | `snprintf` 954.2 ns | **2.40x** |

Correctness on the same run, because a speed number for a wrong answer is not a number: `to_double`
differs from glibc's `strtod` on **0 of 2000** random bit patterns, and `verba.g` rendered at
seventeen digits and read back by glibc names its own value on **0 of 2000**.

So the comparison is identical accuracy, fewer bytes, and fewer cycles, on both directions of the
conversion. Not a trade.

Two things that number is not. It is one host. And the parser has not had the treatment the rest of
the library has had:
`cellularum_laboro` is still a single translation unit holding the scans and the decimal parser
together, and it is the one module that has not been cut back to the shape the others are in. The
1.63x is what it does before that, not after.

## The honest caveat

These are one compiler on one target - gcc 13.2 on x86-64. The shape of the argument holds
elsewhere: code that is already minimal does not benefit from being unrolled. The specific numbers
do not. Rerun the tool on the target that matters before quoting them at anybody.
