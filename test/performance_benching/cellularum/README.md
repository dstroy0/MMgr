# cellularum on-device bench

Cycle-counter A/B of the string entries against the target's own libc, on silicon. Two parts,
because the library builds for both instruction sets and they answer differently: ESP32-S3
(Xtensa LX7, 240 MHz) and ESP32-C6 (RISC-V, 160 MHz).

## Build and run

The install here is ESP-IDF 5.5.5 under `C:\Espressif`, whose virtualenv has the name of Python
3.14. `export.ps1` derives both locations from the environment, and so you have to set both:

```powershell
$env:IDF_PATH          = 'C:\Espressif\frameworks\esp-idf-v5.5.5'
$env:IDF_TOOLS_PATH    = 'C:\Espressif'
$env:IDF_PYTHON_ENV_PATH = 'C:\Espressif\python_env\idf5.5_py3.14_env'
. $env:IDF_PATH\export.ps1

idf.py -B build_esp32s3 -D SDKCONFIG=sdkconfig.esp32s3 set-target esp32s3
ninja -C build_esp32s3 -j 2
idf.py -B build_esp32s3 -p COM4 flash
```

Substitute `esp32c6` and its port for the RISC-V part. Each target retains its own sdkconfig. The
default is one shared file, and the two parts do not agree on CPU frequency. With one file, a
configure of one part leaves the other's build directory on a config that does not match its part.

Cap the job count. ninja defaults to cores + 2, and a full IDF tree at that width can take the
machine down.

The image prints one `DB ` line per operation and repeats every five seconds. A capture opened at
any time catches a whole pass.

## LTO is not optional

The bench compiles this component with `-flto`, and removes the `-fno-lto` that ESP-IDF appends to
every link. Without that it measures something the library never does in a real build.

`EMBED_ENTRY` generates the SWAR primitives in `verbum_scrutor.c`, and every other module calls them
by name. Nothing can inline them across translation units on its own, and the desktop build sets
`CMAKE_INTERPROCEDURAL_OPTIMIZATION` for exactly that reason. Built without it, one four-byte word
step becomes three to five windowed calls, each with its argument struct spilled to the stack first:

```
420093fd: call8 <mmgr_scrut_words>
4200941a: call8 <mmgr_scrut_load>
42009425: call8 <mmgr_scrut_has_zero>
42009435: call8 <mmgr_scrut_tail_mask>
42009449: call8 <mmgr_scrut_lane_lo>
```

Without LTO, `len` costs more per byte than the byte-at-a-time ROM `strnlen`: 26.6 cycles/byte
against 9.0 at n=2048 on the S3.

## What the walks do

Each scan computes the work that matters on one word of a scan only on that one word:

- **No `mask.tail` per word.** The walk knows `cap` before the loop, and so lanes past it can fall
  only in the last word. The walks run whole words with no mask and take the short word once, below
  the loop.
- **No lane search per word.** The common case needs to know only whether two words differ at all,
  and that is one compare. `cmp` resolves the lane once, after the loop finds the word that differs.
  `chr` likewise applies `mask.before` once, on the word that carried a hit or a terminator.

`lane.eq` rebuilds its broadcast from a byte on every call, and LTO leaves it as an out-of-line call
in a hot loop. The bytes of the sieve in `find` do not change during the walk. So the walk
broadcasts them once ahead of it and compares them inline.

Per 4-byte word, `cmp` runs about 6 instructions, as a word-wise memcmp does.

Three more rules, each measured on the part:

- **The aligned load.** `word.load` goes to a type that carries `EMBED_ALIGN(1)`. Neither part we
  ship to has an unaligned word load, and so the compiler assembles one out of four byte loads and
  six shifts. The walks step to the first word boundary and read the body through `word.load_al`.
  On that alone, `len` costs 3.055 cycles/byte against 3.788 through `word.load`.
- **Two words a pass in `len`.** The load and the `has_zero` that reads it are a dependent pair, and
  neither part issues them back to back with no stall. Two words a pass cost 2.547 against 3.055.
  `chr` and `memor.chr` do not gain from it: they already carry enough arithmetic to cover the load.
- **A mask chain for one and two byte needles.** A needle of that size has no rare byte to anchor
  on, so `find` does not build the anchor-and-verify sieve for it. The chain costs 6.543 cycles/byte
  against 11.32 for the sieve.

Cycles per byte at n=2048 on the S3:

| op   | mmgr      | libc ROM |
| ---- | --------- | -------- |
| len  | **2.547** | 9.024    |
| chr  | **4.274** | 7.021    |
| cmp  | **2.020** | 2.773    |
| find | **6.543** | 9.021    |

The module's .text is 8550 bytes at -O2 on Xtensa, against 6506 for walks without these rules.

## Retain every timed result

`DBENCH_RETAIN` stores each result to a volatile sink. The optimizer may delete outright a call whose
result nobody stores, and under LTO it does. Both arms of a dispatch measurement then report `0.00`
cycles, because the optimizer deletes both loops.

## Results

The raw captures are in `results/`. Ratios are mmgr/libc, and so below 1.00 is a win.

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
occurs. `find` searches for one whose first byte is not in the haystack's alphabet at all. MMgr
costs the same either way - 13400.7 cycles at n=2048 against 13400.8. The chain settles every start
position in a word arithmetically. ROM `strstr` does not: 18474 against 20394.

For that reason the chain retains its third `has_zero`. An anchor on one byte and a verify of the
other would be about 20% cheaper. It would buy that with a dependence on the data exactly where libc
already has one.

What stays above 1.00 is at n=8, and it is a cost that does not change with n, not per-byte work. Subtract the floor
first: `floor_call` is 41 cycles on the S3 and 28 on the C6, and both arms pay it. For `find` the
rest is prologue: two broadcasts, a span, a reach and a word count, all settled before the walk reads
the first byte.

No bypass sends short haystacks past the word machinery to the byte walk, and nobody may add one:

- A walk through `cellul_step_byte` per byte costs more than the single sieved word: 260 cycles at
  n=8 against 207.
- A walk in a function that both arms call is a real call on the hot path: 308 cycles at n=8
  against 187, and 10% more at n=2048.
- A walk forced inline puts the whole walk in the entry twice.

So the shape stays: no bypass, one copy of the walk, inline.

The libc side is ESP-ROM code (`strnlen` at `0x400013f8`, `memcmp` at `0x4000120c`), hand-written
assembly that runs from ROM. The library runs from flash through the instruction cache. The
comparison is against that, not against a portable C libc.

### Dispatch costs nothing

`EMBED_CALL` against a direct call to the same entry, same work, same buffer:

| target            | dispatch_len8 | direct_len8 |
| ----------------- | ------------- | ----------- |
| ESP32-S3 (Xtensa) | 116.02        | 116.01      |
| ESP32-C6 (RISC-V) | 111.06        | 111.06      |

Identical on both. The compound literal folds into registers, and the code builds no argument
struct. That does not hold everywhere: on Cortex-M4, `EMBED_CALL` emits a `memset` of the whole
argument type per call. Neither of these parts does that.
