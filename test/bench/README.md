# test/bench

What the library costs, measured rather than asserted.

    cmake -S . -B build -DMMGR_BUILD_BENCH=ON
    cmake --build build
    ./build/bench/swar16/bench_scrutor_swar16

One binary per bench per environment, because the reading that matters is the comparison **between**
environments. A SWAR bench built only at the host's lane width cannot show what widening the lane
bought.

## Reading a result

Compare rows within one build. Do not compare a number here against a number from another machine,
another compiler, or a machine that was doing something else at the time.

The `nop` row runs the loop, the index derivation and the barrier and calls no module entry, so it
is the harness's own cost. A row that is not clearly above `nop` is noise, not a result.

`cycles_per_byte` is `cycles_per_call / MMGR_SWAR_BYTES`. That is the column the design claim lives
in: a wider lane answers for more bytes per register operation, so cycles-per-byte should fall as
the lane widens. It does — roughly halving per doubling, measured on gcc 13.2 `-O2`, x86-64:

| case     | 16-bit | 32-bit | 64-bit | 16 -> 64 |
| -------- | -----: | -----: | -----: | -------: |
| load     |   4.70 |   2.03 |   0.89 |     5.3x |
| has_zero |   8.80 |   3.32 |   1.36 |     6.5x |
| le       |   7.78 |   3.14 |   1.43 |     5.4x |
| spread   |   7.74 |   2.89 |   1.33 |     5.8x |
| eq       |  10.09 |   4.30 |   2.32 |     4.4x |

`eq` sits about 11% above `has_zero`, which is the movzbl, imul and xor it adds. Nothing to explain.
one row here that looks like a question rather than an answer.

## Writing one

Read `bench_harness.h` first, in particular the note on why the barrier alone is not enough. The
short version: derive the argument from `bench_i_`, the loop counter `BENCH_TIME` declares. An
argument that does not change is hoisted out of the loop even with the memory clobber in place, and
the loop left behind times a counter while the work happens once. A bench whose reading does not
move when `SPREAD` changes is almost certainly doing that.

`results/` holds committed CSVs so a change in these numbers is visible as a diff. They are a record
of one machine on one day, not a threshold — nothing fails a build over them.
