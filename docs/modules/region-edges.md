# Confinium exclusivum infinitas — the lock-free edge {#mod_infin_guide}

Single-producer, single-consumer. A byte ring, a segment view over the same bytes, and loculus
keepouts.

## When to reach for it

- One context produces and exactly one consumes.
- An interrupt hands bytes to a task, or a task hands buffers to a driver.
- You want that handover without a lock.

## What it composes with

It moves what @ref mod_confin_guide handed out. The segment view passes whole buffers **by index**,
so nothing is copied across the boundary.

## Three things over the same bytes

**A byte ring.** `available`, `vacant`, `put`, `read`, `read_byte`, `peek`, `consume`. Capacity must
be a power of two — the wrap is a mask, not a modulo.

**A segment view.** `seg_next`, `seg_publish`, `seg_front`, `seg_release`, `seg_at`, `seg_inflight`.
The producer fills a segment and publishes its index; the consumer reads that index and releases it
when done. The bytes never move.

**Loculi.** `loculus_ready`, `loculus_next`, `loculus_hold`, `loculus_keepout`, `loculus_drop`,
`loculus_mark`. A numbered hold that records a region to keep out of, for a reader walking bytes in
place. Their free and held masks are one machine word each, so `MMGR_RING_LOCULI_MAX` is
`MMGR_WORD_BITS`. `MMGR_RING_LOCULI` is the build knob under that ceiling, and a static assert names
it if it is set higher; a build with no use for them sets it to `0` and gets the keepout storage back.

## Worked example

The caller declares the ring and supplies the bytes. Nothing of the state is reachable from outside:

```c
static MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t bytes[4096];
static mmgr_ring ring;

MMGR_CALL(iteratio_infinita.init, InfinCfg, .ring = &ring, .buf = bytes, .cap = 4096u, .nsegs = 8u);
```

Filling and draining:

```c
if (!MMGR_CALL(iteratio_infinita.put, InfinCfg, .ring = &ring, .src = msg, .bytes = n)) {
    /* the whole span did not fit; nothing was written */
}

const size_t got = MMGR_CALL(iteratio_infinita.read, InfinCfg, .ring = &ring, .dst = out, .bytes = n);
```

Passing whole segments by index instead, so nothing is copied across the boundary:

```c
size_t seg = 0;

if (MMGR_CALL(iteratio_infinita.seg_next, InfinCfg, .ring = &ring, .out = &seg)) {
    fill(MMGR_CALL(iteratio_infinita.seg_at, InfinCfg, .ring = &ring, .idx = seg));
    MMGR_CALL(iteratio_infinita.seg_publish, InfinCfg, .ring = &ring);
}

if (MMGR_CALL(iteratio_infinita.seg_front, InfinCfg, .ring = &ring, .out = &seg)) {
    use(MMGR_CALL(iteratio_infinita.seg_at, InfinCfg, .ring = &ring, .idx = seg));
    MMGR_CALL(iteratio_infinita.seg_release, InfinCfg, .ring = &ring);
}
```

`seg_next` and `seg_front` answer whether there is one and write the index through `.out`, so a
caller never needs a sentinel index to mean "none".

## Gotchas

**SPSC only.** Two producers on one ring is a broken program, not a slow one. There is no check and
no assert — exactly one producer advances `head` and exactly one consumer advances `tail`, and that
is the whole reason acquire and release ordering is enough and no entry on those two needs a
read-modify-write.

**`put` is all or nothing.** It checks the whole span against `vacant` first, so a partial write
never happens and a half span is never visible. It answers `MMGR_FALSE` rather than writing what
fits.

**`peek` copies what you ask for whether or not it arrived.** Read `available` first. A count above
the ring's capacity is held there, since one lap is all two runs can express.

**Nothing is ever zeroed.** A loculus keeps its bytes after a drop, so a restream can run again over
the same region. Bytes read are not scrubbed behind the cursor either.

**Capacity must be a power of two.** The index wrap is a mask. `nsegs` likewise, and at most `cap`.

**The buffer is aligned before the ring gets it.** The ring does not check and has no way to.

**`loculus_next` is a SWAR popcount fold, not a builtin.** Same reasoning as the rest of the library —
see @ref concept_swar.

**Self-contained.** It reaches nothing outside `config`: the span mover, the bit index and the span
type it records are all defined in its own `.c`, which is also where `<stdatomic.h>` stays.

## Reference

@ref mod_infin "Generated reference" · @ref concept_ownership for what a pointer's lifetime is

---

# Confinium externum — DRAM or PSRAM {#mod_exter_guide}

Decides where a buffer should live when there is more than one kind of memory.

@note Compiled only when `MMGR_ENABLE_EXTRAM` is set. It defaults off, and its test suite is
skipped loudly rather than silently.

## When to reach for it

A part with both internal SRAM and external PSRAM, where the trade is real: internal is fast and
scarce, external is large and slower, and some of it cannot be reached by DMA.

## What it does

````c
mmgr_place p = mmgr_exter_place(size, needs_dma, free_dram, free_psram, threshold, dram_reserve);
switch (p) {
    case PLACE_DRAM:  mmgr_pingpong_swap(pp);                             ```

## Gotchas

**`needs_dma` is not advisory.** On parts where the DMA engine cannot address external memory,
passing `MMGR_TRUE` is what stops the answer being `PLACE_PSRAM`.

**`PLACE_FAIL` is a real answer**, not an error code. Handle it.

## Reference

@ref mod_exter "Generated reference"

---

# Proximus operor — load and store {#mod_proxim_guide}

Three access strategies. Not one thing under three names.

## The three

| infix    | strategy  | use when                                                       |
| -------- | --------- | -------------------------------------------------------------- |
| `proxim` | unaligned | the address may be anything                                    |
| `aequus` | aligned   | you know the alignment holds                                   |
| `migro`  | may alias | the pointer may alias another live pointer of a different type |

```c
uint32_t v = proxim.u32(p);      uint32_t v = proxim.al_u32(p);   uint32_t v = proxim.mv_load(p);  ```

## Why they are not merged

**Merging them is a miscompile the compiler cannot report.**

An aligned load emitted for an address that is not aligned faults on some machines and silently
returns a wrong value on others. A load without the may-alias marking lets the optimizer reorder it
against a store it genuinely conflicts with — and the reordering is legal, because you told the
compiler the pointers could not alias.

This is also why `tools/dev_env/names.tsv` keeps three infixes for one module. Collapsing them onto
one stem merged seven symbol pairs, including the aligned and the unaligned load.

## Gotchas

**`aequus` does not check.** It is a promise you are making, not a request to verify.

**Unaligned is not always slower.** On x86-64 `proxim.u32` is one `mov`. The distinction matters most
on parts where it is not.

## Reference

@ref mod_proxim "Generated reference" · @ref concept_width
````
