# MMgr

Zero-heap memory manager in C11. A pool is a block of bytes you declare, `ParsMemoriaeInternae`, or
`ParsMemoriaeExternum` where the part has external RAM and `MMGR_ENABLE_EXTRAM` is set. Which memory
a pool sits in is settled where it is declared, and nothing afterwards inspects an address to find
out.

`LocusCarcerum` dresses pools as a prison site. Each cellblock is bound to the pool it was declared
over, the warden is the const struct emitted under the site's name that holds those cellblocks as its
members and that every call goes through, all of it is initialized data, and nothing runs at startup.
The footprint is a number you can check against a budget.

A pool name stands for one region. Declaring it twice in a translation unit fails to compile, in two
units fails to link, and dressing one pool twice fails to compile, each with a diagnostic naming
what went wrong.

A cellblock hands out from both ends. The persistent tier runs up from base. The temporary tier runs
down from size. The gap between them is what either can still take. Both boundaries are the two
halves of one atomic word, and a take moves its own half by one fetch-add whose old value carries the
other (`src/locus_carcerum/locus_carcerum.h:20-22`). A cell from the persistent tier is the caller's
[RETURNS OWNERSHIP] until it goes back through that same cellblock's `persistent_buf_release`
[TAKES OWNERSHIP], and a request the cellblock cannot meet returns NULL
(`src/locus_carcerum/locus_carcerum.h:1066-1067`). Spans bound a view over storage the caller owns
[BORROWS], and rings carry bytes between one producer and one consumer.

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

## Why a take never lands on a cell

A take that the gap cannot meet fails, and its bytes are added to an overrun word while the
boundary stays where the bump moved it. The writer that next closes the cellblock settles each boundary by its
overrun. The invariant is that each boundary is its chain's end displaced by its overrun, and that the
two chains never cross. The proof rests on one step: once a bump fails, every later bump of that
tier fails too until the next settle. That step needs a bump's move of its own boundary and its read of
the other to be one atomic step, and so the two boundaries share one word. With two words a
bump can fail while a later one above it is taken, and the settle then pulls the boundary below
a laid cell.

[LocusCarcerum: the algorithm](docs/concepts/locus-carcerum-algorithm.md) states the state, the
operations, the invariant, Lemmas 1 to 6 and the theorem, and the order of steps that breaks the
form with two words. The same model is checked in Lean 4 under
[`theory/locus_carcerum`](theory/locus_carcerum), with the core library alone:

| file | what it checks |
| ---- | -------------- |
| `LocusCarcerum/Packed.lean` | Lemmas 1 to 5 and the theorem, with both boundaries read in one step |
| `LocusCarcerum/Split.lean` | the steps with two words, and the order of steps that breaks the invariant, by `decide` |
| `LocusCarcerum/Word.lean` | the word's arithmetic: a bump up never carries into the top's half under the two caps, a bump down's borrow leaves the end's half alone, and the bound on the bumps in flight at a 64-bit and a 32-bit word |

```sh
cd theory/locus_carcerum
lake build
```

`lean-toolchain` pins the Lean version, and `elan` fetches it on the first build.

## Where things are

|                                               |                                                                                                  |
| --------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| [`src/`](src)                                 | the library, one directory per module                                                            |
| [`include/mmgr.h`](include/mmgr.h)            | the header a consumer includes: pool declarations, the name guards, the assert hook              |
| [`deps/embedded_types/`](deps/embedded_types) | the widths, the word, the attribute wrappers and the dispatch macros every module builds on      |
| [`test/`](test)                               | `unit` per translation unit, `integration` and `interop` across modules, `environment` per width |
| [`test/stress/`](test/stress)                 | threads on one cellblock, every cell's bytes checked before it goes back                         |
| [`test/fuzz/`](test/fuzz)                     | the libFuzzer harness over LocusCarcerum's entries, checked against a model of the cells         |
| [`test/harness.py`](test/harness.py)          | build, run, the A/B, coverage, and the suite and generator checks                                |
| [`tools/dev_env/`](tools/dev_env)             | the generators for `src/`'s tables, the size sweep, the source rewriters                         |
| [`theory/`](theory)                           | the Lean proofs of LocusCarcerum's algorithm                                                     |
| [`docs/`](docs)                               | the prose, and the Doxygen configuration that renders it                                         |

### The modules

Each module carries a Latin name. The entries and guards a caller reaches carry plain English ones.

