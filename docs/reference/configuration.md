# Every compile-time knob {#ref_configuration}

Nothing here is set at runtime. Every value below is a preprocessor define with a default, so you
set only what you are changing.

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

| knob                         |               default | what it changes                                                    |
| ---------------------------- | --------------------: | ------------------------------------------------------------------ |
| `MMGR_PLAINTEXT_CONFIN_SIZE` |                `4096` | the largest plaintext confinium this build will declare              |
| `MMGR_SECURE_CONFIN_SIZE`    |                `4096` | the largest secure confinium this build will declare                 |
| `MMGR_CARCER_MAX`            | the larger of the two | **derived.** Bounds a scan's word count and `MMGR_STR_MAX`           |
| `MMGR_CARCER_MAX_REGIONS`    |                   `8` | the most pools one region may be carved into                         |
| `MMGR_CARCER_ALIGN`          |    `sizeof(mmgr_word)` | **derived.** The alignment every tenancy is handed out at            |

**The two size knobs allocate nothing and size no pool.** A pool's extent is an argument to
`mmgr_carcer_init`, and nothing in carceribus reads either knob. What they do is feed
`MMGR_CARCER_MAX`, which `verbum_scrutor` sizes its worst-case word count against and
`mmgr_string_shim.h` uses as `MMGR_STR_MAX`. They are a statement of intent about the regions you
are going to declare, so declare a larger one and they want raising.

These are the numbers you change after measuring. Do not guess them — see @ref guide_first_region
for reading the high-water marks.

## Workers

| knob                     |             default | what it changes                                               |
| ------------------------ | ------------------: | ------------------------------------------------------------- |

There is no synchronization anywhere in the allocator, because there is nothing to synchronize. A
region is a pointer, an extent, and two offsets, used by whoever holds it. Two contexts that must
not share get two regions.

## Debug

| knob                     |              default | what it changes                                        |
| ------------------------ | -------------------: | ------------------------------------------------------ |
| `MMGR_DEBUG_CHECKS`      |                  `0` | compiles in the checks, and selects the trapping assert |
| `MMGR_ASSERT(cond, msg)` | a type-checked no-op | what a broken precondition does                        |

The default `MMGR_ASSERT` keeps its expression type-checked with `sizeof` and then discards it, so it
cannot rot and costs nothing.

`MMGR_DEBUG_CHECKS=1` is all it takes to trap: `mmgr_config.h` then defines `MMGR_ASSERT` itself, as
a report to `stderr` naming the expectation, the file and the line, followed by `abort()`. That is
the whole of the `checks` environment. Define `MMGR_ASSERT` yourself before including the header and
neither form is used, which is what a target with no `stderr` and no `abort()` wants.
See @ref ref_error_handling.

## Optional modules

| knob                     | default | what it changes                               |
| ------------------------ | ------: | --------------------------------------------- |
| `MMGR_ENABLE_DMA`        |     `0` | compiles `memoriam_praetereo/`, included from `mmgr.h` |
| `MMGR_ENABLE_PSRAM_POOL` |     `0` | compiles `confinium_externum/`                |
| `MMGR_PRAET_CHANNELS`    |     `2` | only when DMA is on                           |
| `MMGR_PRAET_BUF_SIZE`    |   `256` | only when DMA is on                           |

With these off, the modules are absent entirely — not stubbed. Their test suites are skipped with a
CMake status message rather than silently dropped.

## Scanning and text

| knob                          |                  default | what it changes                                                    |
| ----------------------------- | -----------------------: | ------------------------------------------------------------------ |
| `MMGR_STR_MAX`                | see `mmgr_string_shim.h` | the read cap the shim's `str*` replacements use                    |
| `MMGR_RING_LOCULI_MAX`         |                     `32` | loculi in the ring's bitmap allocator. Fixed by the `uint32_t` mask |
| `MMGR_ANCORAE_FORMA_ENGLISH` |                    unset | byte-frequency profile for substring search                        |
| `MMGR_ANCORAE_FORMA_URI`     |                    unset | "                                                                  |
| `MMGR_ANCORAE_FORMA_INET`    |                    unset | "                                                                  |
| `MMGR_ANCORAE_FORMA_ROUTE`   |                    unset | "                                                                  |

The anchor profiles are mutually exclusive and default to a generic table. Picking the wrong one
costs speed and never correctness — the search still finds what is there. See @ref mod_anchor_guide.

## Platform

| knob                 |                                               default | what it changes             |
| -------------------- | ----------------------------------------------------: | --------------------------- |
| `MMGR_HW_BIG_ENDIAN` |                         derived from `__BYTE_ORDER__` | the host's byte order       |
| `MMGR_INLINE`        | `static inline`, plus `always_inline` where available | how hot entries are inlined |

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
