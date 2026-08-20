# Contributing {#proj_contributing}

## Build and test

All five environments are one build. `cmake/MMgrModule.cmake` emits `mmgr_<module>_<env>` for every
entry in `MMGR_ENVIRONMENTS`, so a single configure builds `host`, `word32`, `word16`, `idx16` and
`checks` together, and a single `ctest` run covers all five.

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

A clean run is 80 CTest targets. `test_dma` and `test_confinium_externum` are skipped unless
`MMGR_ENABLE_DMA` or `MMGR_ENABLE_PSRAM_POOL` is set - skipped loudly, with a CMake status message,
because a silently dropped suite leaves a passing run that tested less than it looks like.

## Formatting

Three formatters, one per language, each owning its own files and nothing else. All three wrap at
120 columns, so a Python tool and the C it rewrites line up in a side-by-side diff.

```sh
find src test -name '*.c' -o -name '*.h' | grep -v '^test/vendor/' | xargs clang-format -i
black tools
npm run format
```

CI checks and never rewrites. A formatter that rewrites on CI produces commits nobody reviewed and
races the author's own push; the fix belongs in the working tree.

## Comments

Public headers carry Doxygen comments. The house style is `/** */` blocks with `@brief` first, then
prose that says **why** rather than restating the signature.

State the fact and then the measurement that earns it. Name the alternative a decision beat and the
specific damage it did. Describe failure concretely - no "may", "could", "care should be taken".
Declarative present, no "we", and no praise adjectives: `fast`, `powerful`, `elegant`, `seamless`
and `robust` appear nowhere in this repository and should not start now.

Identifiers, flags, sizes and paths go in backticks. Ordinary words do not, however technical.

**American spelling.** `behavior`, `color`, `initialize`, `serialization`, `analyzer`, `license`.
`cspell.json` rejects the British forms in `README.md` and `docs/**`.

Two things bite in Doxygen comments and both have already happened here:

- A literal backtick inside an indented block opens a code span that never closes and swallows the
  rest of the file. Put ASCII tables and bit layouts inside `@verbatim` / `@endverbatim`.
- `<Mod>` is read as an HTML tag. Wrap angle-bracket placeholders in backticks.

## Joining a group

The API reference is organized by `docs/groups.dox`, which owns every title, brief and ordering.
A header **joins** a group; it never declares one. Two lines, added once and never touched again.

Immediately after `MMGR_BEGIN_DECLS`, before the first declaration:

```c
/**
 * @addtogroup mod_spat
 * @{
 */
```

Immediately before `MMGR_END_DECLS`:

```c
/** @} */
```

The group name is `mod_<stem>` - the stem column of `tools/dev_env/names.tsv`. `docs/groups.dox`
already declares it. Do not add a `@defgroup` to a header.

The `@file` block also carries `@ingroup mod_<stem>` so the header itself is listed in its group:

```c
/**
 * @file spatium.h
 * @ingroup mod_spat
 * @brief Bounded views over caller memory. A span owns nothing and never allocates.
 */
```

## Adding a module

1. A directory under `src/`.
2. A three-line `CMakeLists.txt` calling `mmgr_add_module()`. Nothing central lists the modules, so
   there is no registry to keep in step.
3. One line in `src/CMakeLists.txt`.
4. One `@defgroup` in `docs/groups.dox`, placed where it belongs in the data path.
5. One guide in `docs/modules/`.
6. A row in `tools/dev_env/names.tsv`.

## Documentation

Anything in `docs/` that can be derived from the tree **is** derived from the tree and lives inside
a generated region. Regenerate before committing:

```sh
python -m tools.ci_tooling.ci gen
python -m tools.ci_tooling.ci check
```

Never hand-edit between `<!-- BEGIN GENERATED ... -->` and `<!-- END GENERATED ... -->`. The marker
names the generator that owns it.
