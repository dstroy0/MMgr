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
each with its argument struct spilled to the stack first, and `cellul.len` measured **26.6
cycles/byte on an ESP32-S3 against 5.0 with it** - worse than the byte-at-a-time ROM `strnlen` it is
being compared to. The walks have since been reworked and `len` is at 2.547, so the gap without LTO
is wider now than that measurement records; it is left as taken rather than scaled by guesswork.

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

Cycles per byte at n=2048, ratios mmgr/libc so below 1.00 is a win:

| module   | op   | S3 mmgr | S3 libc | S3 ratio | C6 mmgr | C6 libc | C6 ratio |
| -------- | ---- | ------: | ------: | -------: | ------: | ------: | -------: |
| `cellul` | len  |   2.547 |   9.024 | **0.28** |   2.292 |   9.016 | **0.25** |
| `cellul` | chr  |   4.274 |   7.021 | **0.61** |   3.775 |   9.018 | **0.42** |
| `cellul` | cmp  |   2.020 |   2.773 | **0.73** |   1.765 |   2.137 | **0.83** |
| `cellul` | find |   6.543 |   9.021 | **0.73** |   6.290 |   7.019 | **0.90** |
| `memor`  | cmp  |   2.019 |   2.774 | **0.73** |   1.763 |   2.137 | **0.82** |
| `memor`  | chr  |   3.270 |   7.020 | **0.47** |   2.765 |  12.017 | **0.23** |
| `memor`  | cpy  |   0.646 |   0.646 |     1.00 |   0.705 |   0.701 |     1.01 |
| `memor`  | set  |   0.333 |   0.336 | **0.99** |   0.391 |   0.394 | **0.99** |

`memor.cpy` is parity, not a win. ROM `memcpy` is hand-written assembly, and matching it with
portable C word moves is the ceiling short of writing assembly or handing the move to DMA.

## Reading the short lengths

Every row above is at 2048 bytes, where the per-byte work dominates. At eight it does not, and the
number that matters there is the harness floor - the loop, the counter and the volatile store, plus
one call the optimiser is not allowed to remove:

| part | loop alone | plus one call |
| ---- | ---------: | ------------: |
| ESP32-S3 | 6.0 | 41.0 |
| ESP32-C6 | 3.0 | 28.0 |

Both arms pay it. `cellul.len` over eight bytes costs 112 cycles on the S3 against ROM `strnlen`'s
113, and 41 of each is the call. A ratio at n=8 is mostly two call floors and should be read as
such; subtract before drawing a conclusion.

`MMGR_CALL` itself is free. Dispatching through the namespace table and calling the entry directly
cost the same to the last two decimals - 112.02 against 112.02 on the S3, 102.06 against 102.06 on
the C6 - so the compound literal folds into registers and no argument struct is built. That is worth
stating because it does not hold everywhere: on Cortex-M4 `MMGR_CALL` emitted a `memset` of the
whole argument type per call.

## Taking the call out

The call is not fixed, though. The entries are large enough that the inliner leaves them out of
line on size even with link-time optimization on, and a caller can overrule that with
`MMGR_FLATTEN` on the one function it cares about:

```c
MMGR_FLATTEN static size_t field_len(const char *s)
{
    return MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = s, .cap = FIELD_MAX);
}
```

ESP32-S3, `cellul.len` over the same eight bytes:

| shape                        | cycles |
| ---------------------------- | -----: |
| through the namespace table  | 112.01 |
| calling the entry by name    | 112.01 |
| `MMGR_FLATTEN` on the caller |  **80.02** |

**32 cycles, a third of the work at that length.** Against ROM `strnlen`'s 113 that is 0.99 called
and 0.71 inlined. libc cannot answer it - its routine is in mask ROM and is reached by a call no
matter what the caller does.

It is worth it where an extent is short and settled before the build, which is what this library is
for. A long scan amortises the call and will not notice: the same measurement at 64 bytes is 253
against 240. And it costs the walk's code at every site that takes it, so it belongs on the one hot
function rather than on a translation unit.

`MMGR_FLATTEN` needs the entry body visible, so a build without link-time optimization gets nothing
from it. It resolves to the attribute on every toolchain in the target list - Xtensa, RISC-V, and ARM
from Cortex-M3 up - and expands to nothing anywhere else, which costs speed and never correctness.

What remains above 1.00 at eight bytes is `find`, at 134 cycles against 106. That is prologue - two
broadcasts, a span, a reach and a word count settled before the first byte is read - and routing
short haystacks past it has been tried twice and lost twice. Both attempts are written up in
`test/performance_benching/cellularum/README.md` so a third is not started from scratch.

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