| module                                                                                                      | what it is                                                 |
| ----------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| [`locus_carcerum`](src/locus_carcerum)                                                                      | the two-ended allocator that everything else sits on       |
| [`memoria_anularis`](src/memoria_anularis)                                                                  | the lock-free ring between a producer and a consumer       |
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

## Building it

The first configure reaches the network: it fetches `embedded_types` into `deps/` at its `main`
branch, in `deps/CMakeLists.txt:13-18`. Name a generator and a compiler, because the default on
Windows is MSVC and MSVC does not build the whole tree: `src/memoria_anularis` includes
`<stdatomic.h>`, which MSVC's C enables only under `/experimental:c11atomics`. Every other module
builds there. GCC and Clang build it all.

To build without the network, put `embedded_types` at `deps/embedded_types` yourself and add
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON`. That skips the fetch and uses what is on disk. It is for a
rebuild on a machine that already has the dependency and never for a first clone: disconnected mode
with an empty `deps/` has nothing to use.

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DMMGR_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

Or through the harness, which carries the flags each build tree needs:

```sh
python test/harness.py test        # build and run the suites
python test/harness.py ab          # this library, then the same suites against libc
python test/harness.py coverage    # what of src/ the suites reached
```

Two environment variables matter on a first build, because there is no earlier tree to read them
off. `MMGR_CMAKE_ARGS` carries the generator and the compiler. `MMGR_BUILD_ROOT` moves the build
trees, which Windows makes necessary. A full object path is capped at 250 characters there and the
deepest object here sits about 180 below its build directory. A checkout more than about 60
characters down cannot build in place (`test/harness.py:56-57`).

## Stress and fuzz

`ctest` runs the stress program short. Run long, it takes its thread count, rounds, cellblock bytes,
runs and seed in that order:

```sh
build/test/host/locus_carcerum_stress_host 16 200000 1048576 1 777
```

Each thread takes and releases cells on an owner of its own, fills every cell with its mark and reads
the mark back before the release. The cellblock must read idle once every thread has ended, and a run
that ends otherwise returns nonzero.

The fuzz harness needs clang and `-DMMGR_FUZZ=ON`, and runs under libFuzzer's own arguments:

```sh
cmake -S . -B build-fuzz -G Ninja -DCMAKE_C_COMPILER=clang -DMMGR_BUILD_TESTS=ON -DMMGR_FUZZ=ON
cmake --build build-fuzz --target locus_carcerum_fuzz_host
build-fuzz/test/host/locus_carcerum_fuzz_host corpus/
```

## What it claims, and where that is answered

- **Nothing allocates.** [Zero heap](docs/concepts/zero-heap.md)
- **Every width is a compile-time knob, and all of them are built.** [Width and order](docs/concepts/width-and-order.md)
- **The scans are SWAR, in plain C, with no intrinsics.** [SWAR](docs/concepts/swar.md)
- **The namespace idiom devirtualizes.** [The ns idiom](docs/concepts/ns-idiom.md)
- **A take is one fetch-add on the word both boundaries share, and no take lands on a laid cell.** [LocusCarcerum: the algorithm](docs/concepts/locus-carcerum-algorithm.md), checked in [Lean](theory/locus_carcerum)
- **`to_double`, `verba.g` and `verba.fixed` are exact. `to_float` rounds twice and can land on the wrong neighbor.** [Numbers, and how far they can be trusted](docs/quality/numeric-accuracy.md)
- **Every compile-time width covered, green against this library and against libc.** [The test suite](docs/quality/testing.md)
- **What each optimization level costs.** [Optimization](docs/quality/optimization.md)

## Status

0.1.0, pre-1.0: the API may change. The numbers in
[`docs/quality/`](docs/quality) come out of tools in this repository and can be rerun.

## Licensing - This library is dual licensed.

Licensed AGPL-3.0-or-later. // various commercial contracts available
It will always be free to use under the AGPL.
Educators: If you would like an exception to use this in your classrooms or research projects,
please email dstroy0 (Douglas Quigg) <dquigg123@gmail.com> from your _.edu or _.org
faculty email address, I would be happy to grant you an exception on a case-by-case basis. Your exception
governs your use, specifically the accreditation requirement of underlying systems in any research/presentation materials.
Academic exemptions can lead to viable market products, in which case this license shifts to a royalty ladder,
based arbitrarily off of the amount of goodwill you've shown and how well you've adhered to crediting students and
other faculty involved in the project, a portion of the royalties go directly to your institution at a minimum and
straight to your department if their rules allow for it.
See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).
