# What it costs {#ref_performance}

Measured, not asserted. This page is how to run the benchmarks and how to tell a result from noise.

## Running them

```sh
cmake -S . -B build-bench -DMMGR_BUILD_BENCH=ON
cmake --build build-bench
./build-bench/bench/word16/bench_scrutor_word16
```

Benches are off by default and are added independently of `MMGR_BUILD_TESTS`, so
`-DMMGR_BUILD_BENCH=ON` alone is enough. They are always compiled `-O2` regardless of
`CMAKE_BUILD_TYPE` — measuring a debug build tells you about the debug build.

One binary per bench **per environment**, because the reading that matters is the comparison
_between_ environments. A SWAR bench built only at the host's carrier width cannot show what
widening the carrier bought.

## Reading a result

**Compare rows within one build.** Do not compare a number here against one from another machine,
another compiler, or a machine that was doing something else at the time.

**The `nop` row is the harness.** It runs the loop, the index derivation and the barrier and calls no
module entry, so it is the cost of measuring. A row that is not clearly above `nop` is noise, not a
result.

**`cycles_per_byte` is the column the design claim lives in.** It is `cycles_per_call / MMGR_SWAR_BYTES`.
A wider carrier answers for more bytes per register operation, so cycles-per-byte should fall as the
carrier widens. If it does not, the claim is wrong.

## The numbers

gcc 13.2 `-O2`, x86-64.

| case       | 16-bit | 32-bit | 64-bit | 16 → 64 |
| ---------- | -----: | -----: | -----: | ------: |
| `load`     |   4.70 |   2.03 |   0.89 |    5.3x |
| `has_zero` |   8.80 |   3.32 |   1.36 |    6.5x |
| `le`       |   7.78 |   3.14 |   1.43 |    5.4x |
| `spread`   |   7.74 |   2.89 |   1.33 |    5.8x |
| `eq`       |  10.09 |   4.30 |   2.32 |    4.4x |

It halves, roughly, per doubling. `eq` sits about 11% above `has_zero`, which is the `movzbl`,
`imul` and `xor` it adds. Nothing to explain.

## What LTO is worth

`mmgr_memor_chr` over 512 bytes:

| build                             | cycles |
| --------------------------------- | -----: |
| SWAR entries as out-of-line calls |    610 |
| linker permitted to inline them   |    187 |

The hot entries are `MMGR_INLINE` in their headers now, so LTO is no longer load-bearing for those.
It still is for every cross-module call that is not.

## Writing a bench

Read `test/bench/bench_harness.h` first, in particular the note on why the memory barrier alone is
not enough.

The short version: **derive the argument from `bench_i_`**, the loop counter `BENCH_TIME` declares.
An argument that does not change is hoisted out of the loop even with the clobber in place, and what
is left times a counter while the work happens once. A bench whose reading does not move when the
input changes is almost certainly doing that.

```c
BENCH_TIME(has_zero, {
    mmgr_scrut_word w = words[bench_i_ & MASK];   /* derived from the counter */
    BENCH_KEEP(scrut.has_zero(w));
});
```

`bench_cycles()` is `lfence; rdtsc; lfence` on x86 and a counter read on aarch64. `bench_now()` is
`CLOCK_MONOTONIC`.

## The committed results

`test/bench/results/*.csv` holds `module,case,lane_bits,cycles_per_call,cycles_per_byte` from one
machine on one day. They are committed so that a change in these numbers shows up as a diff in a
pull request.

They are **a record, not a threshold**. Nothing fails a build over them. A CI runner is a shared,
virtualized, frequency-scaled machine, and a performance gate on one would fail for reasons that
have nothing to do with the change under review.
