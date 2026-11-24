# MMgr

MMgr is a memory manager in C11 that uses no heap. A pool is a block of bytes that you declare. Use
`ParsMemoriaeInternae` for internal memory. Use `ParsMemoriaeExternum` for external RAM, where the part
has external RAM and you set `MMGR_ENABLE_EXTRAM`. The declaration of a pool sets the memory that the
pool is in. No later step reads an address to find the memory.

Use `ParsMemoriaeHospes` for a pool whose bytes the system gives at run time. A cellblock over it takes
`MMGR_MINIMUM_SECURITY_HOSPES` or `MMGR_MAXIMUM_SECURITY_HOSPES` in its declaration. The program places the cellblock once with
`mmgr_series_cellularum_placed` before the first take, and the cellblock refuses every take before that. A
program places a cellblock there so that `mmgr_series_cellularum_idle_each` can give idle pages back to the
system. An array in the program's image does not allow that.

`LocusCarcerum` makes pools into a prison site. Each cellblock uses the pool that its declaration names.
The warden is a const struct with the name of the site. The warden holds the cellblocks as its members,
and each call goes through the warden.

The compiler initializes the warden and the cellblocks as data. Nothing runs at startup. The footprint
is a number that you can compare with a budget.

A pool name identifies one region. Two declarations of one name in one translation unit do not compile.
Two declarations in two units do not link. Two dresses of one pool do not compile. Each of these
failures gives a diagnostic that identifies the error.

A cellblock gives cells from its two ends. The persistent tier goes up from the base. The temporary tier
goes down from the size. The gap between the two tiers is the space that the two tiers can still take.
The two boundaries are the two halves of one atomic word. A take moves its own half with one fetch-add,
and the old value of that fetch-add holds the other half (`src/locus_carcerum/locus_carcerum.h:20-22`).

By default the temporary tier is one stack that every thread shares. Set `MMGR_CARCER_REGIONS` above 0,
and the temporary tier becomes that many regions of `MMGR_CARCER_REGION_BYTES` each, at the top of the
cellblock. A take goes to the region that `embed_thread_place()` selects, modulo the count. A give goes
to the region that holds the cell, from any thread. A take over a region's room goes to the large region
of `MMGR_CARCER_LARGE_REGION_BYTES`, which every thread shares, and a size of 0 refuses that take.
`Regions.lean` proves that no region's depth passes its size (`src/locus_carcerum/locus_carcerum.h:104-141`).

A cell from the persistent tier is the property of the caller [RETURNS OWNERSHIP]. The caller gives the
cell back through `persistent_buf_release` of the same cellblock [TAKES OWNERSHIP]. A request that the
cellblock cannot satisfy returns NULL (`src/locus_carcerum/locus_carcerum.h:1273-1274`).

A program can also give a cellblock movable cells, once, through its guard's `movable_init`.
`movable_buf_alloc` then answers a const pointer to the cell's entry in a translation table, and
`movable_buf_read`, `movable_buf_write`, `movable_buf_compare` and `movable_buf_measure` reach the cell
through that entry. A mover on a thread that the program gives sorts the movable cells when the
fragmentation passes `MMGR_MOVABLE_SETTLE_PERCENT`. [The region guide](docs/modules/confinium.md) gives
the rules. A span is a view
with limits over storage that the caller owns [BORROWS]. A ring moves bytes from one producer to one
consumer.

```c
#include "mmgr.h"

/* Two pools. Each is a bare block of bytes until something dresses it. */
ParsMemoriaeInternae(work, 2048);
ParsMemoriaeInternae(keys, 2048);

/* Dressing them is the claim, and it happens here, never at run time. Releasing a cell in work
   leaves its bytes as they are. Releasing one in keys zeroes them. Nothing after this line can
   change either, and a cellblock is reached by the name of the pool it was built over. */
LocusCarcerum(prison, MMGR_MINIMUM_SECURITY(work), MMGR_MAXIMUM_SECURITY(keys));

/* Every cell is counted to an owner. The program's owners are declared once, as data: the table of their
   records and each owner's const pointer into it, which the linker resolves and the caller passes on every
   take and release. */
#define FRAMING_OWNERS(owner_) owner_(framer, "framer")
MMGR_OWNERS(FRAMING_OWNERS)

void frame(void)
{
    void *const cell = prison.work.persistent_buf_alloc(256u, framer);
    mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = cell, .cap = 256u);

    EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = &span, .value = 0xDEADBEEFu, .bytes = 4u);
}
```

