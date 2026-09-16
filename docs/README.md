# MMgr {#mainpage}

Zero-heap memory manager in C11.

You declare a pool: a block of bytes, whose placement is settled by which declaration you write.
`LocusCarcerum` dresses a pool as a cellblock, and a cellblock hands out cells from both ends. The
persistent tier grows up from base, the temporary tier grows down from size, and the gap between them
is what either can still take. An allocation the gap cannot meet returns NULL and moves no boundary.
Spans are bounded views over cells, and rings move bytes between one producer and one consumer.

Nothing calls `malloc`. Every size is fixed before the program runs, so the footprint is a number you
can check against a budget rather than a thing you find out at runtime.

```c
#include "mmgr.h"

/* A pool of bytes, and the line that dresses it. Together they are the whole setup. */
ParsMemoriaeInternae(work, 4096);
LocusCarcerum(prison, MMGR_MINIMUM_SECURITY(work));

char *const buf = prison.work.persistent_buf_alloc(256u);

size_t at = 0;
at = EMBED_CALL(verba_textus.put, VerbaTextusCfg, .out = buf, .cap = 256u, .at = at, .text = "id=");
at = EMBED_CALL(verba_numerus.u64, VerbaNumerusCfg, .out = buf, .cap = 256u, .at = at, .val = 4211u);

const size_t len = EMBED_CALL(verba_finis.finish, VerbaFinisCfg, .out = buf, .cap = 256u, .at = at);
```

## What it claims

Each of these is a claim the documentation has to answer for, so each links to the page that does.

- **Nothing allocates.** Storage is borrowed, never owned. @ref concept_zero_heap
- **One region, carved by asserted offset.** Persist grows up and interim grows down; what keeps them
  apart is asserted when the region is defined, not tested on each take. @ref concept_architecture
- **Scanning is SWAR, and the carrier is the machine word.** Measured, not asserted.
  @ref concept_swar
- **The width is a compile-time knob, and every width is built.** Five environments, one build.
  @ref ref_environments
- **Host-buildable and host-testable.** The only headers it reaches for are `stddef`, `stdint` and
  `stdatomic`. @ref qa_testing

## Where to go next

| If you want to                     | Read               |
| ---------------------------------- | ------------------ |
| Build it and run something         | @ref nav_start     |
| Understand how the pieces fit      | @ref nav_concepts  |
| Find the module you need           | @ref nav_modules   |
| Look up a knob, a name or a number | @ref nav_reference |
| Know what is tested and analyzed   | @ref nav_quality   |
| Contribute, or read the license    | @ref nav_project   |

## A note on the names

The modules carry Latin category names: `locus_carcerum` is the region and its pools, `spatium` is a
span, `verbum_scrutor` is the SWAR scanner. Every one of them is decoded in @ref ref_glossary.

The reason is that libc and `<string.h>` are everywhere, and these are not libc functions. They
reach the same conclusion for the same input, across every domain — that is the whole of what they
share. How they reach it is entirely different.

The backend is machine-width parallel word processing throughout. Bitmath is extensive and SIMD
within a register is used wherever it applies, which is what buys speed far above the traditional
embedded string and memory routines in far less space. A name that collided with libc's would
suggest a drop-in replacement of the implementation as well as the result, and it is not one.

That difference is also why the testing is what it is. Every translation unit is fuzzed with bit
strobing, bit waves, clock stretching and other abuse the internals will never meet in service. On
top of that: interoperability against libc, newlib and GNU; correctness; binary size; and
resource allocation lifetime cycles.

The call-site idiom follows from that. Each module exposes a dispatch table named for a short stem,
and every entry takes one argument: a pointer to that module's config struct. `EMBED_CALL` builds it
as a compound literal, so a call reads
`EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = n)` with the arguments named instead of
ordered. Both forms exist and both are documented; the free function is what the table points at.
@ref concept_ns_idiom
