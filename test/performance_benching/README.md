# performance_benching

Cycle-counter microbenchmarks for MMgr's hot modules, run on silicon and read against the target's
own libc. Two parts, because the library is built for both instruction sets and they answer
differently: ESP32-S3 (Xtensa LX7, 240 MHz) and ESP32-C6 (RISC-V, 160 MHz).

The harness is ProtoCore's, brought over and adapted: the same `bench.py`, the same
`bench_matrix.json` shape, the same `DBENCH_*` macros and the same `DB ` line. What changed is the
plumbing underneath - MMgr has no clock module and no platform seam, so the counter is read straight
from the part and a host build compiles the module's own sources and nothing else.

## Layout

```
bench.py            the one entry point: add / update / gen / list / deps / run
bench_matrix.json   single source of truth for what each bench compiles; never hand-edited
bench_table.py      the matrix-editing helpers bench.py uses (locking, splicing, verified writes)
common/
  device_bench.h    DBENCH_CYCLES / OP / BULK / AB / BANNER / DONE / MAIN, both arms
  host_bench.h      HBENCH_NS and the host result table
  bench_project.cmake  shared ESP-IDF project setup
common.ini          shared PlatformIO settings, for the second way to build these
<module>/           one directory per benched module
```

## Running one

```powershell
$env:IDF_PATH            = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
$env:IDF_TOOLS_PATH      = 'C:\Espressif'
$env:IDF_PYTHON_ENV_PATH = 'C:\Espressif\python_env\idf5.5_py3.14_env'
. $env:IDF_PATH\export.ps1

idf.py -C test/performance_benching/cellularum -B build_esp32s3 -D SDKCONFIG=sdkconfig.esp32s3 set-target esp32s3
ninja -C test/performance_benching/cellularum/build_esp32s3 -j 2
idf.py -C test/performance_benching/cellularum -B build_esp32s3 -p COM4 flash
```

Substitute `esp32c6` and its port for the RISC-V part. Each target keeps its own sdkconfig: the
default is one shared file, and the two do not agree on CPU frequency, so configuring one otherwise
leaves the other's build directory pointing at a config that no longer matches the part it was built
for.

**Cap the job count.** ninja defaults to cores + 2, and a full IDF tree at that width took the build
machine down.

Each image prints one `DB ` line per operation and repeats every few seconds, so a capture opened at
any time catches a whole pass.

## Two things the harness has to get right

**LTO is not optional.** The SWAR primitives are generated in `verbum_scrutor.c` and called by name
from every other module, so nothing inlines them across translation units on its own; the desktop
build sets `CMAKE_INTERPROCEDURAL_OPTIMIZATION` for that reason. ESP-IDF appends `-fno-lto` to every
link unconditionally, and appending `-flto` after it is not enough - the flag has to be taken back
out, which each bench's `main/CMakeLists.txt` does. Built without it, one four-byte word step becomes
three to five windowed calls with their argument structs spilled to the stack.

**Every timed result must be kept.** `DBENCH_KEEP` stores each one to a volatile sink. A result that
is computed and dropped is a call the optimiser may delete outright, and under LTO it does: the first
run of the dispatch measurement reported `0.00` cycles for both arms because both loops had been
removed.

## What the numbers are against

The libc side is ESP-ROM. `strnlen`, `strchr`, `memcmp`, `memchr`, `memcpy`, `memset` and `strstr`
all resolve into the part's mask ROM as hand-written assembly, executing without flash-cache
pressure, while the library executes from flash through the instruction cache. The comparison is
against that, not against a portable C libc - and not against a desktop one, which would answer the
same calls with SSE or AVX that neither part has.

Ratios are mmgr/libc, so below 1.00 is a win. Cycles per byte at n=2048 on the S3:

| module     | op   | mmgr | libc ROM | ratio |
|------------|------|------|----------|-------|
| cellularum | len  | 5.04 | 9.03 | **0.56** |
| cellularum | chr  | 4.03 | 7.02 | **0.57** |
| cellularum | cmp  | 2.02 | 2.77 | **0.73** |
| cellularum | find | 7.58 | 9.02 | **0.84** |
| memoria    | cmp  | 2.02 | 2.77 | **0.73** |
| memoria    | chr  | 3.27 | 7.02 | **0.47** |
| memoria    | cpy  | 0.646 | 0.646 | 1.00 |
| memoria    | set  | 0.333 | 0.336 | **0.99** |

`cpy` is level with ROM `memcpy` to three decimals, which is where a word-at-a-time move lands once
the loop stops costing as much as the move.
