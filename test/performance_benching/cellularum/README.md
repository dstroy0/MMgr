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

Measured on the S3, `len` at n=2048 when that was taken: **26.6 cycles/byte without LTO, 5.0 with**.
The no-LTO figure is worse than the byte-at-a-time ROM `strnlen` it is being compared to, at 9.0.
`len` is 2.547 now, so the gap is wider than that pair records; it is left as taken rather than
scaled by guesswork.

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

Three more changes followed, each measured on the part:

- **The aligned load.** Every word was read through `word.load`, which goes to a type carrying
  `MMGR_ALIGN(1)`. Neither shipping part has an unaligned word load, so the compiler assembled one
  out of four byte loads and six shifts, in the loop. The walks now step to the first word boundary
  and read the body through `word.load_al`. `len` went 3.788 cycles/byte to 3.055 on that alone.
- **Two words a pass in `len`.** The load and the `has_zero` reading it are a dependent pair and
  neither part issues them back to back without stalling. 3.055 to 2.547. Tried on `chr` and
  `memor.chr` and lost on both - they already carry enough arithmetic to cover the load.
- **A mask chain for one and two byte needles.** `find` was building the anchor-and-verify sieve for
  a needle with no rare byte to anchor on. 11.32 cycles/byte to 6.543.

Cycles per byte at n=2048 on the S3, start of the work to now:

| op   | before | after     | libc ROM |
| ---- | ------ | --------- | -------- |
| len  | 8.04   | **2.547** | 9.024    |
| chr  | 15.30  | **4.274** | 7.021    |
| cmp  | 5.77   | **2.020** | 2.773    |
| find | 11.32  | **6.543** | 9.021    |

The module's .text grew from 6506 to 8550 at -O2 on Xtensa for all of it.

## Every timed result must be kept

`DBENCH_KEEP` stores each result to a volatile sink. A result that is computed and dropped is a call
the optimiser may delete outright, and under LTO it does: the first run of the dispatch measurement
reported `0.00` cycles for both arms because both loops had been removed.

## Results

Raw captures are in `results/`. Ratios are mmgr/libc, so below 1.00 is a win.

| op       | S3 n=8   | S3 n=2048 | C6 n=8   | C6 n=2048 |
| -------- | -------- | --------- | -------- | --------- |
| len      | **0.99** | **0.28**  | 1.11     | **0.25**  |
| chr      | **0.86** | **0.61**  | **0.81** | **0.42**  |
| cmp      | **0.83** | **0.73**  | 1.07     | **0.83**  |
| find     | 1.26     | **0.73**  | 1.45     | **0.90**  |
| find_hot | 1.11     | **0.66**  | 1.29     | **0.81**  |

Every entry beats libc once the buffer is a few words long. `find` crosses over at n=32 on the S3
with the easy needle and n=16 with the hostile one.

`find_hot` searches for a needle whose first byte turns up every fifteen bytes and whose pair never
occurs; `find` searches for one whose first byte is not in the haystack's alphabet at all. MMgr
costs the same either way - 13400.7 cycles at n=2048 against 13400.8 - because the chain settles
every start position in a word arithmetically. ROM `strstr` does not: 18474 against 20394. That is
why the chain keeps its third `has_zero` rather than anchoring on one byte and verifying the other,
which would be about 20% cheaper and would buy it by becoming data dependent exactly where libc
already is.

What remains above 1.00 is at n=8, and it is fixed cost rather than per-byte work. Subtract the
floor first: `floor_call` is 41 cycles on the S3 and 28 on the C6, and both arms pay it. For `find`
the rest is prologue - two broadcasts, a span, a reach and a word count settled before the first byte
is read.

Routing short haystacks past the word machinery to the byte walk was tried twice and lost twice, for
two different reasons, and both are recorded here so it is not tried a third time. The first attempt
went through cellul_step_byte per byte, which cost more than the single sieved word it replaced: n=8
went 207 to 260. The walk's case-sensitive arm is a plain byte compare now, so the second attempt
should have been cheaper - but factoring the walk into a function both arms could call made it a real
call on the hot path too, and n=8 went 187 to 308 while n=2048 regressed 10%. Forcing it inline
instead puts the whole walk in the entry twice. What is left is the shape here: no bypass, one copy
of the walk, inline.

The libc side is ESP-ROM code (`strnlen` at `0x400013f8`, `memcmp` at `0x4000120c`), hand-written
assembly executing from ROM. The library executes from flash through the instruction cache. The
comparison is against that, not against a portable C libc.

### Dispatch costs nothing

`MMGR_CALL` was measured against a direct call to the same entry, same work, same buffer:

| target            | dispatch_len8 | direct_len8 |
| ----------------- | ------------- | ----------- |
| ESP32-S3 (Xtensa) | 116.02        | 116.01      |
| ESP32-C6 (RISC-V) | 111.06        | 111.06      |

Identical on both. The compound literal folds into registers and no argument struct is materialized.
This is worth stating because it does not hold everywhere: on Cortex-M4, `MMGR_CALL` emitted a
`memset` of the whole argument type per call. Neither of these parts does that.
