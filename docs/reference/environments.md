# The environment matrix {#ref_environments}

Five sets of compile-time widths. The whole library and the whole test suite are built against every
one of them, in one build.

## What an environment is

An environment is one set of width macros. It is not a target, a toolchain or a board — it is the
handful of numbers that make one machine different from another, turned into a compile-time knob so
that a wide host can exercise a narrow machine's code paths without owning the narrow machine.

`CMakeLists.txt` declares them as a flat list:

| name     | definitions           | what it is for                                                                                                                   |
| -------- | --------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `host`   | _(none)_              | whatever this machine is. `EMBED_WORD_BITS` derives from `UINTPTR_MAX`. The default build.                                        |
| `word32` | `EMBED_WORD_BITS=32`   | a 32-bit word on a 64-bit host. Catches anything that assumed a word holds a pointer.                                            |
| `word16` | `EMBED_WORD_BITS=16`   | the narrowest carrier. Most likely to expose an off-by-one in a scan tail.                                                       |
| `idx16`  | `EMBED_INDEX_BITS=16`  | a 16-bit index against a 64-bit word, which is the pairing the static asserts in `embed_types.h` exist to police.                 |
| `checks` | `MMGR_DEBUG_CHECKS=1` | the checks compiled in, and the trapping `MMGR_ASSERT` selected, so a broken precondition fails a test instead of being a no-op. |

## One build, not five

This is the part that surprises people. `cmake/MMgrModule.cmake` loops the environment list inside
`mmgr_add_module()` and emits one library target per module **per environment**:

```
mmgr_spatium_host   mmgr_spatium_word32   mmgr_spatium_word16   ...
mmgr_locus_carcerum_host mmgr_locus_carcerum_word32 mmgr_locus_carcerum_word16 ...
```

and `src/CMakeLists.txt` aggregates each column into an interface target — `mmgr_host`,
`mmgr_word32`, `mmgr_word16`, `mmgr_idx16`, `mmgr_checks`. That aggregate is what a consumer or a
test links.

So a single `cmake --build build` builds all five, and a single `ctest` runs all five. There is no
matrix to drive from the outside and nothing to remember:

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

A clean run is **240 CTest targets**. Test names carry the environment as a suffix, so a failure
names the width it failed at:

```
test_verbum_scrutor_word16 ....... Passed
test_locus_carcerum_idx16 ........ Passed
```

To run one environment on its own, filter on the suffix:

```sh
ctest --test-dir build -R '_word16$' --output-on-failure
```

## Why these five

`word16` and `idx16` are the two that earn their place most often. A SWAR scan is written once and
runs at whatever the carrier is; the tail — the bytes after the last whole word — is where an
off-by-one hides, and a 16-bit carrier reaches that tail four times sooner than a 64-bit one.
`idx16` exists because a narrow index against a wide word is the combination nothing else covers,
and it is what the static asserts in `embed_types.h` are there to catch.

`checks` is not a width at all. It compiles in the library's checks and selects the trapping
`MMGR_ASSERT`, which turns a broken precondition from a silent no-op into a failed test. It is the
only environment where an assert is evaluated at all, so an expectation that is never exercised there
is one nothing has ever tested.

## A note about earlier names

Two environments called `swar16` and `swar32` used to exist. They set the _scan lane_ narrower than
the machine word — a configuration no real target has, because the carrier is the machine word on
every machine. They were replaced by narrowing the whole machine instead, which is a case that does
exist. If you find `swar16` in a path or a comment, it is stale.

## Adding one

One line in `CMakeLists.txt`:

```cmake
set(MMGR_ENVIRONMENTS
  "host|"
  "word32|EMBED_WORD_BITS=32"
  "word16|EMBED_WORD_BITS=16"
  "idx16|EMBED_INDEX_BITS=16"
  "checks|MMGR_DEBUG_CHECKS=1"
  "yours|YOUR_KNOB=1"           # <- here
)
```

Every module target, every aggregate and every test registration follows from it. Nothing central
lists the modules, so nothing central has to be kept in step.

@note The generated API reference on this site is produced at one configuration — 64-bit word,
32-bit index — because Doxygen must resolve `embed_word` to a single concrete type. The other four
environments differ only in those typedefs. See `PREDEFINED` in `docs/Doxyfile`.
