# Region, pool, span, ring {#concept_architecture}

How one borrowed buffer becomes everything else in the library.

## The whole picture

There is exactly one source of storage: a buffer the caller already owns. MMgr never asks the system
for memory and never gives any back, because it was never holding any.

```
    caller's buffer
    ┌──────────────────────────────────────────────────────────────┐
    │                     locus carcerum region                    │
    │  persist ──────────►                    ◄────────── interim  │
    │  ┌────────┬────────┐                    ┌────────┬────────┐  │
    │  │ tenant │ tenant │      free          │  mark  │  mark  │  │
    │  └────────┴────────┘                    └────────┴────────┘  │
    └──────────────────────────────────────────────────────────────┘
         │            │                            │
         ▼            ▼                            ▼
       span         span                     released as a
      (a view)     (a view)                  stack, newest first
```

Five things, in the order a byte meets them. The first four are one path:

```
    LocusCarcerum [declaration] ──► prison.pool.persistent_buf_alloc ──► spatium ──► operation
```

A region is carved at compile time; a pool takes a tenancy out of it and hands back its start; a
span bounds that tenancy; and the text, scan and copy calls work against the span. Rings are the
fifth, and sit alongside that line rather than on it.

## 1. The region and its pools

`LocusCarcerum` is a declaration, not a call. It emits each pool's storage, its alignment, its state
and the entries bound to it, and nothing runs at startup. From then on a pool hands out storage
from two ends of its own bytes:

- **persist** grows up from the base. It is for things that live as long as the region does.
- **interim** grows down from the top. It is the working space for one operation.

They grow toward each other, and neither take looks at the gap. `persistent_buf_alloc` and `temporary_buf_alloc`
move the offset and hand back the address; there is no branch in either one and no value they can
return that means no. `buf_available` reports the gap still between them — "bytes at
hand" — and reading it before a take is what keeps the two ends apart. It is not a release; it
answers _how much is left_.

`persistent_buf_release` takes the cell itself — `void (*persistent_buf_release)(void *prisoner)`
(`src/locus_carcerum/locus_carcerum.h:117`) — because the block's own header carries its size, so
the release does not have to be told. A block freed mid-chain stays in the chain until a release
reaches the boundary, so the bottom end does not recover on every free. That is the trade the whole
library is built on: giving up free-anything-anytime is what makes the footprint decidable.

## 2. Interim is released by mark, not by pointer

Interim is a stack.

```c
const size_t mark = prison.work.temporary_buf_mark();
uint8_t *buf = prison.work.temporary_buf_alloc(512);
/* ... use it ... */
prison.work.temporary_buf_reset();
```

`temporary_buf_mark` reports the current top. `temporary_buf_reset` assigns the top the pool's size, releasing
every interim take at once.

`temporary_buf_release` winds back to one mark rather than all of them, and takes that mark as `.mark`. The
caller holds it, so savepoints nest: an inner mark and its rollback leave an outer one standing.
`temporary_buf_reset` is the same step against the pool's own size.

Nothing is reallocated and nothing moves, so `work` still points at readable memory after a reset.
It is dead all the same. A pointer handed out after a mark is invalid the moment that mark is
released, and the library cannot tell you that you kept it. This is the sharpest edge in MMgr and it
is worth reading twice.

## 3. A cell is given back by one call, and the pool decides what that costs

A _cell_ is what a pool hands out. Every one is taken with the pool's own `persistent_buf_alloc` and
given back with its own `persistent_buf_release`. There is one release, not two — what differs is
which guard the pool was declared under:

|                      | `MMGR_MINIMUM_SECURITY`    | `MMGR_MAXIMUM_SECURITY`                    |
| -------------------- | -------------------------- | ------------------------------------------ |
| gives the bytes back | yes                        | yes                                        |
| clears them first    | no                         | yes (`locus_carcerum.h:140`)               |
| costs                | a chain walk               | a chain walk and a pass over the bytes     |

The guarantee is in the **declaration** rather than in a flag or a second call, so a caller cannot
ask for a wipe and not get one, and cannot reach for an unwiped release on a pool that promised one.
The extent cleared is the block's own, read from its header, so a caller cannot under-wipe a cell by
naming fewer bytes than it holds.

Two guards exist rather than one that always clears because the clear costs a pass over the bytes,
and most cells hold nothing worth paying it for. Which storage is which is a matter of declaring two
pools and handing secrets to the maximum-security one. Their sizes are the extent of a row you
declared; `MMGR_PLAINTEXT_CONFIN_SIZE` and `MMGR_SECURE_CONFIN_SIZE` do not size them and nothing in
`locus_carcerum` reads those two. They state the largest region the build intends to declare, which
is what `MMGR_CARCER_MAX` bounds the scanner and the string shim against.

Bytes are cleared on release, not on hand-out. A take does **not** return zeroed storage: a block
released from a minimum-security pool and handed out again carries what the last occupant left. That
is what declaring the sensitive pool under `MMGR_MAXIMUM_SECURITY` buys.

`mmgr_zero_buf` clears an address and a count in place without giving anything back. Its stores are
`volatile` machine-width stores: a plain store there is a dead store the optimizer is entitled to
drop, and a byte loop would pay eight times the stores for the same guarantee. Byte edges cover a
length or an address that is not a whole number of words.

## 4. Spans are views, and own nothing

A span is a pointer, an extent, a position and a sticky flag. `spat.from` borrows; it does not
allocate, and the span dies with the buffer it was given.

There are two, and they are different types on purpose:

|          | `mmgr_span` | `mmgr_cspan` |
| -------- | ----------- | ------------ |
| for      | filling     | reading      |
| `buf`    | writable    | `const`      |
| extent   | `cap`       | `len`        |
| the flag | `overflow`  | `err`        |

