# Confinium exclusivum infinitas — the lock-free edge {#mod_infin_guide}

Single-producer, single-consumer. A byte ring, a segment queue, and a slot bitmap.

## When to reach for it

- One worker produces and exactly one consumes.
- An interrupt hands bytes to a task, or a task hands buffers to a driver.
- You want that handover without a lock.

## What it composes with

It moves what @ref mod_confin_guide and the pools handed out. The segment queue passes whole buffers
**by index**, so nothing is copied across the boundary.

## Three things in one module

**A byte ring.** `available`, `read_byte`, `read`, `peek`, `consume`, `free`, `write_span`. Capacity
must be a power of two — the wrap is a mask, not a modulo.

**A segment queue.** `seg_next`, `seg_publish`, `seg_front`, `seg_release`, `seg_at`, `seg_inflight`.
The producer fills a segment and publishes its index; the consumer reads that index and releases it
when done. The bytes never move.

**A slot bitmap.** `slot_take`, `slot_hold`, `slot_drop`, `slot_mark`, `slot_clear`, `slot_ready`,
`slot_next`. Thirty-two slots, because the held mask is a `uint32_t` — which is why
`MMGR_RING_SLOTS_MAX` is not a number you can raise by editing it.

## Worked example

```c
/* producer */
size_t seg = infin_seg_next(&q);
if (seg != MMGR_SEG_NONE) {
    fill(infin_seg_at(&q, seg));
    infin_seg_publish(&q, seg);
}

/* consumer */
size_t seg = infin_seg_front(&q);
if (seg != MMGR_SEG_NONE) {
    use(infin_seg_at(&q, seg));
    infin_seg_release(&q, seg);
}
```

## Gotchas

**SPSC only.** Two producers on one ring is a broken program, not a slow one. There is no check and
no assert — the data structure simply does not have the ordering to make it safe.

**Capacity must be a power of two.** The index wrap is a mask.

**Header-only, `static inline`, `<stdatomic.h>`.** It is the only module that includes it.

**`slot_ctz` is a SWAR popcount fold, not a builtin.** Same reasoning as the rest of the library —
see @ref concept_swar.

## Reference

@ref mod_infin "Generated reference" · @ref concept_ownership for what crossing workers costs

---

# Confinium externum — DRAM or PSRAM {#mod_exter_guide}

Decides where a buffer should live when there is more than one kind of memory.

@note Compiled only when `MMGR_ENABLE_PSRAM_POOL` is set. It defaults off, and its test suite is
skipped loudly rather than silently.

## When to reach for it

A part with both internal SRAM and external PSRAM, where the trade is real: internal is fast and
scarce, external is large and slower, and some of it cannot be reached by DMA.

## What it does

```c
mmgr_place p = mmgr_exter_place(size, threshold, needs_dma);
switch (p) {
    case PLACE_DRAM:  /* internal */       break;
    case PLACE_PSRAM: /* external */       break;
    case PLACE_FAIL:  /* neither fits */   break;
}
```

It is a **decision**, not an allocator. It answers where a buffer of this size, with this DMA
requirement, ought to go. Taking the storage is still yours to do.

`PingPong` double-buffer index helpers ship alongside it, because the workload that needs external
memory is usually the one streaming through two buffers.

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
uint32_t v = proxim.u32(p);      /* p may be misaligned      */
uint32_t v = proxim.al_u32(p);   /* p is known aligned       */
uint32_t v = proxim.mv_load(p);  /* p may alias              */
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
