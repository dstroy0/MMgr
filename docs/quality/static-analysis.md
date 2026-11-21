# The analyzers {#qa_static_analysis}

Four tools run on every push and pull request, and each answers a different question.

## The warning set

Before any analyzer, the compiler. `mmgr_flags` is one interface target that carries the whole set,
and so the library, the tests and the benches all meet the same standard:

```
-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
-Wcast-align -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes
-Wpointer-arith -Wvla
```

`-Wconversion` and `-Wsign-conversion` are the two that earn their place in a library where a width
is a compile-time knob. They catch the implicit narrow conversion that only becomes wrong at
`EMBED_WORD_BITS=16`.

`MMGR_WERROR` turns them into errors. It is off by default, because a warning you are about to fix
should not block a work tree in the middle of an edit. It is on in CI, the wall.

## CodeQL

Advanced setup and not GitHub's default, because CodeQL cannot analyze C from source alone. It has
to observe a real compile to know how the build compiled each translation unit. The workflow does a
traced CMake build and CodeQL extracts the database from it.

It runs the `security-and-quality` suite over two languages:

- **`c-cpp`**: the library.
- **`actions`**: the workflows themselves. They are worth a check precisely because this repository
  auto-merges Dependabot pull requests. A compromised action version would otherwise land
  unreviewed.

## SonarQube

Analysis plus coverage, in one job, and the order of its steps is essential.

C needs the build wrapper for the same reason CodeQL needs a traced build. Without it the scanner
skips the C files, and the dashboard shows a green project that analyzed nothing.

Coverage comes from gcov. The configure step sets `--coverage`, so every translation unit emits
`.gcno` at compile time and `.gcda` when a test runs it. So:

1. configure with `--coverage -O0 -g`
2. build, wrapped
3. **run the whole suite**: no `.gcda` exists until then
4. merge with `gcovr`
5. scan

A scanner invoked before `ctest` reports 0%. The test step is also a real gate. A test that fails
fails the job, and the job does not quietly report the coverage of a broken build.

`sonar.sources` and `sonar.tests` name two sets, so the dashboard does not charge test code against
the library's own duplication and complexity numbers.

## Formatters

Three, one per language, each the one owner of its own files:

| tool         | files                                | config            |
| ------------ | ------------------------------------ | ----------------- |
| clang-format | `src/`, `test/`: `.c` `.h`           | `.clang-format`   |
| black        | `tools/`: `.py`                      | `pyproject.toml`  |
| prettier     | everywhere: md, json, yml, css, html | `.prettierignore` |

All three wrap at **120 columns**. A Python tool and the C it rewrites line up in a side-by-side
diff.

The workflow pins clang-format to 22.1.8 and not to whatever the runner ships. `.clang-format` uses
keys that only exist from version 20, and an older one fails on `unknown key` and not on the layout.
With the pin, CI and a developer's machine agree about what "formatted" means, which an unpinned
formatter cannot promise across versions.

**The job checks and never rewrites.** A formatter that rewrites on CI produces commits nobody
reviewed and races the author's own push. Each step reports the exact command that fixes it locally.
The three run with `continue-on-error`, so one failure does not hide the other two. Three red steps
tell you everything in one run.

## Docs

The documentation has its own gates: Doxygen's warning stream for undocumented public symbols, and
the ASD-STE100 check over the prose. See @ref qa_docs_coverage.

## What none of this catches

Static analysis does not know that a pointer outlived its cell's release. That is the single one
real hazard in this library's model. The compiler and every tool above cannot see it, and the only
defense is the discipline in @ref concept_ownership.
