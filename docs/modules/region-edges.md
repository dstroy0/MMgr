# Confinium exclusivum infinitas — the lock-free edge {#mod_infin_guide}

Single-producer, single-consumer. A byte ring, a segment queue, and a loculus bitmap.

## When to reach for it

- One context produces and exactly one consumes.
- An interrupt hands bytes to a task, or a task hands buffers to a driver.
- You want that handover without a lock.

## What it composes with

It moves what @ref mod_confin_guide and the pools handed out. The segment queue passes whole buffers
**by index**, so nothing is copied across the boundary.

## Three things in one module

**A byte ring.** `available`, `read_byte`, `read`, `peek`, `consume`, `free_`, `seek`. Capacity
must be a power of two — the wrap is a mask, not a modulo.

**One ingestion path.** `singularitas` and `detach`. Everything that enters the ring enters through
`singularitas`; there is no other way in and no second writer. See below.

**A segment queue.** `seg_next`, `seg_publish`, `seg_front`, `seg_release`, `seg_at`, `seg_inflight`.
The producer fills a segment and publishes its index; the consumer reads that index and releases it
when done. The bytes never move.

**A loculus bitmap.** `loculus_take`, `loculus_hold`, `loculus_drop`, `loculus_mark`, `loculus_clear`, `loculus_ready`,
`loculus_next`. Thirty-two loculi, because the held mask is a `uint32_t` — which is why
`MMGR_RING_LOCULI_MAX` is not a number you can raise by editing it.

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

## Singularitas — the way in

One path, one cursor, one holder. A writer asks for so many **units** and is handed an address or
refused; it never says where the address will be. The unit is fixed when the path is opened — a byte
at a time up to a machine word at a time — and every count on the path is in units, never in bytes.

That is the whole trick behind the zero-copy ingest. Because the head only ever moves by whole units,
a head that starts aligned stays aligned, and a granted address is one a DMA channel can be handed
exactly as it stands. Nothing rounds and nothing decays after the first short transfer.

```c
static const SingularitasCfg mine = {&me, 4u};      /* words */
size_t tess = 0, got = 0;

/* Ask for a run. Fall back to whatever fits, which is the ask that gets an answer at the wrap. */
uint8_t *at = iteratio_infinita.singularitas(
    &(InfinCfg){.r = &ring, .n = 16u, .tessera = &tess, .sing = &mine, .units = &got});

dma.tx_submit(ch, at, (uint16_t)(got * 4u));        /* the channel fills it */

/* From the completion: publish what landed and take the next run in the same call. */
iteratio_infinita.singularitas(
    &(InfinCfg){.r = &ring, .off = landed, .tessera = &tess, .sing = &mine, .units = &got});
```

Pass `.src` instead and it is a synchronous copy — the ring lays the bytes down itself and publishes
them, which is the whole of a producer that already holds its data.

**A grant is a destination, not a credential.** The address lets you write. Only the tessera lets you
publish, and the cursor moves by an offset handed back through the ring. A bare pointer entitles
nobody to anything, which is what makes a late completion from a torn-down channel harmless.

**Switching streams is a signal from the auctor.** `detach` is the only way the path changes hands,
and only its holder may call it. The ring will not take the path off anyone, and a grant still out
refuses the detach outright — commit it, or commit nothing, then let go. Afterwards the path reports
`MMGR_SING_READY` and the next stream may attach.

**Ask for a status on any call.** The address answers *whether*; `MMGR_SING_*` answers *why*, which
an address cannot. A producer handed NULL has to choose between waiting, asking for less and giving
up, and `FULL`, `ALIEN`, `STALE` and `GRANTED` are what tell it which. A status is one 16-bit scalar
on every target — constants in the low octet, the segment index in the high one — so the handling is
written once and does not change shape with the machine.

## Gotchas

**SPSC only.** Two producers on one ring is a broken program, not a slow one. There is no check and
no assert — the data structure simply does not have the ordering to make it safe.

**A grant never wraps.** A channel takes one base and one length, so a claim is clipped at the end of
the buffer. Asking for an exact count near the end is refused forever; `.n = 0` with `.units` set is
the ask that gets an answer there, and a producer that only ever asks for a fixed run will stall at
the wrap.

**The unit ceiling follows the machine.** `MMGR_SING_GRANULE_MAX` is `sizeof(mmgr_word)`, so a
four-byte unit is legal on a 32-bit target and refused on a 16-bit one. Derive it, do not write a
number.

**The buffer is aligned before the ring gets it.** The ring does not check and has no way to. A unit
wider than the buffer's own alignment is the consumer's mistake to not make.

**Capacity must be a power of two.** The index wrap is a mask.

**Header-only, `static inline`, `<stdatomic.h>`.** It is the only module that includes it.

**`loculus_ctz` is a SWAR popcount fold, not a builtin.** Same reasoning as the rest of the library —
see @ref concept_swar.

## Reference

@ref mod_infin "Generated reference" · @ref concept_ownership for what a pointer's lifetime is

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
