# What it costs {#ref_performance}

Measured, not asserted. This page is how to run the benchmarks and how to tell a result from noise.

There are two sets, and they answer different questions. The host benches under `test/bench` compare
the library against itself - carrier widths, optimization levels, what LTO is worth. The on-device
benches under `test/performance_benching` compare it against the target's own libc on silicon, and
those are the numbers that decide whether an entry is fast. A host figure cannot settle that: a
desktop libc reaches for SSE or AVX and no target in the list has anything of the kind.

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

LTO is load-bearing, and it is worth being blunt about how much. The SWAR primitives are generated
in `verbum_scrutor.c` by `GENERIC_ENTRY` and called by name from every other module, so no
translation unit can inline them on its own; the build sets `CMAKE_INTERPROCEDURAL_OPTIMIZATION` for
exactly that reason. Built without it, one machine-word step becomes three to five out-of-line calls,
each with its argument struct spilled to the stack first, and `cellul.len` measures **26.6
cycles/byte on an ESP32-S3 against 5.0 with it** - worse than the byte-at-a-time ROM `strnlen` it is
being compared to.

@warning A toolchain that cannot do link-time optimization does not give a slower build of the same
library; it gives a different one. ESP-IDF appends `-fno-lto` to every link unconditionally, so an
IDF project has to take that flag back out - see `test/performance_benching` for how.

## On the parts it ships to

The tables above are x86-64. They say which optimization level to hand a module and how the lane
width scales; they are not a figure for how fast the library is, because a desktop libc answers the
same calls with SSE or AVX and no target in the list has anything of the kind.

`test/performance_benching` runs the entries against the target's own libc on silicon, reading the
part's cycle counter. That libc is ESP-ROM - `strnlen`, `strchr`, `memcmp`, `memchr`, `memcpy`,
`memset` and `strstr` are hand-written assembly in the mask ROM, executing without flash-cache
pressure, while the library executes from flash through the instruction cache.

Cycles per byte at n=2048, ESP32-S3 at 240 MHz, ratios mmgr/libc so below 1.00 is a win:

| module       | op   |  mmgr | libc ROM | ratio |
| ------------ | ---- | ----: | -------: | ----: |
| `cellul`     | len  |  5.04 |     9.03 | **0.56** |
| `cellul`     | chr  |  4.03 |     7.02 | **0.57** |
| `cellul`     | cmp  |  2.02 |     2.77 | **0.73** |
| `cellul`     | find |  7.58 |     9.02 | **0.84** |
| `memor`      | cmp  |  2.02 |     2.77 | **0.73** |
| `memor`      | chr  |  3.27 |     7.02 | **0.47** |
| `memor`      | cpy  | 0.646 |    0.646 | 1.00 |
| `memor`      | set  | 0.333 |    0.336 | **0.99** |

`memor.cpy` is parity, not a win: ROM `memcpy` is hand-written assembly, and matching it with
portable C word moves is the ceiling short of writing assembly. The ESP32-C6 agrees throughout.

What is still above 1.00 is at short lengths, and it is fixed cost rather than per-byte work: an
entry call that does not inline into the caller, and for `find` the sieve setup, which runs the
needle against the cost table before a haystack byte is read.

## Writing a bench

Read `test/bench/bench_harness.h` first, in particular the note on why the memory barrier alone is
not enough.

The short version: **derive the argument from `bench_i_`**, the loop counter `BENCH_TIME` declares.
An argument that does not change is hoisted out of the loop even with the clobber in place, and what
is left times a counter while the work happens once. A bench whose reading does not move when the
input changes is almost certainly doing that.

```c
BENCH_TIME(has_zero, {
    mmgr_word w = words[bench_i_ & MASK];       BENCH_KEEP(scrut.has_zero(w));
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