Naming the extent differently in each is what stops one being handed where the other belongs without
the compiler saying so.

The flag is sticky, and that is what a span buys over a bare pointer and length. A caller may append
a whole message through several calls and test once at the end, rather than after each.
`spat.reset` is the one call that clears the flag.

The two flags do not mean the same kind of thing. **`overflow` is a build failure**: what a writer
emits and how big its buffer is are both fixed before the build, so there is no runtime condition
under which a correct writer overruns a correctly sized span. It asserts — nothing in a shipping
build, an abort in `checks` — and the latch is what a shipping build does with a wrong program,
keeping it from walking off the end rather than offering it somewhere to go.

**`err` is a runtime fact**: a read span runs out because whatever sent the bytes sent fewer, and
nothing was built wrong. That is why every take answers and no append returns anything — a short
read is a case to handle, an overrun append is a bug to fix. A take that reaches past the end leaves
the cursor where it was, so a caller that keeps reading after a failure still knows where it is.

A read is a buffer, how far it may go, and where it is. Those are the members
@ref mod_cellul_guide names `src`, `cap` and `at`. A struct holding the three added a second
spelling and no information.

```c
size_t at = 0;
at = MMGR_CALL(verba.put, VerbaCfg, .out = buf, .cap = n, .at = at, .text = "id=");
at = MMGR_CALL(verba.uint, VerbaCfg, .out = buf, .cap = n, .at = at, .val = id);
at = MMGR_CALL(verba.put, VerbaCfg, .out = buf, .cap = n, .at = at, .text = " len=");
at = MMGR_CALL(verba.uint, VerbaCfg, .out = buf, .cap = n, .at = at, .val = len);

/* One check, covering all four: a writer with no room returns cap, and so does every writer
   after it, so finish reports zero. */
if (MMGR_CALL(verba.finish, VerbaCfg, .out = buf, .cap = n, .at = at) == 0u) { }
```

## 5. Rings move bytes between a producer and a consumer

`memoria_anularis` is the only part of the library that is concurrent, and only in one
shape: **single producer, single consumer**. Exactly one producer advances `head` and exactly one
consumer advances `tail`, so ordering is all that is needed: every atomic access goes through the
module's own `MMGR_ATOMIC_LOAD` and `MMGR_ATOMIC_STORE`, acquire and release, and no entry on those
two takes a lock or a read-modify-write.

It offers three things over the same bytes: a byte ring, a segment view that hands out whole
segments by index instead of copying them, and loculi — numbered holds that record a region to keep
out of, for a reader walking bytes in place.

The ring's size is yours to pick. `cap` is the bytes in the buffer you hand `mmgr_anular_init`, and
any non-zero power of two is accepted, because the ring wraps by masking. `nsegs` is a power of two
at most `cap`.

What the word width bounds is the loculi, not the segments: their free and held masks are one
machine word each, so `MMGR_RING_LOCULI_MAX` is `MMGR_WORD_BITS` — 64 on a 64-bit build, 16 on a
16-bit one. `MMGR_RING_LOCULI` is a build knob under that ceiling, and a static assert names it if
it is set higher. A build with no use for the loculus view sets it to `0` and gets the keepout
storage back.

The caller declares the ring as an `mmgr_ring` and supplies the bytes. Everything else — the two
cursors, the segment counters, the masks and the keepout records — lives inside that storage and is
declared nowhere a consumer can reach.

## Who owns what

| Thing           | Allocates               | Frees                      | Lifetime                  |
| --------------- | ----------------------- | -------------------------- | ------------------------- |
| caller's buffer | the caller              | the caller                 | outlives everything below |
| pool            | nothing                 | nothing                    | the region's              |
| persist take    | a block from the middle | `persistent_buf_release` by address | as long as it likes       |
| interim take    | a block from the middle | a mark, or `temporary_buf_reset` | until that mark           |
| span            | nothing                 | nothing                    | its target's              |
| ring segment    | a counter step          | `seg_release`              | until released            |
| loculus         | a bit in a mask         | `loculus_drop`             | until dropped             |

The column that matters is the third one. Nothing in MMgr reaches an allocator: every take comes out
of a region the caller declared, and every free either returns a block to that region's own chain,
moves a boundary, or clears a bit.

The persistent end is the one exception to "nothing is ever really freed" — it keeps a chain of
blocks, so a release there is a genuine free that merges with its neighbours and can be reused. The
interim end is not: nothing is released one at a time, and the whole run comes back at once.

## What this buys, and what it costs

**Buys.** The footprint is a compile-time number. There is no fragmentation, because there is no
general free. There is no allocation failure at an arbitrary point, because every take is against a
region whose size you chose. Worst-case timing is a pointer bump.

**Costs.** You must size it yourself, up front. Get it wrong and the take returns NULL — the region
fails closed rather than letting the two ends walk into each other, but no allocator quietly finds
more. The counters exist for exactly this: run the real workload under the `checks` environment,
read them, then size the region.

`pool->persistent_end` is how far the bottom has reached and `pool->temporary_top` how far the top has,
both read straight off the pool's own state. `buf_available` reports the gap
between them.

For the peak rather than the current value, turn on `MMGR_ENABLE_HW_MEM_CAPACITY_CB`. Each end then
keeps the largest it has seen in its own field — `persistent_hw` records `persistent_end`, `temporary_hw`
records `size - temporary_top` — one per end, so neither is a maximum over the other. It is off by
default, and there is no entry that returns either: read the field.

See @ref concept_zero_heap for the argument, @ref concept_ownership for the lifetime rules in
detail, and @ref ref_configuration for the knobs that set the sizes.
