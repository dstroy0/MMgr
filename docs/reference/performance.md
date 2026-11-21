# What it costs {#ref_performance}

Measured and not asserted. This page is how to run the benchmarks and how to tell a result from noise.

There are two sets, and they answer different questions. The host benches under `test/bench` compare
the library against itself - carrier widths, optimization levels, what LTO is worth. The on-device
benches under `test/performance_benching` compare it against the target's own libc on silicon, and
those are the numbers that decide whether an entry is fast. A host figure cannot settle that: a
desktop libc reaches for SSE or AVX and no target in the list has anything of the type.

## Running them

```sh
cmake -S . -B build-bench -DMMGR_BUILD_BENCH=ON
cmake --build build-bench
./build-bench/bench/word16/bench_scrutor_word16
```

Benches are off by default, and the build adds them independently of `MMGR_BUILD_TESTS`. So
`-DMMGR_BUILD_BENCH=ON` alone is enough. They always compile at `-O2` regardless of
`CMAKE_BUILD_TYPE`: a measurement of a debug build tells you about the debug build.

One binary per bench **per environment**, because the important reading is the comparison
_between_ environments. A SWAR bench built only at the host's carrier width cannot show what a wider
carrier bought.

## Reading a result

**Compare rows within one build.** Do not compare a number here against one from another machine,
another compiler, or a machine that did other work at the same time.

**The `nop` row is the harness.** It runs the loop, the index derivation and the barrier and calls no
module entry, and so it is the cost of the measurement. A row that is not clearly above `nop` is
noise and reports no result.

**`cycles_per_byte` is the column the design claim lives in.** It is `cycles_per_call / MMGR_SWAR_BYTES`.
A wider carrier answers for more bytes per register operation, and so cycles-per-byte should fall as the
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

It falls to about half for each doubled width. `eq` sits about 11% above `has_zero`, the `movzbl`,
`imul` and `xor` it adds. Nothing to explain.

## What LTO is worth

`mmgr_memor_chr` over 512 bytes:

| build                             | cycles |
| --------------------------------- | -----: |
| SWAR entries as out-of-line calls |    610 |
| linker permitted to inline them   |    187 |

LTO is essential, and this page is blunt about how much. `EMBED_ENTRY` generates the SWAR primitives
in `verbum_scrutor.c`, and every other module calls them by name. So no translation unit can inline
them on its own; the build sets `CMAKE_INTERPROCEDURAL_OPTIMIZATION` for exactly that reason.

Built without it, one machine-word step becomes three to five out-of-line calls, each with its
argument struct spilled to the stack first. Without LTO, `cellul.len` costs more per byte than the
byte-at-a-time ROM `strnlen`: **26.6 cycles/byte against 9.0** on an ESP32-S3.

@warning A toolchain that cannot do link-time optimization does not give a slower build of the same
library; it gives a different one. ESP-IDF appends `-fno-lto` to every link unconditionally. An
IDF project has to take that flag back out - see `test/performance_benching` for how.

## On the parts it ships to

The tables above are x86-64. They say which optimization level to hand a module and how the lane
width scales. They are not a figure for how fast the library is. A desktop libc answers the same
calls with SSE or AVX, and no target in the list has anything of the type.

`test/performance_benching` runs the entries against the target's own libc on silicon, and reads
the part's cycle counter. That libc is ESP-ROM. `strnlen`, `strchr`, `memcmp`, `memchr`, `memcpy`,
`memset` and `strstr` are hand-written assembly in the mask ROM. That code runs with no flash-cache
pressure, while the library runs from flash through the instruction cache.

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

`memor.cpy` is parity and not a win. ROM `memcpy` is hand-written assembly. Portable C word moves
that match it are the ceiling, short of assembly or a hand-off of the move to DMA.

## Reading the short lengths

Every row above is at 2048 bytes, where the per-byte work dominates. At eight it does not. There
the number that matters is the harness floor: the loop, the counter and the volatile store, plus one
call the optimizer may not remove:

| part     | loop alone | plus one call |
| -------- | ---------: | ------------: |
| ESP32-S3 |        6.0 |          41.0 |
| ESP32-C6 |        3.0 |          28.0 |

Both arms pay it. `cellul.len` over eight bytes costs 112 cycles on the S3 against ROM `strnlen`'s
113, and 41 of each is the call. A ratio at n=8 is mostly two call floors, and you should read it as
such; subtract before you draw a conclusion.

`EMBED_CALL` itself is free. A call through the namespace table and a direct call of the entry cost
the same to the last two decimals. That is 112.02 against 112.02 on the S3, and 102.06 against
102.06 on the C6.
So the compound literal folds into registers, and the code builds no argument struct. That does not
hold everywhere: on Cortex-M4 `EMBED_CALL` emits a `memset` of the whole argument type per call.

## Taking the call out

The call is not a cost you must pay, though. The entries are large enough that the inliner leaves
them out of line on size, even with link-time optimization on. A caller can overrule that with
`EMBED_FLATTEN` on the one function it cares about:

```c
EMBED_FLATTEN static size_t field_len(const char *s)
{
    return EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s, .cap = FIELD_MAX);
}
```

ESP32-S3, `cellul.len` over the same eight bytes:

| shape                         |    cycles |
| ----------------------------- | --------: |
| through the namespace table   |    112.01 |
| calling the entry by name     |    112.01 |
| `EMBED_FLATTEN` on the caller | **80.02** |

**32 cycles, a third of the work at that length.** Against ROM `strnlen`'s 113 that is 0.99 as a
call and 0.71 inlined. The libc routine cannot answer it: it is in mask ROM, and a call reaches it
no matter what the caller does.

It is worth it where an extent is short and settled before the build, as this library is for. A long
scan amortizes the call and will not notice: the same measurement at 64 bytes is 253 against 240. It
also costs the walk's code at every site that takes it. So it belongs on the one hot function and
not on a translation unit.

`EMBED_FLATTEN` needs the entry body visible. A build without link-time optimization gets nothing
from it. It resolves to the attribute on every toolchain in the target list: Xtensa, RISC-V, and ARM
from Cortex-M3 up. It expands to nothing anywhere else, which costs speed and never correctness.

What stays above 1.00 at eight bytes is `find`, at 134 cycles against 106. That is prologue: two
broadcasts, a span, a reach and a word count, all settled before the walk reads the first byte. No
bypass sends short haystacks past it. `test/performance_benching/cellularum/README.md` lists each
bypass and its cost, and nobody may add one.

## Writing a bench

Read `test/bench/bench_harness.h` first, in particular the note on why the memory barrier alone is
not enough.

The short version: **derive the argument from `bench_i_`**, the loop counter `BENCH_TIME` declares.
The compiler hoists an argument that does not change out of the loop, even with the clobber in
place. What stays then times a counter, and the work occurs once. A bench whose reading does not
move when the input changes almost certainly has this defect.

```c
BENCH_TIME(has_zero, {
    embed_word w = words[bench_i_ & MASK];       BENCH_RETAIN(scrut.has_zero(w));
});
```

`bench_cycles()` is `lfence; rdtsc; lfence` on x86 and a counter read on aarch64. `bench_now()` is
`CLOCK_MONOTONIC`.

## The committed results

`test/bench/results/*.csv` holds `module,case,lane_bits,cycles_per_call,cycles_per_byte` from one
machine on one day. The tree holds them so that a change in these numbers shows up as a diff in a
pull request.

They are **a record, not a threshold**. Nothing fails a build over them. A CI runner is a shared,
virtualized machine whose frequency scales. A performance gate on one would fail for reasons that
have nothing to do with the change under review.
