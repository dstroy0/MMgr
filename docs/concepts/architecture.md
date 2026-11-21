# Region, pool, span, ring {#concept_architecture}

How one borrowed buffer becomes everything else in the library.

## The whole picture

There is exactly one source of storage: a buffer the caller already owns. MMgr never asks the system
for memory and never gives any back, because it never held any.

```
    caller's buffer
    ┌──────────────────────────────────────────────────────────────┐
    │                     locus carcerum region                    │
    │  persist ──────────►                    ◄────────── interim  │
    │  ┌────────┬────────┐                    ┌────────┬────────┐  │
    │  │ tenant │ tenant │      free          │ tenant │ tenant │  │
    │  └────────┴────────┘                    └────────┴────────┘  │
    └──────────────────────────────────────────────────────────────┘
         │            │                            │
         ▼            ▼                            ▼
       span         span                     released one cell
      (a view)     (a view)                  at a time, any order
```

Five things, in the order a byte meets them. The first four are one path:

```
    ParsMemoriae* [declaration] ──► LocusCarcerum [declaration]
                                          │
                                          ▼
                        prison.work.persistent_buf_alloc ──► spatium ──► operation
```

You declare a pool at compile time, and a cellblock dresses over it. The cellblock passes out a cell
and returns its first byte, and a span bounds that cell. The text, scan and copy calls work against
the span. Rings are the fifth, and sit beside that line and not on it.

## 1. The pools and the cellblocks over them

`ParsMemoriaeInternae` and `ParsMemoriaeExternum` declare the storage and its alignment.
`LocusCarcerum` is a declaration too and never a call: it emits each cellblock's state and the entries
bound to it, and nothing runs at startup. From then on a cellblock passes out storage from two ends of
its pool's bytes:

- **persist** grows up from the base. It is for things that live as long as the region does.
- **interim** grows down from the top. It is the space one operation works in.

They grow toward each other out of one gap, and both allocations test it. `carcer_grow` compares the
header plus the payload against the gap. It returns `NULL` and moves no boundary when the cell will
not fit. Both `persistent_buf_alloc` and `temporary_buf_alloc` can answer no, and every call site
has to test the return. `buf_available` reports the gap still between them. It is not a release; it
answers _how much stays_.

`persistent_buf_release` takes the cell itself, `void (*persistent_buf_release)(void *prisoner,
const CellOwner *owner)`. The block's own word carries its size, and so nobody has to tell the
release. A block freed mid-chain stays in the chain until a release reaches the boundary: the bottom
end does not recover on every free. That is the trade the whole library rests on: a design without
free-anything-anytime has a footprint you can decide.

## 2. Interim is persist with the base at the top

Interim is the same algorithm as persist with the arithmetic mirrored: its base is the pool's size, its boundary
the top, and it grows down.

```c
uint8_t *buf = prison.work.temporary_buf_alloc(512, reader);
/* ... use it ... */
prison.work.persistent_buf_release(buf, reader);
```

A take bumps the top down. A release frees the cell's header, and at the top it also trims the
top back up over the free cells there. A take the gap cannot meet walks the
tier alone. It merges every run of free cells into one and takes the run that fits nearest the
pool's size. It returns the free run at the top to the gap. One release serves both tiers, since a cell's
offset says which tier it lies in.

Nothing reallocates and nothing moves: `buf` still points at readable memory after its release. It
is dead all the same, and the library cannot tell you that you retained it.

## 3. One call gives a cell back, and the pool decides what that costs

A _cell_ is what a pool passes out. The pool's own `persistent_buf_alloc` takes each one, and its own
`persistent_buf_release` gives it back. There is one release and never two. What differs is which
guard the declaration of the pool names:

|                      | `MMGR_MINIMUM_SECURITY` | `MMGR_MAXIMUM_SECURITY`                |
| -------------------- | ----------------------- | -------------------------------------- |
| gives the bytes back | yes                     | yes                                    |
| clears them first    | no                      | yes (`locus_carcerum.h:140`)           |
| costs                | a chain walk            | a chain walk and a pass over the bytes |

The guarantee is in the **declaration**, never in a flag or a second call. A caller cannot ask for a
wipe and not get one, and cannot reach for an unwiped release on a pool that promised one. The
release clears the block's own extent, read from its header. A caller cannot under-wipe a cell with a
count smaller than it holds.

Two guards exist and not one that always clears. The clear costs a pass over the bytes, and max cells
hold nothing worth that cost. You settle which storage is which: declare two pools and hand secrets
to the max-security cellblock. Each one's extent is the count in its own pool declaration, and no
build knob sizes it.

A release clears bytes, and a hand-out never does. An allocation does **not** return zeroed storage:
a cell released from a minimum-security cellblock and passed out again carries what the last
prisoner left. A declaration of the sensitive pool under `MMGR_MAXIMUM_SECURITY` buys the zero.

`mmgr_zero_buf` clears an address and a count in place and gives nothing back. Its stores are
`volatile` machine-width stores. The optimizer may drop a plain store there as a dead store. A byte
loop would pay eight times the stores for the same guarantee. Byte edges cover a length or an
address that is not a whole number of words.

## 4. Spans are views, and own nothing

A span is a pointer, an extent, a position and a sticky flag. `spat.from` borrows; it does not
allocate, and the span dies with the buffer it got.

There are two, and they are different types by design:

|          | `mmgr_span` | `mmgr_cspan` |
| -------- | ----------- | ------------ |
| for      | filling     | reading      |
| `buf`    | writable    | `const`      |
| extent   | `cap`       | `len`        |
| the flag | `overflow`  | `err`        |

