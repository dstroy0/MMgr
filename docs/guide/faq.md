# Questions this library gets asked {#guide_faq}

## Why Latin names?

So nothing collides. A module called `span` collides with the half-dozen other things in an embedded
codebase already called span, and with C++20's. `spatium` does not. The cost is @ref ref_glossary;
the benefit: no name ever has to change again.

## Why no malloc?

Because the footprint is then a number you can state before the program runs, in place of an
observation about one run. The long answer, with what it costs you, is @ref concept_zero_heap.

## Is it thread-safe?

Yes, with no switch. A cellblock's boundaries, claims and counts are atomic words, each moved in one
step. Every context that reaches a cellblock reaches it at once and does not wait for another. The
context that holds a cell of either tier gives it back, in any order.

The one genuinely concurrent module is `memoria_anularis`, which is single-producer
single-consumer only. Two producers on one ring is broken, and never merely slow. See @ref concept_ownership.

## What does the dispatch table cost?

Nothing, given `const`. The gcc compiler devirtualizes a call through a `static const <Mod>Ns` to
the inlined body, with instructions identical to a direct call of the entry. It does not
devirtualize through a non-const one, which becomes a real call with an 88-byte frame. clang devirtualizes both. For that reason
`EMBED_TABLE_STORAGE` includes `const`. See @ref concept_ns_idiom.

## Do I need LTO?

Not for the hot entries: they are `EMBED_INLINE` in their headers. For cross-module calls that are
not, it is worth a lot: `mmgr_memor_chr` over 512 bytes measured 610 cycles without and 187 with.
`MMGR_LTO` defaults to `ON`.

## Can I use it from C++?

Yes. Each header sits inside `EMBED_BEGIN_DECLS` / `EMBED_END_DECLS`, which is `extern "C"` under a
C++ compiler. The dispatch tables are plain structs of function pointers and work unchanged.

## Why is MinSizeRel not -Os?

Because `-Os` declines to inline the small branch-free entries that _are_ this library. The calls
it saves space on are the calls that cost the max. The top-level `CMakeLists.txt` rewrites `-Os` to
`-O2` for that configuration. An embedded build that reaches for `MinSizeRel` wants small, not
slow-and-small.

## Why are there five builds of everything?

There aren't five builds. One build produces five sets of targets. Each is the library
at a different set of compile-time widths. A defect that only appears at a 16-bit carrier fails a
run on your machine and does not wait for the hardware. One `cmake --build`, one `ctest`. See
@ref ref_environments.

## A test says `test_memoriam_praetereo` was skipped. Is that a problem?

No. `memoriam_praetereo` and `memoria_externa` are behind `MMGR_ENABLE_DMA` and
`MMGR_ENABLE_EXTRAM`, both off by default, and the build skips their suites, loudly, with a CMake
status message. Turn the flag on if you want them built. A capability gates the whole suite, never a
case inside one, because a suite that compiles half its cases away still reports a pass.

## How do I know how big to make my pool?

Measure it, and turn the peak tracker on first. A cellblock's own counters and `buf_available`
report where the tier boundaries are **right now** and never where they were. A read after a
workload tells you about that instant alone.

The peaks are `persistent_hw` and `temporary_hw`, one per tier, and they exist and change only when
`MMGR_ENABLE_HW_MEM_CAPACITY_CB` is on. It is off by default: both stay zero and a reading from
a default build means nothing. Build the `checks` environment with it on, run your real workload,
read them, and size to the peak plus margin. @ref guide_first_region has the procedure.

## Why does my span still work after I released the cell?

Because a minimum security pool scrubs nothing. The memory is readable and holds whatever was
there; the next take will hand it to someone else. A pointer that outlives its release is the sharpest
edge in this library and nothing can detect it for you. @ref concept_ownership.

## Why is there no `strcpy` in the string shim?

There is no bounded form of it: the target size is not one of its arguments. There is nothing for
the shim to map it onto. The shim does not provide the name, and it also defines the `<string.h>`
include guards, so libc's declaration never arrives either. A call to it fails to compile and does
not overflow quietly. Use `strlcpy`, which takes the size. @ref guide_string_shim.

## Why no `__builtin_ctz` or `popcount`?

Measured: the open-coded fold was 3.609 cycles against 3.680 for `__builtin_ctzll`, and
`__builtin_popcountll` compiles to a libgcc call on baseline x86-64. They are not faster, and
without them the scan path stays free of per-compiler `#ifdef`s. @ref concept_swar.

## Is it AGPL even if I only link it?

The license text governs, not this page, and this is not legal advice. @ref proj_license_notes has
the practical notes for a library consumed by `add_subdirectory`. If the terms do not suit your
project, ask: a conversation settles a dual license, with no policy to read first.