## Why a take does not land on a cell

A take fails when the gap cannot hold it. The take then adds its bytes to an overrun word, and the
boundary stays where the bump moved it. The next writer that closes the cellblock settles each boundary
by its overrun.

The invariant has two parts. Each boundary is the end of its chain, moved by its overrun. The two chains
do not cross. The proof uses one step: after a bump fails, each later bump of that tier also fails until
the next settle.

That step needs one atomic step for two actions of a bump: the move of its own boundary and the read of
the other boundary. Thus the two boundaries share one word. With two words, a bump can fail while a later
bump above it succeeds. The settle then moves the boundary below a positioned cell.

[LocusCarcerum: the algorithm](docs/concepts/locus-carcerum-algorithm.md) gives the state, the
operations, the invariant, Lemmas 1 to 6 and the theorem. It also gives the order of steps that breaks
the form with two words.

## The proofs

Lean 4 checks the models in [`theory/locus_carcerum`](theory/locus_carcerum). The models use the core
library and embedded_types' theory, which `lakefile.toml` requires from `deps/embedded_types/theory`.
That theory holds the turnstile proofs for the passes and the drain. `Pass.lean`, `Order.lean` and
`Sleep.lean` import its Manifold proofs and build on them.

| file                    | what it proves                                                                                                                                                                                                    |
| ----------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Packed.lean`           | Lemmas 1 to 5 and the theorem, with the two boundaries read in one step                                                                                                                                           |
| `Split.lean`            | the steps with two words, and the order of steps that breaks the invariant                                                                                                                                        |
| `Word.lean`             | the arithmetic of the word, the request guard, and the limit on the bumps in progress                                                                                                                             |
| `Layout.lean`           | the sweep retains the bytes, the busy cells and the count of each owner                                                                                                                                           |
| `Take.lean`             | a take over a free run, and the count that the mover retains                                                                                                                                                      |
| `Move.lean`             | the move ends with the sweep: the tier ends busy, every count stays, a lone cell ends the tier                                                                                                                    |
| `Spill.lean`            | the spill, the bump at the end, and the two growths                                                                                                                                                               |
| `Give.lean`             | a cell goes off the count of its owner one time                                                                                                                                                                   |
| `Temporary.lean`        | the temporary tier is the persistent tier mirrored: its sweep, its last fit and its positioning                                                                                                                   |
| `Stacks.lean`           | one temporary stack that two threads share sinks past every depth, and a stack for each thread does not                                                                                                           |
| `Regions.lean`          | a region of the temporary tier stays within its size over any interleaving, through the fall to the large region, and a refused take moves no cell                                                                |
| `Idle.lean`             | the spans the idle walk passes hold no byte of a cell, and an unplaced cellblock refuses every take                                                                                                               |
| `Boundary.lean`         | a free cell stays at the edge only while its trim bit is set                                                                                                                                                      |
| `Fragment.lean`         | the fragmentation value is 100 or less                                                                                                                                                                            |
| `Futex.lean`            | the address wait that GenMC runs has the behavior of the futex contract and no other                                                                                                                              |
| `Bounds.lean`           | the span test, the pointer test and the high-water marks                                                                                                                                                          |
| `Entry.lean`            | two contexts do not name one movable entry                                                                                                                                                                        |
| `Pass.lean`             | a pass does not work on the cellblock while a writer works alone                                                                                                                                                  |
| `Sleep.lean`            | a pass that sleeps on the closed word wakes when the writer opens                                                                                                                                                 |
| `Trim.lean`             | a cellblock with no trim bit set and no give in progress owes no trim                                                                                                                                             |
| `Order.lean`            | the C11 orders that the closed word, the drain and the free use                                                                                                                                                   |
| `OwnerWord.lean`        | the owner word's fields, and the undo of one owner's takes after a sequence point                                                                                                                                 |
| `Lowest.lean`           | the claim of a movable entry takes the lowest free bit, and names the entry at that bit's place                                                                                                                   |
| `Ticket.lean`           | the owner word, the give and the owner's sequence as the stamp, the claim and the ticket of embedded_types' ring                                                                                                  |
| `Guard.lean`            | a guard passes a token to its record, one CAS commits a call, the `~&` claim test keeps two claims apart, and `vec[n]` decays to a span inside its cell                                                           |
| `Linearizable.lean`     | any count of threads on any schedule, each with a read, a claim CAS, its work and a release CAS, is linearizable: conditions L1 and L2 of Herlihy and Wing, Section 2.2                                           |
| `ConcurrentVector.lean` | `at`, `set`, `push_back` and `pop_back` on any count of threads, each with a claim bit of its element and one CAS or read of the count, are linearizable                                                          |
| `MPMCQueue.lean`        | Folly's MPMC queue is linearizable against the queue with one lock, with the external linearization point of a dequeue: a port of the Coq proof of Vindum, Frumin and Birkedal, CPP 2022                          |
| `EndToEnd.lean`         | a call runs from the ticket ring through the turnstile lane to the cells on embedded_types' integrated model, a refused job never runs, a drain waits for each call in progress, and no wake is lost              |
| `Hypotheses.lean`       | each hypothesis of the guard, the vector, the claims and the run end to end holds at the widths the C builds for, and a state that the steps reach meets all of them at once                                      |
| `Vector.lean`           | a vector behind the guard does what `std::vector` does within its pool's ceiling and alignment, a compaction changes nothing the caller sees, and the fill closes the holes of the persistent and temporary tiers |

```sh
cd theory/locus_carcerum
lake build
```

`lean-toolchain` sets the Lean version. `elan` gets that version on the first build.

`python test/harness.py prove` holds the C to the same theorems. Each harness in `test/prove` states one
theorem with inputs, assumptions and assertions. CBMC proves each harness on the C source. A symbolic
executor proves each harness on the machine code that GCC and Clang make at `-O0` and `-O2`. Each prover
must also reach the end of each harness, and a harness with no possible path cannot pass.

GenMC holds the threads. Each client in `test/prove` named `*_genmc.c` runs two or three threads on the C. GenMC
runs each client under SC, TSO, RC11, IMM and ARMv8, and it explores each execution that the model allows. A
client fails on a broken assertion, a data race, or a thread that sleeps with no wake to come. The address wait
that the clients use is the model in `Futex.lean`.

## Where things are

|                                               |                                                                                                         |
| --------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| [`src/`](src)                                 | the library, one directory per module                                                                   |
| [`include/mmgr.h`](include/mmgr.h)            | the header a consumer includes: pool declarations, the name guards, the assert hook                     |
| [`deps/embedded_types/`](deps/embedded_types) | the widths, the word, the attribute wrappers and the dispatch macros every module builds on             |
| [`test/`](test)                               | `unit` per translation unit, `integration` and `interop` across modules, `environment` per width        |
| [`test/stress/`](test/stress)                 | threads on one cellblock, every cell's bytes checked before it goes back                                |
| [`test/fuzz/`](test/fuzz)                     | a libFuzzer harness for every module, each checked against a model, and the replay every toolchain runs |
| [`test/verify/`](test/verify)                 | every toolchain's build of a module, and the check that no build of `src/` calls the C library          |
| [`test/prove/`](test/prove)                   | the proof harnesses that CBMC and the symbolic executor hold                                            |
| [`test/harness.py`](test/harness.py)          | build, run, fuzz, verify, nolibc, prove, the A/B, coverage, and the suite and generator checks          |
| [`tools/dev_env/`](tools/dev_env)             | the generators for `src/`'s tables, the size sweep, the source rewriters                                |
| [`theory/`](theory)                           | the Lean proofs of LocusCarcerum and of the calls with plain arguments                                  |
| [`docs/`](docs)                               | the prose, and the Doxygen configuration that renders it                                                |

### The modules

Each module has a Latin name. The entries and the guards that a caller uses have English names.

| module                                                                                                      | what it is                                                 |
| ----------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| [`locus_carcerum`](src/locus_carcerum)                                                                      | the two-ended allocator that everything else sits on       |
| [`memoria_anularis`](src/memoria_anularis)                                                                  | the lock-free ring between a producer and a consumer       |
| [`memoria_vector`](src/memoria_vector)                                                                      | a vector behind a token, its elements in one movable cell  |
| [`spatium`](src/spatium)                                                                                    | bounded views over caller memory; owns nothing             |
| [`cellularum_laboro`](src/cellularum_laboro)                                                                | bounded string work: search, compare, parse                |
| [`verbum_scrutor`](src/verbum_scrutor)                                                                      | the SWAR core the scans are built from                     |
| [`clz`](src/clz)                                                                                            | leading and trailing zero counts, branchless               |
| [`memoria_operor`](src/memoria_operor)                                                                      | copy, move, compare, set, find                             |
| [`proximus_operor`](src/proximus_operor)                                                                    | word-width loads and stores at any alignment               |
| [`verba_scribo`](src/verba_scribo)                                                                          | text out: integers, hex, floats, JSON, XML                 |
| [`numeros_scribo`](src/numeros_scribo)                                                                      | records built from a field spec                            |
| [`byteio`](src/octetus_introitus_exitus) / [`bitio`](src/bitorum_introitus_exitus) / [`endian`](src/endian) | the wire                                                   |
| [`fractio`](src/fractio)                                                                                    | takes a double apart, without `<math.h>`                   |
| [`transformo`](src/transformo)                                                                              | decimal to binary, into a double or an integer             |
| [`pow5`](src/pow5)                                                                                          | generated: the powers of five the decimal conversion needs |
| [`ascii_persona_bitorum`](src/ascii_persona_bitorum) / [`impensa_ancorae_acus`](src/impensa_ancorae_acus)   | generated: character classes and byte rarity               |
| [`memoria_externa`](src/memoria_externa)                                                                    | internal or external placement; `MMGR_ENABLE_EXTRAM`       |
| [`memoriam_praetereo`](src/memoriam_praetereo)                                                              | DMA channels over weak port hooks; `MMGR_ENABLE_DMA`       |
| [`string_shim`](src/config)                                                                                 | `memset`, `memcpy`, `memmove` and `memcmp`; opt in         |

`verba_scribo/verba_linea.h` gives calls with plain arguments and no dispatch table. It is a line that carries its
own cursor. It includes `mmgr.h` alone, and so a C++ translation unit calls them as they are.

### The vector

`MemoriaVector(name, element, ceiling, owner)` declares a vector as data. The token `name` is a const
pointer to its record, and the caller holds the token and never an address in the cell. The elements
are in one movable cell, which a growth replaces up to the ceiling. Each call goes through the movable
entries of a guard, which `MMGR_VECTOR_THROUGH(guard)` names in the call's `MemoriaVectorCfg`.

```c
MemoriaVector(samples, uint32_t, 64u, framer);

