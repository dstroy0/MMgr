# Confinium — the region {#mod_confin_guide}

One tenant. Two ends, growing toward each other, and nothing in between is ever freed.

## When to reach for it

- You have a buffer and you need to hand pieces of it out.
- Some of those pieces live as long as the program and some live for one operation.
- You want the failure to be "this region is full", not "the system is out of memory".

## What it composes with

Everything sits on top of it. @ref mod_clarus_guide and @ref mod_occult_guide are pools built over
confinia; @ref mod_spat_guide views what a confinium hands out; @ref mod_infin_guide moves those
views between a producer and a consumer.

It is the one module with **no dispatch table**. Its surface is a set of verbs on an explicit
`mmgr_confin *`, so a table would add indirection without shortening a call site.

## Worked example

```c
static uint8_t region[4096];

mmgr_confin c;
mmgr_confin_init(&c, region, sizeof region);

/* Lives as long as the region. Grows up from the base. */
uint8_t *table = mmgr_confin_persist_capio(&c, 512, 8);

/* Interim, for one operation. Grows down from the top. */
const size_t m = mmgr_confin_interim_mark(&c);
uint8_t *work  = mmgr_confin_interim_capio(&c, 256, 8);
if (work == NULL) {
    return -1;                              /* nothing was taken; nothing to undo */
}
/* ... use work ... */
mmgr_confin_interim_reddo(&c, m);           /* one line, however many takes happened */
```

`capio` is _take_, `reddo` is _give back_. @ref ref_glossary has the rest of the verbs.

## Gotchas

**A pointer that outlives its mark is dead and still readable.** Nothing is scrubbed and nothing
moves, so it dereferences fine and returns whatever the next take put there. Keep a mark and its
`reddo` in the same function.

**`octas_praesto` is not a release.** It reports the bytes still between the two ends. It is what you
log when a take returns `NULL`.

**`persist_reddo` only unwinds the most recent take.** It is not a general free, and there is no
general free.

**`mmgr_confin_init` believes the length you give it.** There is no way to check, and a length longer
than the real buffer makes every subsequent bounds check meaningless. See @ref proj_security.

## Sizing it

Read `mmgr_confin_persist_used` and `mmgr_confin_interim_used` after a real workload under the
`checks` environment. They are high-water marks, not current values. @ref guide_first_region has the
procedure.

## Reference

@ref mod_confin "Generated reference for confinium"
