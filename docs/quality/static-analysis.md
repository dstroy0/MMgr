# The analyzers {#qa_static_analysis}

Four tools run on every push and pull request, each answering a different question.

## The warning set

Before any analyzer, the compiler. `mmgr_flags` is one interface target carrying the whole set, so
the library, the tests and the benches are all held to the same standard:

```
-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
-Wcast-align -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes
-Wpointer-arith -Wvla
```

`-Wconversion` and `-Wsign-conversion` are the two that earn their place in a library where a width
is a compile-time knob: they catch the implicit narrowing that only becomes wrong at
`MMGR_WORD_BITS=16`.

`MMGR_WERROR` turns them into errors. It is off by default, because a working tree mid-edit should
not be blocked by a warning you are about to fix, and on in CI, which is the wall.

## CodeQL

Advanced setup rather than GitHub's default, because C cannot be analyzed from source alone —
CodeQL has to observe a real compile to know how each translation unit was built. The workflow does a
traced CMake build and CodeQL extracts the database from it.

It runs the `security-and-quality` suite over two languages:

- **`c-cpp`** — the library.
- **`actions`** — the workflows themselves. Worth having precisely because this repository
  auto-merges Dependabot pull requests, so a compromised action version would otherwise land
  unreviewed.

## SonarQube

Analysis plus coverage, in one job, and the order of its steps is load-bearing.

C needs the build wrapper for the same reason CodeQL needs a traced build: without it the C files are
skipped and the dashboard shows a green project that analyzed nothing.

Coverage comes from gcov. The build is configured with `--coverage` so every translation unit emits
`.gcno` at compile time and `.gcda` when a test runs it. So:

1. configure with `--coverage -O0 -g`
2. build, wrapped
3. **run the whole suite** — no `.gcda` exists until then
4. merge with `gcovr`
5. scan

A scanner invoked before `ctest` reports 0%. The test step is also a real gate: a failing test fails
the job rather than quietly reporting the coverage of a broken build.

`sonar.sources` and `sonar.tests` are split so the dashboard does not charge test code against the
library's own duplication and complexity numbers.

## Formatters

Three, one per language, each owning its own files:

| tool         | files                                 | config            |
| ------------ | ------------------------------------- | ----------------- |
| clang-format | `src/`, `test/` — `.c` `.h`           | `.clang-format`   |
| black        | `tools/` — `.py`                      | `pyproject.toml`  |
| prettier     | everywhere — md, json, yml, css, html | `.prettierignore` |

All three wrap at **120 columns**, so a Python tool and the C it rewrites line up in a side-by-side
diff.

clang-format is pinned to 20.1.7 rather than whatever the runner ships: `.clang-format` uses keys
that only exist from version 20, and an older one failed on `unknown key` rather than on any actual
formatting. Pinning also means CI and a developer's machine agree about what "formatted" means,
which an unpinned formatter cannot promise across versions.

**The job checks and never rewrites.** A formatter that rewrites on CI produces commits nobody
reviewed and races the author's own push. Each step reports the exact command that fixes it locally,
and the three run with `continue-on-error` so one failure does not hide the other two — three red
steps tell you everything in one run.

## Docs

The documentation has its own gates: Doxygen's warning stream for undocumented public symbols, and
cspell over the prose. See @ref qa_docs_coverage.

## What none of this catches

Static analysis does not know that a pointer outlived its mark. That is the one real hazard in this
library's model, it is invisible to the compiler and to every tool above, and the only defense is the
discipline in @ref concept_ownership.