void keep(uint32_t sample)
{
    (void)EMBED_CALL(memoria_vector.push_back, MemoriaVectorCfg, MMGR_VECTOR_THROUGH(prison.work),
                     .vector = samples, .src = &sample);
}
```

`at`, `set`, `push_back`, `pop_back`, `size` and `release` are the calls, and each can run on many
threads at once. `at` and `set` claim the bit of their element. A push and a pop take effect at one CAS
of the count. `ConcurrentVector.lean` proves the four calls linearizable. The cellblock needs its
movable cells, through `movable_init`, before the first push.

### A program with no C library

No build of `src/` calls the C library. `python test/harness.py nolibc` holds that for four toolchains
and each optimization level, LTO included. It reads the IR, the objects and an image that links with
no default library. [No C library in src/](docs/quality/no-libc.md) gives the stages.

A compiler writes calls to `memset`, `memcpy`, `memmove` and `memcmp` in a program's own code. The
target `mmgr_string_shim_<env>` defines the four names over `memor`, for a program with no C library.
`mmgr_<env>` does not link it, and a program that wants the names links the target itself.

Three build rules keep the C library out of `src/`. Each module with no level of its own builds at
`-O2`. The frontend of cl, in cl and in clang-cl, builds with `/GS-`. The string shim builds with no
LTO. `verba_scribo`, `memoria_anularis` and `memoriam_praetereo` link `memoria_operor`, since their
fills go through `memor.set`.

## How to build it

The first configure uses the network. It gets `embedded_types` into `deps/` at its `main` branch
(`deps/CMakeLists.txt:13-18`). Give a generator and a compiler. GCC and Clang build all of the tree. MSVC
builds all of the tree with `/experimental:c11atomics`, because the atomics of embedded_types take
their C11 arm there.

To build without the network, put `embedded_types` at `deps/embedded_types` and add
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON`. The build then uses the files on the disk. Use this only on a
machine that already has the dependency. With an empty `deps/`, this mode has nothing to use.

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DMMGR_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

