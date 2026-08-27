# MMgr

Zero-heap memory manager in C11. Nothing calls `malloc`. Every size is fixed before the program
runs, so the footprint is a number you can check against a budget rather than something you find
out at runtime.

You lend it a buffer. A **confinium** carves that buffer from both ends — persistent allocations
grow up from the base, interim ones grow down from the top, and a request fails rather than let the
two cross. Pools hand out tenants cut from it, spans are bounded views over those, and rings move
them between a producer and a consumer.

```c
#include "mmgr.h"

/* The region and its pools are carved at compile time; this emits the storage. */
mmgr_carcer_init(g_ram, 4096u, MMGR_POOL(g_pool, 4096u));

CarcerCtx *const pool = MMGR_CARCER_POOL(g_ram, g_pool);

uint8_t *const p = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = pool, .size = 256u);
mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = p, .cap = 256u);

MMGR_CALL(byteio.put_be, OctetusCfg, .w = &s, .val = 0xDEADBEEFu, .bytes = 4u);
```

## Where things are

|                                          |                                                                         |
| ---------------------------------------- | ----------------------------------------------------------------------- |
| [`src/`](src)                            | the library, one directory per module                                   |
| [`src/mmgr.h`](src/mmgr.h)               | the single header a consumer includes                                   |
| [`src/config/mmgr_config.h`](src/config/mmgr_config.h) | every size and width, all compile-time                                  |
| [`test/`](test)                          | `test/unit` mirrors `src/`, `test/integration` is the cross-module work |
| [`test/harness.py`](test/harness.py)     | build, run, the A/B, coverage, suite discovery                          |
| [`tools/dev_env/`](tools/dev_env)        | generators for the tables in `src/`, and the size sweep                 |
| [`docs/`](docs)                          | the prose; the same content Doxygen renders                             |

### The modules

Latin names, because they were carved out of a larger tree and the short English words were taken.

| module                                                                 | what it is                                                 |
| ---------------------------------------------------------------------- | ---------------------------------------------------------- |
| [`carceribus`](src/carceribus)                                         | the two-ended allocator that everything else sits on       |
| [`confinium_exclusivum_infinitas`](src/confinium_exclusivum_infinitas) | the lock-free ring between a producer and a consumer       |
| [`spatium`](src/spatium)                                               | bounded views over caller memory; owns nothing             |
| [`cellularum_laboro`](src/cellularum_laboro)                           | bounded string work: search, compare, parse                |
| [`verbum_scrutor`](src/verbum_scrutor)                                 | the SWAR core the scans are built from                     |
| [`memoria_operor`](src/memoria_operor)                                 | copy, move, compare, set, find                             |
| [`proximus_operor`](src/proximus_operor)                               | word-width loads and stores at any alignment               |
| [`verba_scribo`](src/verba_scribo)                                     | text out: integers, hex, floats, JSON, XML                 |
| [`numeros_scribo`](src/numeros_scribo)                                 | records built from a field spec                            |
| [`byteio`](src/octetus_introitus_exitus) / [`bitio`](src/bitorum_introitus_exitus) / [`endian`](src/endian) | the wire                                                   |
| [`fractio`](src/fractio)                                               | takes a double apart, without `<math.h>`                   |
| [`pow5`](src/pow5)                                                     | generated: the powers of five the decimal conversion needs |
| [`ascii_persona_bitorum`](src/ascii_persona_bitorum) / [`impensa_ancorae_acus`](src/impensa_ancorae_acus)      | generated: character classes and byte rarity               |

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

## What it claims, and where that is answered

- **Nothing allocates.** [Zero heap](docs/concepts/zero-heap.md)
- **Every width is a compile-time knob, and all of them are built.** [Width and order](docs/concepts/width-and-order.md)
- **The scans are SWAR, in plain C, with no intrinsics.** [SWAR](docs/concepts/swar.md)
- **The namespace idiom devirtualises.** [The ns idiom](docs/concepts/ns-idiom.md)
- **`to_double` is correctly rounded; `verba.g` is not.** [Numbers, and how far they can be trusted](docs/quality/numeric-accuracy.md)
- **150 targets, 100% of lines and branches, green against libc as well.** [The test suite](docs/quality/testing.md)
- **What each optimization level costs.** [Optimization](docs/quality/optimization.md)

## Status

0.1.0, pre-1.0, so the API may change. What is measured is measured — the numbers in
[`docs/quality/`](docs/quality) come out of tools in this repository and can be rerun.

Licensed AGPL-3.0-or-later. See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).
