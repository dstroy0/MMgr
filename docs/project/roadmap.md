# Roadmap {#proj_roadmap}

What is next, and what is deliberately not.

## Next

**Interop suites.** `test/interop/` is a directory with a README and no suites. An interop suite
checks the wire formats against another implementation, and there is not one yet. The integration
directory holds `test_hostile_content`, `test_praet_correctness`, `test_read_bounds`,
`test_ring_bounds`, `test_scan_reference`, `test_stream_lexer`, `test_tenant_lifecycle`,
`test_text_pipeline` and `test_wire_roundtrip`.

**Documented dispatch loculi, or a decision not to.** About 180 symbols with no documentation are
the members of the `<Mod>Ns` tables. Either each gets its documentation, or the reference makes it
obvious that a loculus's prose lives on the free function it points at. See @ref qa_docs_coverage.

**A generator toolchain for the derived documentation.** The module table, the configuration table,
the glossary and the symbol index all derive from the tree. A person edits them by hand, and a copy
made by hand drifts from the tree.

## Later

**A second concurrency shape.** The ring is single-producer single-consumer. Multi-producer is a
different data structure, not a flag on this one, and it should stay out until something needs it.

**More anchor profiles.** The byte-frequency tables for substring search cover generic, English,
URI, inet and route. A new one is a header and a define; the only question is whether a workload
justifies it.

## Deliberately not

A page of non-goals is worth more than a page of goals, because it stops arguments.

**A general allocator.** No free lists and no size classes. If a workload
needs those, it needs a heap, and MMgr is the wrong shape for it. A general free would take
away the property the whole library exists for: a footprint decided before the program runs.

**Runtime configuration.** Every knob is a compile-time define. A runtime-sized pool would move
allocation failure from configure time back to run time, the very thing this design avoids.

**`install()` rules and a binary artifact.** The ABI is a function of the compile-time widths. A
prebuilt archive can silently disagree with its consumer about the size of `embed_word`. A consumer
that compiles the source cannot have that defect. See @ref guide_install.

**Compiler intrinsics in the scan path.** Measured and rejected. See @ref concept_swar. Beyond the
performance answer, to retain them out retains every compiler conditional in one file.

**Locks.** Contexts share a cellblock on atomic words: each boundary, claim and count moves in one
step, and no context waits for another. A lock inside a bump allocator is a lock on the hot path.
The whole claim of this library is a cost with a bound that you can predict, and so there is no lock.

## Versioning

The version is 2.3.0 and the API is stable. It is in five places: `CMakeLists.txt`,
`library.json`, `library.properties`, `CITATION.cff` and `sonar-project.properties`, with nothing
to retain them in step. A generator for them is open work.
