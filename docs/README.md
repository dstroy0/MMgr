# MMgr {#mainpage}

Zero-heap memory manager in C11.

You lend it a buffer. A confinium carves that buffer from both ends: persist grows up from the base,
interim grows down from the top, and a take fails rather than let the two cross. Pools hand out
tenants cut from it, spans are bounded views over those, and rings move them between workers.

Nothing calls `malloc`. Every size is fixed before the program runs, so the footprint is a number you
can check against a budget rather than a thing you find out at runtime.

```c
#include "mmgr.h"

static uint8_t region[4096];

mmgr_confin c;
mmgr_confin_init(&c, region, sizeof region);

uint8_t *p = mmgr_confin_persist_capio(&c, 256, 8);   /* grows up from the base */
mmgr_spat  s = spat.from(p, 256);                     /* a view; owns nothing   */

verba.put(&b, "id=");
verba.u32(&b, 4211);
```

## What it claims

Each of these is a claim the documentation has to answer for, so each links to the page that does.

- **Nothing allocates.** Storage is borrowed, never owned. @ref concept_zero_heap
- **One region, carved by asserted offset.** Persist grows up, interim grows down, and a take fails
  rather than let them cross. @ref concept_architecture
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

The modules carry Latin category names: `confinium` is the arena-like region, `spatium` is a span,
`verbum_scrutor` is the SWAR scanner. The names were chosen so that no module collides with libc or
with a consumer's own vocabulary. Every one of them is decoded in @ref ref_glossary.

The call-site idiom follows from that. Each module exposes a dispatch table named for a short stem,
so a call reads `spat.from(buf, cap)` rather than `mmgr_spatium_span_from(buf, cap)`. Both spellings
exist and both are documented; the free function is what the table points at. @ref concept_ns_idiom
