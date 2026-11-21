# What each optimization level costs {#qa_optimization}

One optimization level for a whole library is a guess that suits some of it. This page is the
measurement. A module that names its own level has a reason on record and not a preference.

Reproduce it with:

```
python tools/dev_env/target_sizes.py
```

That compiles each unit at each level, on its own, with no LTO and no link. It uses the toolchains the
library ships to: 32-bit Xtensa, RISC-V and ARM. The tool reads sections and not file lengths. An
object also carries relocations, symbol tables and debug records that never get flashed.

@warning `tools/dev_env/sizes.py` measures the same thing on the host. It reads its options out of
`build/compile_commands.json`, and so its figures are x86-64 code size. They differ from the target by
more than a constant: on the host `-O1` comes in under `-O2`, and on **all three** targets it comes
in over. A level picked off the host table comes from the wrong machine. Retain sizes.py for a
quick check that a change does not blow a unit up while you work on the desktop. Decide with the
table below.

## Size

`.text`, in bytes, per translation unit. Each unit compiles on its own, with no LTO and no link. A
row is that unit alone:

```
<target>-gcc -std=c11 -I src -O<level> -c <unit>.c -o <unit>.o && <target>-size <unit>.o
```

The ARM column uses `-mcpu=cortex-m4 -mthumb`. Without it, `arm-none-eabi-gcc` defaults to a core
that nothing in the target list is, and the wide ARM encoding reports a code size no target flashes.

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

`memoria_externa` and `memoriam_praetereo` compile to nothing, and the table leaves them out. The five
`impensa_ancorae_acus_*` units are one row because they are alternatives and not additions. A build
links exactly one cost table, and they all define the same symbol. The total counts one.

**-Os is the smallest, and -O1 is not second.** On all three targets -O1 lands _above_ -O2: by 2521
bytes on Xtensa, 2934 on RISC-V and 1596 on ARM. So the order is -Os, -O2, -O1, -O3. A build that
wants small with no further thought should ask for -Os and stop there; there is no reason to reach for
-O1. On Xtensa, -O3 costs 7080 bytes over -O2, and @ref ref_performance is where to look before you
pay it.

@note That order is the reason this page measures on the targets. The host row is in the table only
to show the disagreement. It puts -O1 under -O2, which reads as a sensible ladder and is not the one
any target we ship to walks. Three instruction sets agree with each other, and not one of them agrees
with the desktop.

Two units carry the majority of it. `verba_scribo` and `cellularum_laboro` are half the total at
every level, as a decimal engine and a string module cost. `verba_scribo` is also the one unit where
the size buys correctness and not speed. Without the engine inlined, `verba.fixed` is wrong for
15.87% of values below about 1e-41, and `verba.g` fails to name its own value back 87.07% of the
time. With it, both are wrong for 0.0000%. See @ref qa_numeric.

`transformo` has the widest spread that matters, 2898 at -Os against 5936 at -O3 on Xtensa. It is the
unit to look at first if a target is tight and -O3 is on.

`cellularum_laboro` and `memoria_operor` carry the rules that the on-device measurements set, and both
pay for them: 8550 against 6742 on Xtensa, and 1294 against 1246. The scans build no extent mask and
no lane index on every word. They read through the aligned load and not one assembled from byte loads.
`len` takes two words a pass, and a mask chain settles a one or two byte needle in place of a sieve.
The region moves run four words an iteration.

@ref ref_performance carries what each of those buys, measured on the parts themselves: `len` costs
2.547 cycles per byte against 5.040 without them. Whether that trade is right is a judgement about
the target and not a fact. The section below on newlib is where it shows up as flash.

@note These figures measure the tree as it is. No comparison here rests on a table whose build
settings nobody recorded.

## Speed

There is no speed table on this page, by design.

