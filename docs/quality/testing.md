# The test suite {#qa_testing}

Eighty CTest targets, in about two seconds, covering every compile-time width.

## Running it

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

One environment at a time:

```sh
ctest --test-dir build -R '_word16$' --output-on-failure
```

## Layout

```
test/unit/<module>/test_<name>/test_<name>.c    one per module
test/environment/test_<env>/test_<env>.c        one per environment
test/integration/                               across modules
test/interop/                                   against other implementations
test/support/platform_host.c                    supplies mmgr_platform_context_id()
test/bench/                                     benchmarks, off by default
```

A suite is a **directory holding exactly one `.c`** with file-scope `void test_<name>(void)` cases.
That constraint is what lets the harness find them without a registry.

## Unity, fetched not vendored

Unity v2.6.1 arrives through CMake's `FetchContent` with `GIT_SHALLOW`. It is not committed, which
is why `sonar-project.properties` and the CodeQL config exclude a `test/vendor/**` path that does
not exist — that exclusion is vestigial.

Tests deliberately do **not** link `mmgr_flags`. Unity's assertion macros do not survive
`-Wconversion`, and the choice is between weakening the warning set for the library or accepting
that the test binaries build with a smaller one. The library's warnings are the ones that matter.

## One suite, five environments

`test/CMakeLists.txt` registers each unit suite once per entry in `MMGR_ENVIRONMENTS`, linked
against that environment's aggregate:

```
add_test(NAME test_spatium_host   ...)
add_test(NAME test_spatium_word32 ...)
add_test(NAME test_spatium_word16 ...)
```

so a defect that only appears at a 16-bit carrier fails a run here rather than waiting for someone
to own that hardware. The environment suites themselves are built only in their own environment, and
assert that the widths actually reached the translation unit.

## harness.py

`test/harness.py` generates the Unity runners by shelling out to Unity's own
`generate_test_runner.rb`, regenerating whenever a source changes. It also has three commands worth
knowing:

| command  | answers                                             |
| -------- | --------------------------------------------------- |
| `suites` | what suites exist, and whether the suite map agrees |
| `cases`  | which cases Unity's regex will silently walk past   |
| `deps`   | which suites bind a capability they never assert on |

`suites --strict` and `deps --strict` exit non-zero on a finding, which is what makes them usable as
CI gates.

A suite that borrows a region and never drives it looks covered and is not — that is what `deps`
is for.

## Capability gating

`test_dma` needs `MMGR_ENABLE_DMA`; `test_confinium_externum` needs `MMGR_ENABLE_PSRAM_POOL`. Both
default off, so both are skipped — **loudly**, with a CMake status message:

```
-- MMgr: skipping test_dma (needs MMGR_ENABLE_DMA)
```

Silently dropping them would leave a passing run that tested less than it looks like, which is worse
than a red one.

## What the suites actually assert

Honest answer: the unit suites are currently mostly thin. Many assert that the header compiled with
nothing before it and that the namespace instance is its own type — real properties, but shallow
ones. `test_cellularum_laboro` is the substantial exception.

The environment suites are the ones carrying weight today: they assert the widths reached the
translation unit, which is what the whole five-environment build exists to check.

Deepening the unit suites is the largest open piece of work in this repository. See @ref proj_roadmap.

## Coverage

Coverage is produced by the SonarQube workflow, not by a separate job: it configures with
`--coverage -O0 -g`, builds, runs the full suite to produce the `.gcda` files, and merges them with
`gcovr`. The order is load-bearing — no `.gcda` exists until the tests have run, so a scanner
invoked before `ctest` reports zero.
