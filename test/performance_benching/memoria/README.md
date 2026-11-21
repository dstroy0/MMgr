# memoria on-device bench

Cycle-counter A/B of the region entries against the target's own libc, on silicon. Two parts:
ESP32-S3 (Xtensa LX7, 240 MHz) and ESP32-C6 (RISC-V, 160 MHz).

`memor.cmp` against `memcmp`, `memor.chr` against `memchr`, `memor.cpy` against `memcpy` and
`memor.set` against `memset`. They run over the same lengths and the same aligned fixture as the
cellularum bench, and so the two read against one libc.

## Build and run

See `../cellularum/README.md` - the toolchain setup, the job cap and the LTO requirement are the same
for every bench here, and this page does not repeat them. Substitute this directory:

```powershell
idf.py -C test/performance_benching/memoria -B build_esp32s3 -D SDKCONFIG=sdkconfig.esp32s3 set-target esp32s3
ninja -C test/performance_benching/memoria/build_esp32s3 -j 2
idf.py -C test/performance_benching/memoria -B build_esp32s3 -p COM4 flash
```

## Results

Raw captures in `results/`. Ratios are mmgr/libc, and so below 1.00 is a win. Cycles per byte at n=2048:

| op  | S3 mmgr | S3 libc | S3 ratio | C6 mmgr | C6 libc | C6 ratio |
| --- | ------: | ------: | -------: | ------: | ------: | -------: |
| cmp |   2.019 |   2.774 | **0.73** |   1.763 |   2.137 | **0.82** |
| chr |   3.270 |   7.020 | **0.47** |   2.765 |  12.017 | **0.23** |
| cpy |   0.646 |   0.646 |     1.00 |   0.705 |   0.701 |     1.01 |
| set |   0.333 |   0.336 | **0.99** |   0.391 |   0.394 | **0.99** |

At eight bytes, where the call floor dominates and not the work: S3 cmp 0.81, chr 0.71, cpy 0.89,
set 0.75; C6 cmp 0.97, chr 0.44, cpy 0.97, set 0.97. Every entry is at or under libc at both ends.

## What the walks do

`cmp` and `chr` walk a word at a time. The walk knows the count before the loop. Lanes past it
can fall only in the last word, so neither walk builds an extent mask on whole words. `cmp` needs to
know only whether two words differ at all. Whole words run on an inequality test with no mask, and
`cmp` resolves the lane once, after the loop finds the word that differs.

`chr` broadcasts the sought byte once ahead of the walk and does not reach `lane.eq`, which rebuilds
the broadcast from a byte on every call.

`cpy` and `set` move four words an iteration. At one word the two pointer bumps, the counter and the
branch cost as much as the move itself: 1.016 cycles/byte against ROM `memcpy`'s 0.646. With four
words, they hold its rate exactly.

That last one needs a plain statement: **`cpy` is parity and not a win.** ROM `memcpy` is
hand-written assembly. Portable C word moves that match it are the ceiling, short of assembly or a
hand-off of the move to DMA. It also costs flash - see the newlib section of @ref qa_optimization,
where the code that moves and compares bytes is larger than newlib's.

A `chr` with four words an iteration, the way `cellul_len` runs, loses: 6959 cycles against 6696 at
2048 bytes. It already carries an xor and a `has_zero` per word, which is enough work to cover the
load. No stall stays for a second word to hide behind.
