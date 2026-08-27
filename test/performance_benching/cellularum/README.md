# cellularum on-device bench

Cycle-counter A/B of the string entries against the target's own libc, on silicon. Two parts,
because the library is built for both instruction sets and they answer differently: ESP32-S3
(Xtensa LX7, 240 MHz) and ESP32-C6 (RISC-V, 160 MHz).

## Build and run

The install used here is ESP-IDF 5.5.5 under `C:\Espressif`, whose virtualenv is named for Python
3.14. `export.ps1` derives both locations from the environment, so both have to be set:

```powershell
$env:IDF_PATH          = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
$env:IDF_TOOLS_PATH    = 'C:\Espressif'
$env:IDF_PYTHON_ENV_PATH = 'C:\Espressif\python_env\idf5.5_py3.14_env'
. $env:IDF_PATH\export.ps1

idf.py -B build_esp32s3 -D SDKCONFIG=sdkconfig.esp32s3 set-target esp32s3
ninja -C build_esp32s3 -j 2
idf.py -B build_esp32s3 -p COM4 flash
```

Substitute `esp32c6` and its port for the RISC-V part. Each target keeps its own sdkconfig: the
default is one shared file, and the two do not agree on CPU frequency, so configuring one leaves the
other's build directory pointing at a config that no longer matches the part it was built for.

Cap the job count. ninja defaults to cores + 2, and a full IDF tree at that width took this machine
down.

The image prints one `DB ` line per operation and repeats every five seconds, so a capture opened at
any time catches a whole pass.

## LTO is not optional

The bench compiles this component with `-flto`, and removes the `-fno-lto` that ESP-IDF appends to
every link. Without that it measures something the library never does in a real build.

The SWAR primitives are generated in `verbum_scrutor.c` by `GENERIC_ENTRY` and called by name from
every other module. Nothing can inline them across translation units on its own, and the desktop
build sets `CMAKE_INTERPROCEDURAL_OPTIMIZATION` for exactly that reason. Built without it, one
four-byte word step becomes three to five windowed calls, each with its argument struct spilled to
the stack first:

```
420093fd: call8 <mmgr_scrut_words>
4200941a: call8 <mmgr_scrut_load>
42009425: call8 <mmgr_scrut_has_zero>
42009435: call8 <mmgr_scrut_tail_mask>
42009449: call8 <mmgr_scrut_lane_lo>
```

Measured on the S3, `len` at n=2048: **26.6 cycles/byte without LTO, 5.0 with**. The no-LTO figure is
worse than newlib's byte-at-a-time `strnlen` at 9.0.

## What the walks stopped doing

The scan loops used to compute two things on every word that can only matter on one word of a scan,
and that, not the ROM assembly on the other side, was the whole gap against libc:

- **mask.tail per word.** cap is known before the loop, so lanes past it can only fall in the last
  word. The walks now run whole words with no mask and take the short word once, below the loop.
- **Which lane differs, per word.** `cmp` resolved the differing lane on every word when the common
  case only needs to know whether two words differ at all - one compare. The lane is resolved once,
  after the loop finds the word that differs. `chr` likewise applies mask.before once, on the word
  that carried a hit or a terminator.

`find` had a third: `lane.eq` rebuilds its broadcast from a byte on every call, and LTO left it as
an out-of-line call in the hot loop. The sieve's bytes are fixed for the walk, so they are broadcast
once ahead of it and compared inline.

Per 4-byte word, `cmp` went from ~18 instructions to ~6, which is what a word-wise memcmp does.
Cycles per byte at n=2048 on the S3:

| op | before | after | libc ROM |
|------|--------|-------|----------|
| len  | 8.04 | **5.04** | 9.03 |
| chr  | 15.30 | **4.03** | 7.02 |
| cmp  | 5.77 | **2.02** | 2.77 |
| find | 11.32 | **7.58** | 9.02 |

The module's .text grew 264 bytes for it, 6506 to 6770 at -O2 on Xtensa.

## Every timed result must be kept

`DBENCH_KEEP` stores each result to a volatile sink. A result that is computed and dropped is a call
the optimiser may delete outright, and under LTO it does: the first run of the dispatch measurement
reported `0.00` cycles for both arms because both loops had been removed.

## Results

Raw captures are in `results/`. Ratios are mmgr/libc, so below 1.00 is a win.

| op | S3 n=8 | S3 n=2048 | C6 n=8 | C6 n=2048 |
|------|--------|-----------|--------|-----------|
| len  | 1.04 | **0.56** | 1.20 | **0.53** |
| chr  | 1.06 | **0.57** | 1.01 | **0.42** |
| cmp  | **0.83** | **0.73** | 1.09 | **0.83** |
| find | 1.92 | **0.84** | 1.95 | **0.79** |

Every entry beats libc once the buffer is a few words long. `len`, `chr` and `cmp` cross over at
n=16; `find` at n=128 on the S3 and n=64 on the C6.

What remains above 1.00 is at n=8, and it is fixed cost rather than per-byte work: one entry call
that is not inlined into the caller, plus, for `find`, building the sieve. `find` runs
cellul_pick_rows over the needle against the cost table before a haystack byte is read, and over one
word that is most of the measurement. Routing short haystacks to the byte walk instead was tried and
is worse - the byte walk goes through cellul_step_byte per byte and costs more than the single
sieved word it replaces.

The libc side is ESP-ROM code (`strnlen` at `0x400013f8`, `memcmp` at `0x4000120c`), hand-written
assembly executing from ROM. The library executes from flash through the instruction cache. The
comparison is against that, not against a portable C libc.

### Dispatch costs nothing

`MMGR_CALL` was measured against a direct call to the same entry, same work, same buffer:

| target | dispatch_len8 | direct_len8 |
|--------|---------------|-------------|
| ESP32-S3 (Xtensa) | 140.09 | 140.08 |
| ESP32-C6 (RISC-V) | 147.06 | 147.06 |

Identical on both. The compound literal folds into registers and no argument struct is materialized.
This is worth stating because it does not hold everywhere: on Cortex-M4, `MMGR_CALL` emitted a
`memset` of the whole argument type per call. Neither of these parts does that.
