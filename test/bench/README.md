# test/bench

What the library costs, measured and not asserted.

    cmake -S . -B build -DMMGR_BUILD_BENCH=ON
    cmake --build build
    ./build/bench/swar16/bench_scrutor_swar16

One binary per bench per environment, because the reading of interest is the comparison **between**
environments. A SWAR bench built only at the host's lane width cannot show what a wider lane
bought.

## Reading a result

Compare rows within one build. Do not compare a number here against a number from another machine,
another compiler, or a machine that did other work at the same time.

The `nop` row runs the loop, the index derivation and the barrier and calls no module entry, and so it
is the harness's own cost. A row that is not clearly above `nop` is noise and not a result.

`cycles_per_byte` is `cycles_per_call / MMGR_SWAR_BYTES`. The design claim lives in that column. A
wider lane answers for more bytes per register operation, and so cycles-per-byte should fall as the
lane widens. It does: it falls to about half for each doubled width, on gcc 13.2 `-O2`, x86-64:

| case     | 16-bit | 32-bit | 64-bit | 16 -> 64 |
| -------- | -----: | -----: | -----: | -------: |
| load     |   4.70 |   2.03 |   0.89 |     5.3x |
| has_zero |   8.80 |   3.32 |   1.36 |     6.5x |
| le       |   7.78 |   3.14 |   1.43 |     5.4x |
| spread   |   7.74 |   2.89 |   1.33 |     5.8x |
| eq       |  10.09 |   4.30 |   2.32 |     4.4x |

`eq` sits about 11% above `has_zero`, the movzbl, imul and xor it adds. Nothing to explain.
one row here that looks like a question and not an answer.

## Writing one

Read `bench_harness.h` first, in particular the note on why the barrier alone is not enough. The
short version: derive the argument from `bench_i_`, the loop counter `BENCH_TIME` declares. The
compiler hoists an argument that does not change out of the loop, even with the memory clobber in
place. The loop that stays then times a counter, and the work occurs once. A bench whose reading
does not move when `SPREAD` changes almost certainly has this defect.

`results/` holds committed CSVs, and a change in these numbers shows as a diff. They are a record
of one machine on one day and not a threshold: nothing fails a build over them.