A build with no command line flags sets the tunables in a header, `mmgr_settings.h`, on its include
path. The Arduino IDE builds a library that way. `mmgr.h` and `config/mmgr_config.h` read the header
before the first tunable, and this repository holds no such header.

The harness gives each build tree the flags that it needs:

```sh
python test/harness.py test        # build and run the suites
python test/harness.py fuzz        # build the fuzz harnesses and run each one
python test/harness.py verify      # each toolchain's build of each module, compared
python test/harness.py nolibc      # each toolchain's build of src/, held to no symbol of a C library
python test/harness.py prove       # each proof harness, on the C and on the machine code
python test/harness.py ab          # this library, then the same suites against libc
python test/harness.py coverage    # the parts of src/ that the suites reached
```

`nolibc --report build/nolibc-<stamp>` prints the summary of an earlier run again. `coverage --arms
file:line` prints each arm of the branches on a line, with its count in each translation unit.

Two environment variables are necessary on a first build, because no earlier tree gives their values.
`MMGR_CMAKE_ARGS` holds the generator and the compiler. `MMGR_BUILD_ROOT` moves the build trees.

Windows needs `MMGR_BUILD_ROOT`, because Windows limits a full object path to 250 characters. The deepest
object here is approximately 180 characters below its build directory. Thus a checkout more than
approximately 60 characters down cannot build in place (`test/harness.py:61-62`).

