# Every compile-time knob {#ref_configuration}

**Purpose:** Set the library's widths, sizes and optional modules for your target, and know which of
them are derived rather than yours to set.
**Scope:** `src/config/mmgr_config.h`, `src/config/mmgr_compiler_directives.h`, `CMakeLists.txt`
**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-08-30

Nothing here is set at runtime. Every value below is a preprocessor define with a default, so you
set only what you are changing. The last section is the exception: those are CMake options rather
than defines.

## Widths

| knob              |                             default | what it changes                                                                     |
| ----------------- | ----------------------------------: | ----------------------------------------------------------------------------------- |
| `MMGR_WORD_BITS`  |          derived from `UINTPTR_MAX` | the SWAR carrier and `mmgr_word`. 64, 32 or 16                                      |
| `MMGR_INDEX_BITS` | 32, or `MMGR_WORD_BITS` if narrower | `mmgr_idx`, the type of an offset into a region                                     |
| `MMGR_SWAR_BITS`  |                    `MMGR_WORD_BITS` | **derived, not a knob.** `#undef`ed and redefined so it cannot be set independently |

Setting `MMGR_WORD_BITS` narrower than the machine does not make anything faster — it makes the
scanner answer for fewer bytes per load. It exists so a wide host can exercise a narrow machine's
code paths. See @ref concept_swar.

`mmgr_types.h` carries static asserts that police the combination; `idx16` is the environment that
exists to reach them.

## Region sizes

| knob                         |               default | what it changes                                            |
| ---------------------------- | --------------------: | ---------------------------------------------------------- |
| `MMGR_PLAINTEXT_CONFIN_SIZE` |                `4096` | the largest plaintext confinium this build will declare    |
| `MMGR_SECURE_CONFIN_SIZE`    |                `4096` | the largest secure confinium this build will declare       |
| `MMGR_CARCER_MAX`            | the larger of the two | **derived.** Bounds a scan's word count and `MMGR_STR_MAX` |
| `MMGR_CARCER_ALIGN`          |   `sizeof(mmgr_word)` | **derived.** The alignment every tenancy is handed out at  |

**The two size knobs allocate nothing and size no pool.** A pool's extent is one row of the storage
you declared, and nothing in locus_carcerum reads either knob. What they do is feed
`MMGR_CARCER_MAX`, which `verbum_scrutor` sizes its worst-case word count against and
`mmgr_string_shim.h` uses as `MMGR_STR_MAX`. They are a statement of intent about the regions you
are going to declare, so declare a larger one and they want raising.

These are the numbers you change after measuring. Do not guess them — see @ref guide_first_region
for reading the high-water marks.

## Workers

There is no knob here, which is the point rather than an omission.

There is no synchronization anywhere in the allocator, because there is nothing to synchronize. A
region is a pointer, an extent, and two offsets, used by whoever holds it. Two contexts that must
not share get two regions.

## Debug

| knob                     |              default | what it changes                                         |
| ------------------------ | -------------------: | ------------------------------------------------------- |
| `MMGR_DEBUG_CHECKS`      |                  `0` | compiles in the checks, and selects the trapping assert |
| `MMGR_ASSERT(cond, msg)` | a type-checked no-op | what a broken precondition does                         |

The default `MMGR_ASSERT` keeps its expression type-checked with `sizeof` and then discards it, so it
cannot rot and costs nothing.

`MMGR_DEBUG_CHECKS=1` is all it takes to trap: `mmgr_config.h` then defines `MMGR_ASSERT` itself, as
a report to `stderr` naming the expectation, the file and the line, followed by `abort()`. That is
the whole of the `checks` environment. Define `MMGR_ASSERT` yourself before including the header and
neither form is used, which is what a target with no `stderr` and no `abort()` wants.
See @ref ref_error_handling.

## Optional modules

| knob                  | default | what it changes                                        |
| --------------------- | ------: | ------------------------------------------------------ |
| `MMGR_ENABLE_DMA`     |     `0` | compiles `memoriam_praetereo/`, included from `mmgr.h` |
| `MMGR_ENABLE_EXTRAM`  |     `0` | compiles `memoria_externa/`, included from `mmgr.h`    |
| `MMGR_PRAET_CHANNELS` |     `2` | only when DMA is on                                    |
| `MMGR_PRAET_BUF_SIZE` |   `256` | only when DMA is on                                    |

With these off, the modules are absent entirely — not stubbed. Their test suites are skipped with a
CMake status message rather than silently dropped.

## Scanning and text

| knob                   |                  default | what it changes                                                    |
| ---------------------- | -----------------------: | ------------------------------------------------------------------ |
| `MMGR_STR_MAX`         | see `mmgr_string_shim.h` | the read cap the shim's `str*` replacements use                    |
| `MMGR_RING_LOCULI`     |                      `8` | loculi this build reserves                                         |
| `MMGR_RING_LOCULI_MAX` |         `MMGR_WORD_BITS` | loculi a mask can address, one bit per loculus in a machine word   |
| `MMGR_RING_WORDS`      |                     `40` | size of the ring storage a caller declares, in `size_t` units      |
| `MMGR_SIEVE_ROWS`      |                      `1` | needle offsets the search sieve tests per candidate word           |
| `MMGR_FIND_CHAIN_MAX`  |               `SIZE_MAX` | longest haystack a one or two byte needle is settled by mask chain |

