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

A clean run is 80 CTest targets. `test_memoriam_praetereo` and `test_confinium_externum` are skipped unless
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

Public headers carry Doxygen comments. The house style is ````

Immediately before `MMGR_FINIS_DECLS`:

```c
```

The group name is `mod_<stem>` - the stem column of `tools/dev_env/names.tsv`. `docs/groups.dox`
already declares it. Do not add a `@defgroup` to a header.

The `@file` block also carries `@ingroup mod_<stem>` so the header itself is listed in its group:

```c
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