## Stress and fuzz

`ctest` runs the stress program for a short time. For a long run, give the program these values in this
order: threads, rounds, cellblock bytes, runs and seed.

```sh
build/test/host/locus_carcerum_stress_host 16 200000 1048576 1 777
```

Each thread takes and gives back cells on an owner of its own. Each thread fills each cell with its mark,
and reads the mark before it gives the cell back. After all the threads stop, the cellblock must be idle.
A run that stops in a different state returns a value that is not zero.

Each module has a libFuzzer harness, and a model in the harness examines each result. The harnesses need
Clang and `-DMMGR_FUZZ=ON`. `python test/harness.py fuzz` builds them and runs each one in the suite.

```sh
python test/harness.py fuzz --seconds 60 --filter locus_carcerum
```

## How each change is verified

The checks run in this order, from the commit on a workstation to the jobs on GitHub. A check that fails stops
the change.

1. **The commit.** The pre-commit hook in `.githooks` runs `python test/harness.py ste`. This check compares
   each technical document with ASD-STE100. A document can have no more findings than `test/ste_ratchet.tsv`
   permits, and a new document can have none. To use the hook, run `git config core.hooksPath .githooks` one
   time in each clone.
2. **The suite.** `python test/harness.py test` builds the tree, and `ctest` runs each suite. These are the
   unit, integration, interop and environment suites, the stress programs, the trim race test and the
   ASD-STE100 check.
3. **The fuzz harnesses.** `python test/harness.py fuzz` builds each libFuzzer harness with Clang, with the
   address and undefined behavior sanitizers. A model in each harness examines each result.
