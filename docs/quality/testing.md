# The test suite {#qa_testing}

276 CTest targets over every compile-time width, at 100% of lines, branches and functions, green
against this library and against libc.

## Running it

The harness carries the flags. Each run is a command, never a remembered incantation:

```sh
python test/harness.py test            # build, then run
python test/harness.py ab              # both sides of the A/B, one after the other
python test/harness.py coverage        # what of src/ the suites reached
```

One environment, or one suite:

```sh
python test/harness.py test --filter '_word16$'
```

The plain cmake and ctest still work, but the trees differ in ways you should not have to
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

A suite is a **directory that holds exactly one `.c`** with file-scope `void test_<name>(void)`
cases. The harness finds them by that shape, with no registry.

## Unity, fetched not vendored

Unity v2.6.1 arrives through CMake's `FetchContent` with `GIT_SHALLOW`. The tree does not hold it.
`sonar-project.properties` and the CodeQL config exclude a `test/vendor/**` path that does not exist,
and the exclusion matches nothing.

Tests deliberately do **not** link `mmgr_flags`. Unity's assertion macros do not survive
`-Wconversion`. The choice is a weaker warning set for the library, or test binaries that build with
a smaller one. The library's warnings are the ones that matter.

## One suite, five environments

`test/CMakeLists.txt` registers each unit suite once per entry in `MMGR_ENVIRONMENTS`, linked
against that environment's aggregate:

```
add_test(NAME test_spatium_host   ...)
add_test(NAME test_spatium_word32 ...)
add_test(NAME test_spatium_word16 ...)
```

A defect that only appears at a 16-bit carrier fails a run here, before anyone owns that hardware.
The build compiles each environment suite only in its own environment, and the suite asserts that
the widths actually reached the translation unit.

## harness.py

`test/harness.py` generates the Unity runners through Unity's own `generate_test_runner.rb`, and it
generates them again whenever a source changes. It also has these commands:

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

`suites --strict`, `deps --strict` and `generated --strict` exit non-zero on a finding. A CI job
gates on that exit.

Six build trees, each a different question, and each carries its own flags in the harness instead
of in somebody's shell history:

| tree             | what it is                                                         |
| ---------------- | ------------------------------------------------------------------ |
| `build`          | the library as it ships                                            |
| `build-oracle`   | every entry with a libc equivalent replaced by that equivalent     |
| `build-cov`      | instrumented, with `always_inline` and link time optimization off  |
| `build-dma`      | DMA on, with recovery, the boundary word check and a settle window |
| `build-dma-lean` | DMA on, with recovery and the settle window off                    |
| `build-extram`   | external memory on                                                 |

The two DMA trees answer every schedule knob through `MMGR_PRAET_KNOBS` (`PRAET_KNOBS` in
`test/harness.py`). A context the suites declare has to be the size the library walks, and the knobs
reach the library and the suites together. The cases gate on the knobs, and each tree compiles the
arm the other one leaves out. The Capabilities workflow runs the two DMA trees and `build-extram` on
every pull request, because the analysis job builds with every capability off.

`build-oracle` and `build-cov` matter for coverage. The compiler honors `always_inline` at `-O0`.
With it on, every call site of a header entry gets its own copy of that entry's branch records. The
report then counts optimizer copies and not source branches: `mmgr_ascii_in` is one condition on one
line and reports 28 branches. Link time optimization rewrites the code across translation units
before anything reads the counters, and the report then measures something nobody wrote.

`ab` runs the two sides one after the other, never at once. Two full builds at the same time make
the machine unusable, and the comparison does not need them concurrent.

A suite that borrows a region and never drives it looks covered and is not, and `deps` reports it.

## Capability gating

`test_memoriam_praetereo` and `test_praet_correctness` need `MMGR_ENABLE_DMA`.
`test_memoria_externa` and `test_memoria_externa_accuracy` need `MMGR_ENABLE_EXTRAM`. Both knobs
default off (`CMakeLists.txt`). The build skips all four suites **loudly**, with a CMake status
message that names the capability that turned each one off:

