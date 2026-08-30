# MMgr

Zero-heap memory manager in C11. `LocusCarcerum` declares a prison site. Each cellblock gets its own
storage and its state, the warden is the const struct emitted under the site's name that holds those
cellblocks as its members and that every call goes through, all of it is initialized data, and
nothing runs at startup (`src/locus_carcerum/locus_carcerum.h:458-463`). The footprint is a number
you can check against a budget.

A cellblock hands out from both ends. The persistent tier runs up from base. The temporary tier runs
down from size (`src/locus_carcerum/locus_carcerum.h:16-17`). The gap between them is what either
can still take. A cell from the persistent tier is the caller's
[RETURNS OWNERSHIP] until it goes back through that same cellblock's `persistent_buf_release`
[TAKES OWNERSHIP], and a request the cellblock cannot meet returns NULL
(`src/locus_carcerum/locus_carcerum.h:484-486`). Spans bound a view over storage the caller keeps
[BORROWS], and rings carry bytes between one producer and one consumer.

```c
#include "mmgr.h"

/* Two cellblocks, one per security level. Releasing a cell in work leaves its bytes as they
   are. Releasing one in keys zeroes them. Nothing after this declaration can change either. */
LocusCarcerum(prison, MMGR_MINIMUM_SECURITY(work, 2048), MMGR_MAXIMUM_SECURITY(keys, 2048));

void frame(void)
{
    void *const cell = prison.work.persistent_buf_alloc(256u);
    mmgr_span span = MMGR_CALL(spat.from, SpatiumCfg, .buf = cell, .cap = 256u);

    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &span, .value = 0xDEADBEEFu, .bytes = 4u);
}
```

## Where things are

|                                                        |                                                                                                  |
| ------------------------------------------------------ | ------------------------------------------------------------------------------------------------ |
| [`src/`](src)                                          | the library, one directory per module, plus `config`                                             |
| [`src/mmgr.h`](src/mmgr.h)                             | the single header a consumer includes                                                            |
| [`src/config/mmgr_config.h`](src/config/mmgr_config.h) | widths, cellblock size bounds, feature switches, the assert hook, the entry macros               |
| [`test/`](test)                                        | `unit` per translation unit, `integration` and `interop` across modules, `environment` per width |
| [`test/harness.py`](test/harness.py)                   | build, run, the A/B, coverage, and the suite and generator checks                                |
| [`tools/dev_env/`](tools/dev_env)                      | the generators for `src/`'s tables, the size sweep, the source rewriters                         |
| [`docs/`](docs)                                        | the prose, and the Doxygen configuration that renders it                                         |

### The modules

Latin names, because they were carved out of a larger tree and the short English words were taken.

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

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build
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
deepest object here sits about 180 below its build directory, so a checkout more than about 60
characters down cannot build in place (`test/harness.py:27-34`).

## What it claims, and where that is answered

- **Nothing allocates.** [Zero heap](docs/concepts/zero-heap.md)
- **Every width is a compile-time knob, and all of them are built.** [Width and order](docs/concepts/width-and-order.md)
- **The scans are SWAR, in plain C, with no intrinsics.** [SWAR](docs/concepts/swar.md)
- **The namespace idiom devirtualizes.** [The ns idiom](docs/concepts/ns-idiom.md)
- **`to_double`, `verba.g` and `verba.fixed` are exact. `to_float` rounds twice and can land on the wrong neighbor.** [Numbers, and how far they can be trusted](docs/quality/numeric-accuracy.md)
- **160 targets, 100% of `src/` lines and branches, green against libc as well.** [The test suite](docs/quality/testing.md)
- **What each optimization level costs.** [Optimization](docs/quality/optimization.md)

## Status

0.1.0, pre-1.0, so the API may change. What is measured is measured — the numbers in
[`docs/quality/`](docs/quality) come out of tools in this repository and can be rerun.

## Licensing - This library is dual licensed.

Licensed AGPL-3.0-or-later. // various commercial contracts available
It will always be free to use under the AGPL.
Educators: If you would like an exception to use this in your classrooms or research projects,
please feel free to email dstroy0 (Douglas Quigg) <dquigg123@gmail.com> from your _.edu or _.org
faculty email address, I would be happy to grant you an exception on a case-by-case basis. Your exception
governs your use, specifically the accreditation requirement of underlying systems in any research/presentation materials.
Academic exemptions can lead to viable market products, in which case this license shifts to a royalty ladder,
based arbitrarily off of the amount of goodwill you've shown and how well you've adhered to crediting students and
other faculty involved in the project, a portion of the royalties go directly to your institution at a minimum and
straight to your department if their rules allow for it.
See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).