4. **The Lean proofs.** `lake build` in `theory/locus_carcerum` checks each model. A theorem that `sorry`
   closes has no proof. Lean does not stop on it, but the CI job fails.
5. **The proof harnesses.** `python test/harness.py prove` runs three provers. CBMC proves each harness on the
   C source. A symbolic executor proves each harness on the machine code that GCC and Clang make at `-O0` and
   `-O2`. GenMC runs each thread client under each of the five memory models.

On GitHub, each push to `main` and each pull request starts these workflows:

| workflow           | what it does                                                                                               | what makes it fail                                                 |
| ------------------ | ---------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| `SonarCloud.yml`   | builds the default tree with coverage, runs `ctest`, and sends the analysis and the coverage to SonarCloud | a build error or a test that fails                                 |
| `capabilities.yml` | runs the suite on `build-dma`, `build-dma-lean` and `build-extram`, the trees with DMA or external RAM     | a build error or a test that fails                                 |
| `proofs.yml`       | runs the fuzz harnesses, the Lean proofs, CBMC and the machine arm, and GenMC                              | a fuzz error, a Lean error, a `sorry`, or a harness with no proof  |
| `codeql.yml`       | analyzes the C and the workflows with the `security-and-quality` queries                                   | the code scanning check that the branch ruleset of `main` requires |
| `format-code.yml`  | checks the C with clang-format, the Python in `tools` with black, and the markup and data with prettier    | a file that the formatter changes                                  |
| `docs.yml`         | makes the Doxygen pages, counts the warnings, and puts the pages on GitHub Pages from `main`               | Doxygen does not run                                               |

A hosted runner has 16 GB of memory. `prove --hosted` does not run a harness whose bound needs more memory, and
it writes the name of each harness that it does not run. The `locus_carcerum_movable` harness needs more than
27 GB under CBMC. Thus, this harness runs only on a workstation with `python test/harness.py prove`.

## What MMgr claims, and where the proof is

- **Nothing allocates.** [Zero heap](docs/concepts/zero-heap.md)
- **Each width is a compile-time knob, and the tree builds all of them.** [Width and order](docs/concepts/width-and-order.md)
- **The scans are SWAR, in plain C, with no intrinsics.** [SWAR](docs/concepts/swar.md)
- **The namespace idiom removes the indirect calls.** [The ns idiom](docs/concepts/ns-idiom.md)
- **A take is one fetch-add on the word that the two boundaries share, and no take lands on a positioned cell.** [LocusCarcerum: the algorithm](docs/concepts/locus-carcerum-algorithm.md), proved in [Lean](theory/locus_carcerum)
- **`to_double`, `verba.g` and `verba.fixed` are exact. `to_float` rounds two times and can give the wrong neighbor.** [Numbers, and their accuracy](docs/quality/numeric-accuracy.md)
- **The suites cover each compile-time width, and they pass against this library and against libc.** [The test suite](docs/quality/testing.md)
- **The cost of each optimization level.** [Optimization](docs/quality/optimization.md)
- **No build of `src/` calls the C library, under gcc, clang, clang-cl and msvc, LTO included.** [No C library in src/](docs/quality/no-libc.md)

## Status

The version is 2.3.0 and the API is stable. The tools in this repository make the numbers
in [`docs/quality/`](docs/quality), and you can run them again.

## Licensing - This library is dual licensed.

Licensed AGPL-3.0-or-later. // various commercial contracts available
It will always be free to use under the AGPL.
Educators: If you would like an exception to use this in your classrooms or research projects,
please email dstroy0 (Douglas Quigg) <dquigg123@gmail.com> from your _.edu or _.org
faculty email address, I would be happy to grant you an exception on a case-by-case basis. Your exception
governs your use, specifically the accreditation requirement of underlying systems in any research/presentation materials.
Academic exemptions can lead to viable market products, in which case this license shifts to a royalty ladder,
based arbitrarily off of the amount of goodwill you've displayed and how well you've adhered to crediting students and
other faculty involved in the project, a portion of the royalties go directly to your institution at a minimum and
straight to your department if their rules allow for it.
See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).
