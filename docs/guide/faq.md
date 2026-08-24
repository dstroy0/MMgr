# Questions this library gets asked {#guide_faq}

## Why Latin names?

So nothing collides. A module called `span` collides with the half-dozen other things in an embedded
codebase already called span, and with C++20's. `spatium` does not. The cost is @ref ref_glossary;
the benefit is never renaming anything again.

## Why no malloc?

Because the footprint is then a number you can state before the program runs, rather than an
observation about one run. The long answer, including what it costs you, is @ref concept_zero_heap.

## Is it thread-safe?

The region synchronizes nothing, because there is nothing to synchronize: a `CarcerCtx` is a base, an
extent and two cursors, used by whoever holds it. Two contexts that must not share get two regions,
and the region never learns there were two.

The one genuinely concurrent module is `confinium_exclusivum_infinitas`, which is single-producer
single-consumer only. Two producers on one ring is broken, not slow. See @ref concept_ownership.

## What does the dispatch table cost?

Nothing, given `const`. gcc devirtualizes a call through a `static const <Mod>Ns` to the inlined
body — identical instructions to calling the entry directly — and does not devirtualize through a
non-const one, which becomes a real call with an 88-byte frame. clang devirtualizes both. That is
why `MMGR_NS` includes `const`. See @ref concept_ns_idiom.

## Do I need LTO?

Not for the hot entries — they are `MMGR_INLINE` in their headers. For cross-module calls that are
not, it is worth a lot: `mmgr_memor_chr` over 512 bytes measured 610 cycles without and 187 with.
`MMGR_LTO` defaults to `ON`.

## Can I use it from C++?

Yes. Headers are wrapped in `MMGR_INCIPE_DECLS` / `MMGR_FINIS_DECLS`, which is `extern "C"` under a C++
compiler. The dispatch tables are plain structs of function pointers and work unchanged.

## Why is MinSizeRel not -Os?

Because `-Os` declines to inline the small branch-free entries that _are_ this library, so the calls
it saves space on are the calls that cost the most. The top-level `CMakeLists.txt` rewrites `-Os` to
`-O2` for that configuration. An embedded build reaching for `MinSizeRel` wants small, not
slow-and-small.

## Why are there five builds of everything?

There aren't five builds — there is one build that produces five sets of targets. Each is the library
at a different set of compile-time widths, so a defect that only appears at a 16-bit carrier fails a
run on your machine instead of waiting for the hardware. One `cmake --build`, one `ctest`. See
@ref ref_environments.

## A test says `test_memoriam_praetereo` was skipped. Is that a problem?

No. `memoriam_praetereo` and `confinium_externum` are behind `MMGR_ENABLE_DMA` and
`MMGR_ENABLE_EXTRAM`, both off by default, so their suites are skipped — loudly, with a CMake status
message. Turn the flag on if you want them built. A capability gates the whole suite, never a case
inside one, because a suite that compiles half its cases away still reports as passing.

## How do I know how big to make my region?

Measure it, and turn the peak tracking on first. `carcer.persist_used` and `carcer.octas_praesto`
report where the cursors are **right now**, not where they have been, so reading them after a
workload tells you about that instant and nothing else.

The peak is `pool->hw`, and it only exists and is only maintained when
`MMGR_ENABLE_HW_MEM_CAPACITY_CB` is on. It is off by default, so `hw` stays zero and a reading from
a default build means nothing. Build the `checks` environment with it on, run your real workload,
read `hw`, and size to it plus margin. @ref guide_first_region has the procedure.

## Why does my span still work after I released the mark?

Because nothing was scrubbed. The memory is readable and holds whatever was there; the next take
will hand it to someone else. A pointer that outlives its mark is the sharpest edge in this library
and nothing can detect it for you. @ref concept_ownership.

## Why is there no `strcpy` in the string shim?

There is no bounded spelling of it — the destination size is not one of its arguments, so there is
nothing for the shim to map it onto. The name is simply not provided, and the shim also defines the
`<string.h>` include guards so libc's declaration never arrives either. A call to it fails to
compile instead of overflowing quietly. Use `strlcpy`, which takes the size. @ref guide_string_shim.

## Why no `__builtin_ctz` or `popcount`?

Measured: the open-coded fold was 3.609 cycles against 3.680 for `__builtin_ctzll`, and
`__builtin_popcountll` compiles to a libgcc call on baseline x86-64. They were not faster, and
avoiding them keeps the scan path free of per-compiler `#ifdef`s. @ref concept_swar.

## Is it AGPL even if I only link it?

The license text governs, not this page, and this is not legal advice. @ref proj_license_notes has
the practical notes for a library consumed by `add_subdirectory`. If the terms do not suit your
project, ask — dual licensing is a conversation, not a policy.
