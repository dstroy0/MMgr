# Memoria anularis: the lock-free edge {#mod_anular_guide}

Single-producer, single-consumer. A byte ring, a segment view over the same bytes, and loculus
keepouts.

## When to reach for it

- One context produces and exactly one consumes.
- An interrupt passes bytes to a task, or a task passes buffers to a driver.
- You want that handover without a lock.

## What it composes with

It moves what @ref mod_confin_guide passed out. The segment view passes whole buffers **by index**,
and nothing crosses the boundary as a copy.

## Three things over the same bytes

**A byte ring.** `available`, `vacant`, `put`, `read`, `read_byte`, `peek`, `consume`. Capacity must
be a power of two: the wrap is a mask instead of a modulo.

**A segment view.** `seg_next`, `seg_publish`, `seg_front`, `seg_release`, `seg_at`, `seg_inflight`.
The producer fills a segment and publishes its index; the consumer reads that index and releases it
when done. The bytes never move.

**Loculi.** `loculus_ready`, `loculus_next`, `loculus_hold`, `loculus_keepout`, `loculus_drop`,
`loculus_mark`. A numbered hold that records a region to retain out of, for a reader that walks bytes in
place. Their free and pinned masks are one machine word each, and so `MMGR_RING_LOCULI_MAX` is
`EMBED_WORD_BITS`. `MMGR_RING_LOCULI` is the build knob under that ceiling.

A static assert names it if a build sets it higher. A build with no use for them sets it to `0` and gets the keepout storage back.

## Worked example

The caller declares the ring and supplies the bytes. Nothing of the state is reachable from outside:

```c
ParsMemoriaeInternae(ring_bytes, 4096);
MemoriaAnularis(ring, ring_bytes);

EMBED_CALL(anularis.init, AnularisCfg, .ring = &ring, .buf = mmgr_pars_storage_ring_bytes,
           .capacity = ring_bytes_bytes, .segment_count = 8u);
```

`MemoriaAnularis` declares the ring's storage and claims the pool. The claim stops a second
dress of those same bytes, as a cellblock or as another ring. That would put two sets of records
over one region, with nothing at run time to report it.

`.segment_bytes` gives the segments their size where the capacity divided by the count is not the size wanted. The
capacity can then be any size that holds them. A segment's place is its index times its size, and the segment
counters wrap by the count, which stays a power of two. Such a ring has no byte stream: `vacant` reads 0, `put`
refuses, and `read` takes nothing.

To fill and to drain:

```c
if (!EMBED_CALL(anularis.put, AnularisCfg, .ring = &ring, .src = msg, .bytes = n)) {
    /* the whole span did not fit; nothing was written */
}

const size_t got = EMBED_CALL(anularis.read, AnularisCfg, .ring = &ring, .dst = out, .bytes = n);
```

To pass whole segments by index instead, with no copy across the boundary:

```c
size_t seg = 0;

if (EMBED_CALL(anularis.seg_next, AnularisCfg, .ring = &ring, .out_index = &seg)) {
    fill(EMBED_CALL(anularis.seg_at, AnularisCfg, .ring = &ring, .index = seg));
    EMBED_CALL(anularis.seg_publish, AnularisCfg, .ring = &ring);
}

if (EMBED_CALL(anularis.seg_front, AnularisCfg, .ring = &ring, .out_index = &seg)) {
    use(EMBED_CALL(anularis.seg_at, AnularisCfg, .ring = &ring, .index = seg));
    EMBED_CALL(anularis.seg_release, AnularisCfg, .ring = &ring);
}
```

`seg_next` and `seg_front` answer whether there is one and write the index through `.out_index`, and therefore
a caller never needs a sentinel index to mean "none".

## Gotchas

**SPSC only.** Two producers on one ring is a broken program and not a slow one. There is no check and
no assert: exactly one producer advances `head` and exactly one consumer advances `tail`. For that reason alone
acquire and release ordering is enough and no entry on those two needs a
read-modify-write.

