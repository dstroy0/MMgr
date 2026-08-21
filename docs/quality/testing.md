# The test suite {#qa_testing}

150 CTest targets covering every compile-time width, at 100% of lines, branches and functions,
green against this library and against libc.

## Running it

The harness carries the flags, so running it is a command rather than a remembered incantation:

```sh
python test/harness.py test            # build, then run
python test/harness.py ab              # both sides of the A/B, one after the other
python test/harness.py coverage        # what of src/ the suites reached
```

One environment, or one suite:

```sh
python test/harness.py test --filter '_word16$'
```

The underlying cmake and ctest still work, but the trees differ in ways worth not having to
remember - see [the harness](#harnesspy) below.

## Layout

```
test/unit/<module>/test_<name>/test_<name>.c    one per module
test/environment/test_<env>/test_<env>.c        one per environment
test/integration/                               across modules
test/interop/                                   against other implementations
test/support/platform_host.c                    the host side of the environment shims
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

| command     | answers                                              |
| ----------- | ---------------------------------------------------- |
| `build`     | configure if needed, then build                      |
| `test`      | build, then run the suites                           |
| `ab`        | both sides of the A/B, one after the other           |
| `coverage`  | what of `src/` the suites reached                    |
| `suites`    | what suites exist, and whether the suite map agrees  |
| `cases`     | which cases Unity's regex will silently walk past    |
| `deps`      | which suites bind a capability they never assert on  |
| `generated` | whether the generated headers match their generators |

`suites --strict`, `deps --strict` and `generated --strict` exit non-zero on a finding, which is
what makes them usable as CI gates.

Three build trees, each a different question, and each carries its own flags in the harness rather
than in somebody's shell history:

| tree           | what it is                                                        |
| -------------- | ----------------------------------------------------------------- |
| `build`        | the library as it ships                                           |
| `build-oracle` | every entry with a libc equivalent replaced by that equivalent    |
| `build-cov`    | instrumented, with `always_inline` and link time optimisation off |

The last two matter. `always_inline` is honoured at `-O0`, so without turning it off every call site
of a header entry gets its own copy of that entry's branch records and the report counts optimiser
copies instead of source branches - `mmgr_ascii_in` is one condition on one line and came back
holding 28 branches. Link time optimisation rewrites the code across translation units before the
counters are read, which measures something nobody wrote.

`ab` runs the two sides one after the other, never at once: two full builds at the same time makes
the machine unusable and the comparison does not need them concurrent.

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

Every line and every branch of `src/` is executed by them, and every function is called. That is a
floor, not a claim of correctness - a line can be run by a case that asserts nothing useful about
it - so what follows is what the cases are actually checking.

**libc is the oracle wherever it has one.** The whole suite is built a second time with
`MMGR_ORACLE_LIBC` on, which replaces every entry that has a libc equivalent with that equivalent.
Both runs have to be green. Where the two deliberately differ - a rounding rule C leaves to the
implementation, a bound libc does not have, the platform's spelling of nan - the case is pinned
against this library and stood down on the other run by `MMGR_SKIP_ON_ORACLE`, with the reason in
the call. Six cases do that today.

**Nothing reads past its bound.** A poison pattern cannot catch a read, because a read leaves
nothing behind. `test_read_bounds` puts the buffer flush against a page marked no-access and catches
the trap, so a load one byte too far is reported rather than being someone else's crash later. It
holds `len`, `chr`, `eq`, `starts`, `diff`, `copy` and `take_be` to the word-rounded cap that
`MMGR_SCAN_MAX_WORDS` reserves, and `find` and `has` to the raw cap with nothing rounded.

**Hostile content, inside legitimate bounds.** Sizes here are the caller's own and bound at compile
time, so a nonsense size is not an input this library accepts. What arrives from outside is the
bytes. `test_hostile_content` drives runs that never terminate, a terminator walked through every
lane, every start alignment, bytes with the high bit set through a compare that is only exact below
it, needles flush with the end and longer than the hay, and builders and spans walked to their caps
with a poison fence either side.

**A reference that is correct by reading it.** `test_scan_reference` checks the scans against a byte
at a time loop written out in the test, over every length to 48 and the three either side of each
word boundary to 300. Not libc: the oracle run asks libc separately, and half of these entries take
a cap libc has no equivalent for.

**Numbers, against a right answer.** A double printed to seventeen digits names exactly one double,
so there is no tolerance to argue about. See @ref qa_numeric for what that found and what it still
says is wrong.

**The insides, where the outside cannot reach.** `test_cellularum_internals` compiles the
translation unit in rather than linking it, so file-local entries are callable. It is where a carry
that happens once in 2^63 multiplies gets executed, on operands that were solved for rather than
found.

**Every width is a separate implementation.** `MMGR_ENVIRONMENTS` decides which arm of a width
conditional is compiled at all, so the five environments are five builds of the same source and all
five run. That is this library's answer to what glibc gets from testing every ifunc variant rather
than only the one the processor selected.

What is deliberately _not_ swept is start alignment. glibc sweeps it because its string functions
peel - single bytes until the pointer reaches a word boundary, then whole words - so the alignment
picks which code runs. Nothing here peels: every load in the scans is at whatever address it was
handed. One case states that property rather than hunting a bug that cannot exist. The one place
this library does peel is the bulk move in `memoria_operor`, and `test_memoria_operor` sweeps both
offsets against every length there.

## Coverage

`python test/harness.py coverage` builds the instrumented tree, clears last run's counters, runs
every environment and reports. 100% of lines, branches and functions.

The counters carry a stamp from the `.gcno` they were built beside, so they are cleared after the
build and before the run: a report always describes the binaries that produced it.

Every environment runs, not just host. Which arm of a width conditional exists at all is
`MMGR_ENVIRONMENTS`' business, so a line dead at 64 bits is live at 16 and only the whole set
describes the library.

Where a branch cannot be reached at any configured environment it carries a `GCOVR_EXCL` marker with
the reason on it - a debug assertion that needs `MMGR_DEBUG_CHECKS`, an arm that only exists on a
big endian target, a guard its own invariants will not let fire. Each says what would make it live
again, so a build option that changes the answer takes the marker with it.

The same numbers come out of the SonarQube workflow, which configures with the same flags. The order
there is load-bearing — no `.gcda` exists until the tests have run, so a scanner invoked before
`ctest` reports zero.