A host measurement, taken on x86-64, does not belong here, because it misleads every round of
optimization work. It puts the scans within a few percent of libc, while on the parts this library
ships to they are three to sixteen times off. A desktop libc answers `strlen`, `memcmp` and `memchr`
with SSE or AVX, and reads 16 to 48 bytes per instruction. No target in the list has anything of the
type. A comparison drawn on the host measures the vector unit and reports it as a fact about this
library.

Speed lives in @ref ref_performance, measured on silicon against the target's own libc, and nowhere
else. If a number here would change a decision, it belongs there instead.

Size is still on this page because size is a property of the emitted code. You can read it straight
off an object file for each target, as the table above does.

## Where a module names its own level

`mmgr_add_module` takes `OPTIMIZE`, which appends after the build's own flags. The last `-O` on the
command line is the single one that counts, and so nothing has to come out for it to take effect.

Costs below are Xtensa, with ARM in parentheses where the two disagree enough to matter.

| module            | level | why                                                                                                                                            |
| ----------------- | ----- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `verba_scribo`    | -O2   | -O3 costs 3888 bytes, 43% more than the whole module at -O2, and 5700 on ARM, which is 75%. The digit loops are short and already shaped.      |
| `transformo`      | -O2   | -O3 costs 2142 bytes, 56% more than the module. On ARM it costs nothing at all, the clearest case on this page for deciding per target.        |
| `proximus_operor` | -O2   | -O3 adds 122 bytes to 492 for entries that are single loads and stores, already one instruction each. Proportionally worse on ARM, 188 to 308. |

`cellularum_laboro` stays at the build's level. On Xtensa, -O3 costs it 780 bytes, and on ARM 356,
against a module of 8550 and 7760. That is not enough either way to override the build.

@note These are size arguments, and only size arguments. Whether a level is worth it for speed is a
question for @ref ref_performance, on the part in question. There is no per-level sweep on silicon,
and so this page makes no such claim.

## Against the libc it would replace

`tools/dev_env/against_libc.py` weighs this library against newlib for the same core at the same
level. newlib is the libc an embedded target actually ships. It is a static archive with one object
per entry, so each function has its own weight. The same compiler family builds it.

The tool counts only `.text`. An archive member also carries relocations and symbol tables that never
reach flash, and the linker pulls whole members. A member is the unit regardless of whether every
entry in it gets a call.

cortex-m4, -Os, newlib from armv7e-m:

| family                     |      MMgr |    newlib |           |
| -------------------------- | --------: | --------: | --------: |
| moving and comparing bytes |      1368 |       924 | **0.68x** |
| searching and parsing text |      7734 |     15816 |     2.04x |
| rendering numbers and text |      7326 |     21364 |     2.92x |
| **total**                  | **16428** | **38104** | **2.32x** |

**21,676 bytes of flash**, for the same set of jobs.

@warning The byte moves and compares are _larger_ than newlib's, 1368 against 924. That is the price
of the on-device rules. `memor.cpy` and `memor.set` move four words an iteration, because at one word
they lose to ROM `memcpy` by half again. The scans build no masks per word, take the aligned load, and
carry a mask chain for short needles.

What those rules buy is in @ref ref_performance: `cellul.len` at 0.28 against ROM `strnlen`,
`memor.chr` at 0.47, and `cpy` and `set` level with ROM assembly. Whether roughly 3,350 bytes is the
right price is a judgement about the target, not a fact. It is the number on this page max worth an
argument.

Every entry on both sides has a bound. So the libc column names `strnlen` and `strncmp` and not their
unbounded twins, and `snprintf` and `vsnprintf` and not `printf`. A bounded string constructor is what
`verba` is. A comparison against something that writes until it finishes would compare two different
jobs.

The families follow where the code actually is and not where the header names suggest.
`cellularum_laboro` holds the bounded scans and the decimal parser in one translation unit. On the
newlib side that is `str*` plus the whole `strtod`, `dtoa` and `mprec` apparatus. The table names each
newlib member once across all three families. `dtoa` and `mprec` serve both directions there, and a
second count of them would flatter this library by six kilobytes.

