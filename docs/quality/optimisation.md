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

| translation unit     |   -O0 |   -O1 |       -Os |   -O2 |   -O3 |
| -------------------- | ----: | ----: | --------: | ----: | ----: |
| `cellularum_laboro`  | 17536 |  8704 |  **5936** |  8736 | 11472 |
| `verba_scribo`       |  7856 |  3984 |  **3248** |  5360 |  9008 |
| `memoria_operor`     |  3968 |  1040 |   **912** |  1424 |  1728 |
| `confinium`          |  3856 |  1392 |  **1120** |  1552 |  2064 |
| `numeros_scribo`     |  3488 |  1536 |      1536 |  1536 |  1536 |
| `byteio`             |  3024 |   576 |   **496** |   720 |   736 |
| `occultum_custodiae` |  2368 |   992 |   **656** |  1136 |  1456 |
| `proximus_operor`    |  1584 |   320 |   **224** |   336 |   816 |
| `spatium`            |  1040 |   464 |   **400** |   464 |   464 |
| `clarus_custodiae`   |  1456 |   688 |   **528** |   816 |  1008 |
| `fractio`            |   960 |   192 |       192 |   240 |   240 |
| `endian`             |   848 |   336 |   **192** |   320 |   352 |
| `bitio`              |   416 |   176 |   **160** |   208 |   208 |
| **total**            | 48400 | 20400 | **15600** | 22848 | 31088 |

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

**-O1 to -O2 is where the speed is.** `memor.cpy` goes 0.703 to 0.258, a 2.7 times step, for 4,800
bytes across the library. That is the one jump that clearly pays.

**-O2 to -O3 buys about 2% on the scans for 8,240 bytes.** `find` moves 0.985 to 0.974. `len` and
`copy` do not move at all. The scanning entries are already the shape they want to be: there is no
loop left for -O3 to unroll into something better, so it inlines and unrolls anyway and the code
gets bigger for nothing.

The exception is `to_double` at -14%, which is real - it has an actual loop over the table.

**-Os is not uniformly slow.** It is the smallest by a distance, 32% under -O2, and `len` is the
fastest of any level there while `copy` is within 1% of -O2. What falls off a cliff is `verba.g`,
at 1073 against 453 - worse than -O1.

## Where a module names its own level

`mmgr_add_module` takes `OPTIMIZE`, which appends after the build's own flags. The last `-O` on the
command line is the one that counts, so nothing has to be removed for it to take effect.

| module            | level | why                                                                                                                                                      |
| ----------------- | ----- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `verba_scribo`    | -O2   | -O3 costs 3648 bytes, 68% more than the whole module at -O2, for 1.5% on a render. The digit loops are short and already shaped.                         |
| `proximus_operor` | -O2   | -O3 more than doubles it, 336 bytes to 816, and moves nothing measurable. The entries are single loads and stores that are already one instruction each. |

`cellularum_laboro` is left at the build's level: -O3 costs it 2,736 bytes and returns 14% on
`to_double`, which is a real workload rather than a microbenchmark artefact.

## The honest caveat

These are one compiler on one target - gcc 13.2 on x86-64. The shape of the argument holds
elsewhere: code that is already minimal does not benefit from being unrolled. The specific numbers
do not. Rerun the tool on the target that matters before quoting them at anybody.