`MMGR_RING_LOCULI_MAX` is derived, not a knob, and it is `MMGR_WORD_BITS` rather than a fixed 32:
the free and held masks are one `mmgr_word` each, one bit per loculus
(`src/memoria_anularis/memoria_anularis.c:308-309`), so the ceiling moves with the word width. A
build declaring more loculi than that fails the static assert in
`src/memoria_anularis/memoria_anularis.h:84`.

The byte-frequency profile is a CMake option taking a value, not a set of defines:
`-DMMGR_ANCORAE_FORMA=english`, and likewise `uri`, `inet`, `route` or `generic`. One profile is one
translation unit — `src/impensa_ancorae_acus/CMakeLists.txt:7-15` selects the single source that
carries the table, so the four not chosen are not in the image at all. It defaults to `generic`, and
picking the wrong one costs speed and never correctness: the search still finds what is there. See
@ref mod_anchor_guide.

`MMGR_FIND_CHAIN_MAX` defaults to no limit, which folds its test away: `read_cap <= SIZE_MAX` holds
for every `size_t`, so a default build emits no comparison. A one or two byte needle is settled by a
mask chain — one broadcast per needle byte, every start position in the word decided at once, nothing
to verify — rather than by building the sieve, which exists to find a rare byte in a long needle and
prove the rest once. Measured with a two byte needle, cycles for the whole call:

| n            |   8 |  64 |  2048 |
| ------------ | --: | --: | ----: |
| Xtensa chain | 124 | 489 | 13391 |
| Xtensa sieve | 187 | 607 | 15494 |
| RISC-V chain | 124 | 488 | 13393 |
| RISC-V sieve | 219 | 680 | 17059 |

The chain wins at every length on both parts, so nothing needs setting. The knob is kept because
that is a measurement rather than a proof; zero sends every needle through the sieve.

Case folding always goes through the sieve — the chain compares raw bytes.

## Platform

| knob                     |                                                  default | what it changes                       |
| ------------------------ | -------------------------------------------------------: | ------------------------------------- |
| `MMGR_HW_BIG_ENDIAN`     |                            derived from `__BYTE_ORDER__` | the host's byte order                 |
| `MMGR_HW_FAST_UNALIGNED` |               derived from `__ARM_FEATURE_UNALIGNED` etc | whether a word load takes any address |
| `MMGR_INLINE`            |    `static inline`, plus `always_inline` where available | how hot entries are inlined           |
| `MMGR_FLATTEN`           | `__attribute__((flatten))` where available, else nothing | a caller's lever to inline an entry   |

`MMGR_FLATTEN` is for a caller, not for the library. Put it on the one hot function that reaches an
entry and the compiler inlines the entry into it, which the inliner otherwise declines to do on size
even under link-time optimization. Measured on an ESP32-S3, `cellul.len` over eight bytes is 112
cycles called and 80 inlined — a third of the work at that length. @ref ref_performance has the table
and the caveats: it needs LTO, it costs the walk's code at every site that takes it, and a long scan
amortises the call and will not notice.

`MMGR_HW_FAST_UNALIGNED` is not whether an unaligned load _compiles_ — every target accepts one
through `mmgr_proxim_word_t`, which carries `MMGR_ALIGN(1)`. It is whether the hardware does it in
one instruction, or the compiler assembles the word out of byte loads and shifts. Measured on a
single such load: ARMv7-M emits one `ldr`, Xtensa twelve instructions, RISC-V eleven. A walk that
needs the word at an offset of one takes the load where it is one instruction and derives it from
the word already in hand where it is a dozen.

It is not a statement about the family either. Cortex-M0 is ARMv6-M, has no unaligned access, and
the macro reports it slow — the compiler answers through `__ARM_FEATURE_UNALIGNED`, which
`-mno-unaligned-access` also turns off on a part that has it.

Every compiler conditional in the library lives in `mmgr_compiler_directives.h` by policy, so
supporting a new toolchain is one file to read. See @ref ref_compiler_support.

## CMake options

These are build options, not preprocessor defines.

| option             | default | what it does                                            |
| ------------------ | ------: | ------------------------------------------------------- |
| `MMGR_BUILD_TESTS` |    `ON` | build the unit and environment suites                   |
| `MMGR_BUILD_BENCH` |   `OFF` | build the benchmarks                                    |
| `MMGR_WERROR`      |   `OFF` | treat warnings as errors                                |
| `MMGR_LTO`         |    `ON` | link-time optimization, where the toolchain supports it |

One more thing the build does that is worth knowing: `MinSizeRel` normally means `-Os`, and `-Os`
declines to inline the small branch-free entries that _are_ this library — so the calls it saves
space on are the calls that cost the most. The top-level `CMakeLists.txt` rewrites `-Os` to `-O2`
for that configuration. An embedded build reaching for `MinSizeRel` wants small, not slow-and-small.
