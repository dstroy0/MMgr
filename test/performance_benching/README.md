# performance_benching

Cycle-counter microbenchmarks for MMgr's hot modules, run on silicon and read against the target's
own libc. Two parts, because the library builds for both instruction sets and they answer
differently: ESP32-S3 (Xtensa LX7, 240 MHz) and ESP32-C6 (RISC-V, 160 MHz).

The harness follows ProtoCore's: the same `bench.py`, the same `bench_matrix.json` shape, the same
`DBENCH_*` macros and the same `DB ` line. Under them, MMgr has no clock module and no platform seam.
So the bench reads the counter straight from the part, and a host build compiles the module's own
sources, alone.

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

Substitute `esp32c6` and its port for the RISC-V part. Each target retains its own sdkconfig. The
default is one shared file, and the two parts do not agree on CPU frequency. With one file, a
configure of one part leaves the other's build directory on a config that does not match its part.

**Cap the job count.** ninja defaults to cores + 2, and a full IDF tree at that width can take the
build machine down.

Each image prints one `DB ` line per operation and repeats every few seconds. A capture opened at
any time catches a whole pass.

## Two things the harness has to get right

**LTO is not optional.** `verbum_scrutor.c` generates the SWAR primitives, and every other module
calls them by name. So nothing inlines them across translation units on its own; the desktop build
sets `CMAKE_INTERPROCEDURAL_OPTIMIZATION` for that reason. ESP-IDF adds `-fno-lto` to every link
unconditionally, and `-flto` after it is not enough. The flag has to come back out, and each bench's
`main/CMakeLists.txt` takes it out. Built without it, one four-byte word step becomes three to five
windowed calls with their argument structs spilled to the stack.

**Retain every timed result.** `DBENCH_RETAIN` stores each one to a volatile sink. The optimizer may
delete outright a call whose result nobody stores, and under LTO it does. Both arms of a dispatch
measurement then report `0.00` cycles, because the optimizer deletes both loops.

## What the numbers are against

The libc side is ESP-ROM. `strnlen`, `strchr`, `memcmp`, `memchr`, `memcpy`, `memset` and `strstr`
all resolve into the part's mask ROM as hand-written assembly. That code runs with no flash-cache
pressure, while the library runs from flash through the instruction cache. The comparison is against
that, not against a portable C libc. It is also not against a desktop one, which would answer the
same calls with SSE or AVX that neither part has.

Ratios are mmgr/libc, and so below 1.00 is a win. Cycles per byte at n=2048:

| module     | op   | S3 mmgr | S3 libc |       S3 | C6 mmgr | C6 libc |       C6 |
| ---------- | ---- | ------: | ------: | -------: | ------: | ------: | -------: |
| cellularum | len  |   2.547 |   9.024 | **0.28** |   2.292 |   9.016 | **0.25** |
| cellularum | chr  |   4.274 |   7.021 | **0.61** |   3.775 |   9.018 | **0.42** |
| cellularum | cmp  |   2.020 |   2.773 | **0.73** |   1.765 |   2.137 | **0.83** |
| cellularum | find |   6.543 |   9.021 | **0.73** |   6.290 |   7.019 | **0.90** |
| memoria    | cmp  |   2.019 |   2.774 | **0.73** |   1.763 |   2.137 | **0.82** |
| memoria    | chr  |   3.270 |   7.020 | **0.47** |   2.765 |  12.017 | **0.23** |
| memoria    | cpy  |   0.646 |   0.646 |     1.00 |   0.705 |   0.701 |     1.01 |
| memoria    | set  |   0.333 |   0.336 | **0.99** |   0.391 |   0.394 | **0.99** |

`cpy` is level with ROM `memcpy`. That is where a word-at-a-time move lands against hand-written
assembly, once the loop no longer costs as much as the move. Everything else is ahead of the part's own
libc at length.

## Subtract the floor before reading a short length

Both benches report `floor_loop` and `floor_call`: the loop, counter and volatile store with nothing
in them, and the same plus one call the optimizer may not remove.

| part     | loop alone | plus one call |
| -------- | ---------: | ------------: |
| ESP32-S3 |        6.0 |          41.0 |
| ESP32-C6 |        3.0 |          28.0 |

Both arms pay it. `len` over eight bytes is 112 cycles on the S3 against ROM `strnlen`'s 113, and 41
of each is the call. A ratio at n=8 is mostly two call floors.

`dispatch_len8` and `direct_len8` price `EMBED_CALL` itself: a call through the namespace table
against a call of the entry by name. They come out identical - 112.02 both ways on the S3, 102.06 on
the C6. So the compound literal folds into registers, and the code builds no argument struct.
