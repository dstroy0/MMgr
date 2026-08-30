# Memoria anularis — the lock-free edge {#mod_anular_guide}

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

MMGR_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = bytes, .capacity = 4096u, .segment_count = 8u);
```

Filling and draining:

```c
if (!MMGR_CALL(anularis.put, AnularisCfg, .ring = &ring, .src = msg, .bytes = n)) {
    /* the whole span did not fit; nothing was written */
}

const size_t got = MMGR_CALL(anularis.read, AnularisCfg, .ring = &ring, .dst = out, .bytes = n);
```

Passing whole segments by index instead, so nothing is copied across the boundary:

```c
size_t seg = 0;

if (MMGR_CALL(anularis.seg_next, AnularisCfg, .ring = &ring, .out_index = &seg)) {
    fill(MMGR_CALL(anularis.seg_at, AnularisCfg, .ring = &ring, .index = seg));
    MMGR_CALL(anularis.seg_publish, AnularisCfg, .ring = &ring);
}

if (MMGR_CALL(anularis.seg_front, AnularisCfg, .ring = &ring, .out_index = &seg)) {
    use(MMGR_CALL(anularis.seg_at, AnularisCfg, .ring = &ring, .index = seg));
    MMGR_CALL(anularis.seg_release, AnularisCfg, .ring = &ring);
}
```

`seg_next` and `seg_front` answer whether there is one and write the index through `.out_index`, so
a caller never needs a sentinel index to mean "none".

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

@ref mod_anular "Generated reference" · @ref concept_ownership for what a pointer's lifetime is

---

# Memoria externa — DRAM or PSRAM {#mod_exter_guide}

Decides where a buffer should live when there is more than one kind of memory.

@note Compiled only when `MMGR_ENABLE_EXTRAM` is set. It defaults off, and its test suite is
skipped loudly rather than silently.

## When to reach for it

A part with both internal SRAM and external PSRAM, where the trade is real: internal is fast and
scarce, external is large and slower, and some of it cannot be reached by DMA.

## What it does

Every entry takes one argument pack, as the rest of the library does, so the six figures are named
rather than positional:

```c
const mmgr_place where = MMGR_CALL(exter.place, ExternaCfg, .size = want, .dma_required = needs_dma,
                                   .free_dram = dram_left, .free_psram = psram_left,
                                   .psram_threshold = threshold, .dram_reserve = reserve);

switch (where) {
    case PLACE_DRAM:  /* internal */ break;
    case PLACE_PSRAM: /* external */ break;
    case PLACE_FAIL:  /* neither will take it */ break;
}
```

The two-buffer index is separate, and acts on a `PingPong` the caller owns:

```c
MMGR_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);
const uint8_t filling = MMGR_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair);
const uint8_t draining = MMGR_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair);
const uint8_t now = MMGR_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair);
```

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

One dispatch table, `proxim` (`src/proximus_operor/proximus_operor.h:218`). The strategy is in the
entry name rather than in a table of its own, and the may-alias part is in the type every entry
moves rather than in a third entry to pick.

| entry               | strategy  | use when                                                                               |
| ------------------- | --------- | -------------------------------------------------------------------------------------- |
| `load`, `put`       | unaligned | the address may be anything                                                            |
| `al_load`, `al_put` | aligned   | you know the alignment holds                                                           |
| `mmgr_migro_word`   | may alias | the type the above move, so a load cannot be reordered against a store of another type |

```c
const mmgr_migro_word any = MMGR_CALL(proxim.load, ProximusCfg, .at = p);
const mmgr_migro_word ali = MMGR_CALL(proxim.al_load, ProximusCfg, .at = p);
const uint32_t narrow = MMGR_CALL(proxim.load32, ProximusCfg, .at = p);
```

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
