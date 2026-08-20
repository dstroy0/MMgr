# Roadmap {#proj_roadmap}

What is next, and what is deliberately not.

## Next

**Deepen the unit suites.** This is the largest open piece of work in the repository and it is worth
being blunt about: most unit suites currently assert that the header compiled and that the namespace
instance is its own type. Those are real properties but shallow ones.
`test_cellularum_laboro` is the exception and is the shape the rest should take — real inputs, real
edge cases, the tail of every scan.

**Integration and interop suites.** `test/integration/` and `test/interop/` are directories with
READMEs and no suites. Integration is where a confinium, a pool and a ring get exercised together
rather than one at a time; interop is where the wire formats get checked against another
implementation rather than against themselves.

**Documented dispatch slots, or a decision not to.** ~180 undocumented symbols are the members of the
`<Mod>Ns` tables. Either they get documented, or the reference makes it obvious that a slot's prose
lives on the free function it points at. See @ref qa_docs_coverage.

**A generator toolchain for the derived documentation.** The module table, the configuration table,
the glossary and the symbol index are all derivable from the tree, and are currently maintained by
hand — which is exactly how "seventeen small modules" survived in a comment until there were
nineteen.

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

**Thread-safe everything.** Pools are partitioned by worker slot; that is the concurrency model. A
lock inside a bump allocator would be a lock on the hot path of a library whose whole claim is
bounded, predictable cost.

## Versioning

Pre-1.0, so the API may change. It is currently 0.1.0 in five places —
`CMakeLists.txt`, `library.json`, `library.properties`, `CITATION.cff` and
`sonar-project.properties` — with nothing keeping them in step. That wants a generator before it
wants a 1.0.
