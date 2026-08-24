# Carceribus — the region {#mod_confin_guide}

One region, carved from both ends. Persist grows up from the base, interim grows down from the top,
and nothing in between is ever freed.

## When to reach for it

- You have a buffer and you need to hand pieces of it out.
- Some of those pieces live as long as the program and some live for one operation.
- You want the failure to be "this region is full", not "the system is out of memory".

## What it composes with

Everything sits on top of it. @ref mod_clarus_guide and @ref mod_occult_guide are pools over a
region; @ref mod_spat_guide views what a region hands out; @ref mod_infin_guide moves those views
between a producer and a consumer.

## Declaring one

`mmgr_carcer_init` is a macro, and nothing it does happens at runtime. It emits the storage, the
layout type, an enumerator per pool and the descriptors, all at file scope, and every size claim in
it is a static assert:

```c
mmgr_carcer_init(g_ram, 4096u, MMGR_POOL(g_scratch, 2048u), MMGR_POOL(g_work, 2048u));
```

A pool that is not a whole number of `MMGR_ALIGN_BYTES`, or one that runs past the end of the
region, does not build. There is no runtime check because there is nothing left to check.

## Worked example

```c
CarcerCtx *const pool = MMGR_CARCER_POOL(g_ram, g_scratch);

char *const table = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = pool, .size = 512u);

const size_t mark = MMGR_CALL(carcer.interim_mark, CarcerCfg, .pool = pool);
char *const work  = MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = pool, .size = 256u);

/* ... use work ... */

MMGR_CALL(carcer.interim_reddo, CarcerCfg, .pool = pool, .size = mark);
```

`capio` is _take_, `reddo` is _give back_. @ref ref_glossary has the rest of the verbs.

## Gotchas

**Nothing checks a size against the room left.** The two cursors move by whatever you hand them, and
a take that would cross them crosses them. `octas_praesto` reports what is between them, and asking
it is the check. This is the same rule the whole library runs on: bounding happens before the call,
at compile time wherever it can.

**A take never returns NULL.** It returns a pointer either way, so testing the return tells you
nothing. Compare your size against `octas_praesto` first.

**A pointer that outlives its mark is dead and still readable.** Nothing is scrubbed and nothing
moves, so it dereferences fine and returns whatever the next take put there. Keep a mark and its
`reddo` in the same function.

**`persist_reddo` only unwinds.** It moves the cursor back by the size you give it. It is not a
general free, there is no free list, and giving back more than is outstanding wraps the cursor.

**`carcer.owns` is for asserts.** It says a pointer is inside the pool's storage. It does not say
the pointer is live.

## Sizing it

```c
const size_t out  = MMGR_CALL(carcer.persist_used,   CarcerCfg, .pool = pool);
const size_t left = MMGR_CALL(carcer.octas_praesto,  CarcerCfg, .pool = pool);
const size_t peak = pool->hw;   /* MMGR_ENABLE_HW_MEM_CAPACITY_CB only */
```

`persist_used` and `octas_praesto` are current values, not peaks. The peak is `hw`, and it is only
maintained when `MMGR_ENABLE_HW_MEM_CAPACITY_CB` is on — off by default, so a run without it leaves
`hw` at zero and a reading from that run means nothing. @ref guide_first_region has the procedure.

## Reference

@ref mod_confin "Generated reference for carceribus"