Each type gives the extent its own name. So the compiler says so when one goes where the other
belongs.

The flag is sticky, and the stickiness is the gain of a span over a bare pointer and length. A caller
may append a whole message through several calls and test once at the end and not after each.
`spat.reset` alone clears the flag.

The two flags do not mean the same type of thing. **`overflow` is a build failure.** The build fixes
both what a writer emits and how big its buffer is. There is no runtime condition under which a
correct writer overruns a correctly sized span. It asserts: nothing in a build you ship, an abort in
`checks`. The latch, in a shipped build, retains a wrong writer from a run off the end and offers it
nowhere to go.

**`err` is a runtime fact.** A read span runs out because whatever sent the bytes sent fewer, and
nobody built anything wrong. For that reason every take answers and no append returns anything. A
short read is a case to handle, and an overrun append is a bug to fix. A take that reaches past the
end leaves the cursor where it was. A caller that reads on after a failure still knows where it is.

A read is a buffer, how far it may go, and where it is. Those are the members @ref mod_cellul_guide
names `src`, `cap` and `at`. A struct that holds the three adds a second name and no information.

```c
size_t at = 0;
at = EMBED_CALL(verba_textus.put, VerbaTextusCfg, .out = buf, .cap = n, .at = at, .text = "id=");
at = EMBED_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = buf, .cap = n, .at = at, .val = id);
at = EMBED_CALL(verba_textus.put, VerbaTextusCfg, .out = buf, .cap = n, .at = at, .text = " len=");
at = EMBED_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = buf, .cap = n, .at = at, .val = len);

/* One check, covering all four: a writer with no room returns cap, and so does every writer
   after it, and finish reports zero. */
if (EMBED_CALL(verba_finis.finish, VerbaFinisCfg, .out = buf, .cap = n, .at = at) == 0u) { }
```

## 5. Rings move bytes between a producer and a consumer

Every context that reaches the cellblock shares it, on atomic words and with no lock.
`memoria_anularis` is concurrent in one shape: **single producer, single consumer**. Exactly one
producer advances `head` and exactly one consumer advances `tail`, and so ordering is all the ring
needs. Every atomic access goes through `embed_atomic_usize_load` and `embed_atomic_usize_store` from
embedded_types, acquire and release. No entry on those two takes a lock or a read-modify-write.

It offers three things over the same bytes. The first is a byte ring. The second is a segment view
that passes out whole segments by index and does not copy them. The third is loculi: numbered holds
that record a region to retain out of, for a reader that walks bytes in place.

The ring's size is yours to pick. `cap` is the bytes in the buffer you hand `mmgr_anular_init`. The
ring accepts any non-zero power of two, because it wraps with a mask. `nsegs` is a power of two at
max `cap`.

What the word width bounds is the loculi and never the segments. Their free and busy masks are one
machine word each, and `MMGR_RING_LOCULI_MAX` is `EMBED_WORD_BITS`: 64 on a 64-bit build, 16 on a
16-bit one. `MMGR_RING_LOCULI` is a build knob under that ceiling, and a static assert names it if a
build sets it higher. A build with no use for the loculus view sets it to `0` and gets the keepout
storage back.

The caller declares the ring as an `mmgr_ring` and supplies the bytes. Everything else lives inside
that storage: the two cursors, the segment counters, the masks and the keepout records. Nothing a
consumer can reach declares them.

## Who owns what

| Thing           | Allocates               | Frees                               | Lifetime                  |
| --------------- | ----------------------- | ----------------------------------- | ------------------------- |
| caller's buffer | the caller              | the caller                          | outlives everything below |
| pool            | nothing                 | nothing                             | the region's              |
| persist take    | a block from the middle | `persistent_buf_release` by address | as long as it likes       |
| interim take    | a block from the middle | `persistent_buf_release` by address | one call                  |
| span            | nothing                 | nothing                             | its target's              |
| ring segment    | a counter step          | `seg_release`                       | until released            |
| loculus         | a bit in a mask         | `loculus_drop`                      | until dropped             |

Read the third column. Nothing in MMgr reaches an allocator: every take comes out of a region the
caller declared. Every free returns a block to that region's own chain, moves a boundary, or clears a
bit.

A release frees the block. A block freed at the end of its tier comes off it, along with every
freed block below it in turn. A block freed under a busy one waits in the tier until that one goes.
Both ends behave the same way.

## What this buys, and what it costs

**Buys.** The footprint is a compile-time number. There is no fragmentation, because there is no
general free. There is no allocation failure at an arbitrary point, because every take is against a
region whose size you chose. Worst-case timing is a pointer bump.

**Costs.** You must size it yourself, up front. Get it wrong and the take returns NULL. The region
fails closed and does not let the two ends run into each other, but no allocator quietly finds more.
The counters exist for exactly this: run the real workload under the `checks` environment, read
them, then size the region.

`mmgr_series_cellularum_persistent_end(pool)` is how far the bottom reaches and
`mmgr_series_cellularum_temporary_top(pool)` how far the top reaches. Both read off the one atomic
word that holds the two boundaries. `buf_available` reports the gap between them.

For the peak in place of the current value, turn on `MMGR_ENABLE_HW_MEM_CAPACITY_CB`. Each end then
retains the largest value it sees in its own field, one per end, and neither is a max over the other.
`persistent_hw` records the persistent end, and `temporary_hw` records the bytes taken from the top.
It is off by default, and no entry returns either: read the field.

See @ref concept_zero_heap for the argument, @ref concept_ownership for the lifetime rules in
detail, and @ref ref_configuration for the knobs that set the sizes.