Where the gap comes from deserves a precise statement.

The parse family is 2.04x because newlib spends 10,272 bytes on `strtod` + `dtoa` + `mprec`, and
`mprec` is an arbitrary-precision bignum. It is exact for every input because it carries however many
limbs the input needs. This library is exact for every input with 128 bits that never grow. 128 bits
are enough to decide a round, and nothing looks at the rest of the expansion. That is a real difference
in approach and not a trick of the count.

So the obvious question is why the other side does not do
the same. Three answers, and the third matters here.

**The result is newer than the code.** newlib's is David Gay's `dtoa` and `mprec`, which date from
1990 and are the reference implementation everyone inherited. A fixed-width intermediate always
suffices: you need enough bits to decide guard, round and sticky, and never the full expansion. That
result is Grisu in 2010, Ryu in 2018, Eisel-Lemire in 2020. Gay's code is not naive; it is correct, and
it predates the result that makes it unnecessary.

**A bignum serves every width with one body.** `mprec` is exact for `double`, `long double` and
whatever else the target has, because limbs do not care. The table here is 360 bytes of powers of five
sized for binary64, alone. libc has to answer for all of them. This library answers for one, and buys
the difference with a narrower promise.

**And a bignum has to put its limbs somewhere.** `mprec.o` in this newlib carries undefined references
to `malloc` and `_calloc_r`. On a target with a heap that is a cost. On a target without one it is not
a trade at all: the entry does not link. So the table is not a smaller way to do what libc does. It is
the only way available to a library that must not allocate, and it also happens to be smaller
(see @ref qa_numeric).

The render family is 2.92x, and the table counts the `FILE` members. That needs a plain statement:
newlib's `snprintf` sits on its `FILE` machinery. It constructs a fake stream over the caller's buffer
and goes through `vfprintf`. So a link of `snprintf` links `fvwrite`, `findfp`, `fflush`, `makebuf` and
`wsetup`, whether or not a stream ever opens.

A caller pays this for the entry they called, and so the table counts it. The reason is not that a
print to a stream is a wider job. The rest of the gap is the format string. The newlib code parses
one at run time, and `verba` has none, because a call names the entry it wants.

Nothing stops a file layer on top of this library once the pieces below it are correct. That
is a separate concern and not what this table is about.

The byte family is 0.68x. There is not much room in `memcpy` against `memcpy`, and the warning above
says where these bytes go.

### And the speed, on a libc that can be measured

The size figures come from a cross-compile for a cortex-m4, which cannot run here. The speed figures
come from the host against glibc, a different target and a different libc. So the two tables are about
different things, and neither transfers to the other.

gcc 13.3, glibc 2.39, x86-64. Same corpus, same order, best of fifteen:

| entry              |     MMgr | glibc               |           |
| ------------------ | -------: | ------------------- | --------: |
| `cellul.to_double` | 179.2 ns | `strtod` 292.1 ns   | **1.63x** |
| `verba.g`          | 397.5 ns | `snprintf` 954.2 ns | **2.40x** |

Correctness on the same run, because a speed number for a wrong answer is not a number. `to_double`
differs from glibc's `strtod` on **0 of 2000** random bit patterns. `verba.g` rendered at seventeen
digits and read back by glibc names its own value on **0 of 2000**.

So the comparison is identical accuracy, fewer bytes, and fewer cycles, on both directions of the
conversion. Not a trade.

Two things that number is not. It is one host. And `cellularum_laboro` is still a single translation
unit that holds the scans and the decimal parser together. It is the single one module not yet in the
shape the others are in, and the 1.63x is its figure in that shape.

## The caveat

Each target here is one compiler version. The shape of the argument holds elsewhere: code that is
already minimal does not gain from an unroll. The specific numbers do not. Run the tool again on the
target that matters before you quote them to anybody.