```
-- MMgr: 4 suites NOT built, their capability is off:
-- MMgr:   test_memoria_externa (MMGR_ENABLE_EXTRAM=OFF)
-- MMgr:   test_memoriam_praetereo (MMGR_ENABLE_DMA=OFF)
-- MMgr:   test_memoria_externa_accuracy (MMGR_ENABLE_EXTRAM=OFF)
-- MMgr:   test_praet_correctness (MMGR_ENABLE_DMA=OFF)
```

A silent drop of them would leave a green run that tested less than it looks like, which is worse
than a red one.

## What the suites actually assert

The suites execute every line and every branch of `src/`, and they call every function. That is a
floor, not a claim of correctness. A case can run a line and assert nothing useful about it, so
what follows is what the cases actually check.

**libc is the oracle wherever it has one.** The build compiles the whole suite a second time with
`MMGR_ORACLE_LIBC` on, which replaces every entry that has a libc equivalent with that equivalent.
Both runs have to be green, and in some places the two deliberately differ. One example is a rule
for the last digit that C leaves to the implementation. Others are a bound libc does not have and
the platform's text for nan. There the case pins this library, and `MMGR_SKIP_ON_ORACLE` stands it down on the other run, with the
reason in the call.

**Nothing reads past its bound.** A poison pattern cannot catch a read, because a read leaves
nothing behind. `test_read_bounds` puts the buffer's last byte against a page marked no-access and
catches the trap. A load one byte too far shows there, and it never crashes somewhere else later. It
holds `len`, `chr`, `eq`, `starts`, `diff`, `copy` and `take_be` to the word-rounded cap, and `find`
and `has` to the raw cap with nothing rounded.

**Hostile content, inside legitimate bounds.** Sizes here are the caller's own and bound at compile
time. A nonsense size is not an input this library accepts. What arrives from outside is the bytes.

`test_hostile_content` drives runs that never terminate, a terminator walked through every lane,
and every start alignment. It drives bytes with the high bit set through a compare that is only
exact below it. It drives needles that end at the hay's last byte or are longer than the hay. It
also walks builders and spans to their caps with a poison fence on each side.

**A reference that is correct by a read of it.** `test_scan_reference` checks the scans against a
byte at a time loop written out in the test. It covers every length to 48, and the three on each
side of each word boundary to 300. Not libc: the oracle run asks libc on its own, and half of these
entries take a cap libc has no equivalent for.

**Numbers, against a right answer.** A double printed to seventeen digits names exactly one double,
and there is no tolerance to argue about. See @ref qa_numeric for what that found and what it still
says is wrong.

**The insides, where the outside cannot reach.** `test_cellularum_internals` compiles the
translation unit in and does not link it, and so its file-local entries are callable. There, a case
executes a carry that occurs once in 2^63 multiplies, on operands the case solves for.

**Every width is a separate implementation.** `MMGR_ENVIRONMENTS` decides which arm of a width
conditional the build compiles. The five environments are five builds of the same source, and all
five run. It answers what glibc gets from a test of every ifunc variant and not only the one the
processor selected.

What the suites deliberately do _not_ sweep is start alignment. The glibc functions sweep it because
they peel: single bytes until the pointer reaches a word boundary, then whole words. So the alignment
picks which code runs. Nothing here peels: every load in the scans is at whatever address
it gets. One case states that property, and no case hunts a bug that cannot exist. The bulk move in
`memoria_operor` does peel, and `test_memoria_operor` sweeps both offsets against every length there.

## Coverage

`python test/harness.py coverage` builds the instrumented tree, clears the counters of the last run,
runs every environment and reports. 100% of lines, branches and functions.

The counters carry a stamp from the `.gcno` the build wrote beside them. The harness clears them
after the build and before the run, and so a report describes the binaries that produced it.

Every environment runs, host and the four others. Which arm of a width conditional exists at all is
`MMGR_ENVIRONMENTS`' business. A line dead at 64 bits is live at 16, and only the whole set
describes the library.

A branch that no configured environment can reach carries a `GCOVR_EXCL` marker with the reason on
it. Examples are a debug assertion that needs `MMGR_DEBUG_CHECKS` and an arm that only exists on a
big endian target. Another is a guard its own invariants will not let fire. Each says what would make it live
again. A build option that changes the answer takes the marker with it.

The same numbers come out of the SonarQube workflow, which configures with the same flags. The order
there matters: no `.gcda` exists until the tests run. A scanner invoked before `ctest` reports zero.