**`put` is all or nothing.** It checks the whole span against `vacant` first. A partial write
never happens, and no reader ever sees a half span. It answers `EMBED_FALSE` and does not write what
fits.

**`peek` copies what you ask for regardless of whether it arrived.** Read `available` first. A count above
the ring's capacity clamps to it there, since one lap is all two runs can express.

**Nothing ever zeroes.** A loculus retains its bytes after a drop. A restream can run again over
the same region. The ring does not scrub bytes behind the cursor either.

**Capacity must be a power of two.** The index wrap is a mask. `nsegs` likewise, and at max `cap`.

**Align the buffer before the ring gets it.** The ring does not check and has no way to.

**`loculus_next` is a SWAR popcount fold and not a builtin.** Same reasons as the rest of the library,
see @ref concept_swar.

**Self-contained.** It reaches nothing outside `config`. Its own `.c` defines the span mover, the bit
index and the span type it records, and the atomics of embedded_types stay there too.

## Reference

@ref mod_anular "Generated reference" · @ref concept_ownership for what a pointer's lifetime is

---

# Memoria externa: DRAM or PSRAM {#mod_exter_guide}

Decides where a buffer should live when there is more than one type of memory.

@note The build compiles it only when `MMGR_ENABLE_EXTRAM` is on. It defaults off, and the build
skips its test suite loudly and not silently.

## When to reach for it

A part with both internal SRAM and external PSRAM, where the trade is real. Internal is fast and
scarce, external is large and slower, and DMA cannot reach some of it.

## What it does

Every entry takes one argument pack, as the rest of the library does, and so the six figures have
names and not positions:

```c
const mmgr_place where = EMBED_CALL(exter.place, ExternaCfg, .size = want, .dma_required = needs_dma,
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
EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);
const uint8_t filling = EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair);
const uint8_t draining = EMBED_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair);
const uint8_t now = EMBED_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair);
```

## Gotchas

**`dma_required` is not advisory.** On parts where the DMA engine cannot address external memory,
an `EMBED_TRUE` there stops the answer `PLACE_PSRAM`.

**`PLACE_FAIL` is a real answer**, not an error code. Handle it.

## Reference

@ref mod_exter "Generated reference"

---

# Proximus operor: load and store {#mod_proxim_guide}

Three access strategies. Not one thing under three names.

## The three

One dispatch table, `proxim` (`src/proximus_operor/proximus_operor.h:218`). The strategy is in the
entry name and not in a table of its own. The may-alias part is in the type every entry moves, and
not in a third entry to pick.

| entry               | strategy  | use when                                                                            |
| ------------------- | --------- | ----------------------------------------------------------------------------------- |
| `load`, `put`       | unaligned | the address may be anything                                                         |
| `al_load`, `al_put` | aligned   | you know the alignment holds                                                        |
| `mmgr_migro_word`   | may alias | the type the above move. A load cannot be reordered against a store of another type |

```c
const mmgr_migro_word any = EMBED_CALL(proxim.load, ProximusCfg, .at = p);
const mmgr_migro_word ali = EMBED_CALL(proxim.al_load, ProximusCfg, .at = p);
const uint32_t narrow = EMBED_CALL(proxim.load32, ProximusCfg, .at = p);
```

## Why they are not merged

**A merge of them is a miscompile the compiler cannot report.**

An aligned load emitted for an address without that alignment faults on some machines and silently
returns a wrong value on others. A load without the may-alias mark lets the optimizer reorder it
against a store it genuinely conflicts with. That reorder is legal, because you told the compiler
the pointers could not alias.

This is also why `tools/dev_env/names.tsv` retains three infixes for one module. One stem for all
three merges seven symbol pairs, the aligned and the unaligned load among them.

## Gotchas

**`aequus` does not check.** It is a promise you make and not a request to verify.

**Unaligned is not always slower.** On x86-64 `proxim.u32` is one `mov`. The distinction matters max
on parts where it is not.

## Reference

@ref mod_proxim "Generated reference" · @ref concept_width
