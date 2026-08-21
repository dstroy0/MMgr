# Roadmap {#proj_roadmap}

What is next, and what is deliberately not.

## Next

**Interop suites.** `test/interop/` is a directory with a README and no suites, because interop
means checking the wire formats against another implementation and there is not one yet. The
integration directory is no longer empty: `test_read_bounds`, `test_hostile_content`,
`test_scan_reference`, `test_tenant_lifecycle`, `test_text_pipeline` and `test_wire_roundtrip` all
live there.

**Documented dispatch loculi, or a decision not to.** ~180 undocumented symbols are the members of the
`<Mod>Ns` tables. Either they get documented, or the reference makes it obvious that a loculus's prose
lives on the free function it points at. See @ref qa_docs_coverage.

**A generator toolchain for the derived documentation.** The module table, the configuration table,
the glossary and the symbol index are all derivable from the tree, and are currently maintained by
hand — which is exactly how "seventeen small modules" survived in a comment until there were
nineteen.

## Done since this was written

**The unit suites are no longer thin.** 150 targets at 100% of lines, branches and functions, and
the whole suite runs a second time with `MMGR_ORACLE_LIBC` on so libc answers the same questions.
What that turned up is in @ref qa_testing and @ref qa_numeric — a parser that was returning the
right double 16% of the time, a search that read up to seven bytes past its cap, an exponent that
silently became zero past 1e308.

**Per-unit optimisation levels.** `mmgr_add_module` takes `OPTIMIZE`, and the measurement behind
each use of it is in @ref qa_optimisation.

## Later

**`MMGR_CALL`.** The designated-initializer call convention is defined and documented in
`mmgr_compiler_directives.h`, with measured instruction counts, and is used by nothing. It reads as
an intended future API shape rather than dead code, but it is unproven until a module adopts it.

**A second concurrency shape.** The ring is single-producer single-consumer. Multi-producer is a
different data structure, not a flag on this one, and it should stay out until something needs it.

**More anchor profiles.** The byte-frequency tables for substring search cover generic, English,
URI, inet and route. Adding one is a header and a define; the question is only whether a workload
justifies it.

## Deliberately not

A page of non-goals is worth more than a page of goals, because it is the one that stops arguments.

**A general allocator.** No free lists, no size classes, no arbitrary-order release. If a workload
needs those, it needs a heap, and MMgr is the wrong shape for it. Adding a general free would take
away the property the whole library exists for — a footprint decided before the program runs.

**Runtime configuration.** Every knob is a compile-time define. A runtime-sized pool would move
allocation failure from configure time back to run time, which is the thing being avoided.

**`install()` rules and a binary artifact.** The ABI is a function of the compile-time widths, so a
prebuilt archive can silently disagree with its consumer about the size of `mmgr_word`. Consuming
the source makes that impossible. See @ref guide_install.

**Compiler intrinsics in the scan path.** Measured and rejected — see @ref concept_swar. Beyond the
performance answer, keeping them out is what keeps every compiler conditional in one file.

**MSVC support.** The target is embedded C11 toolchains. `/W4` does not mean what `-Wall -Wextra`
means and there is no `-Wconversion` equivalent worth gating on, so keeping it green would buy a
platform nobody ships this to.

**Thread-safe everything.** A region is handed to whoever holds it, and two contexts that must not
share get two regions; that is the concurrency model. A
lock inside a bump allocator would be a lock on the hot path of a library whose whole claim is
bounded, predictable cost.

## Versioning

Pre-1.0, so the API may change. It is currently 0.1.0 in five places —
`CMakeLists.txt`, `library.json`, `library.properties`, `CITATION.cff` and
`sonar-project.properties` — with nothing keeping them in step. That wants a generator before it
wants a 1.0.
