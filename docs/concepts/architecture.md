# Region, confinium, pool, span, ring {#concept_architecture}

How one borrowed buffer becomes everything else in the library.

## The whole picture

There is exactly one source of storage: a buffer the caller already owns. MMgr never asks the system
for memory and never gives any back, because it was never holding any.

```
    caller's buffer
    ┌──────────────────────────────────────────────────────────────┐
    │                          confinium                           │
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

Four things, in the order a byte meets them.

## 1. The confinium is the region

`mmgr_confin_init` is handed a base pointer and a length and takes both on trust. From that moment
the confinium hands out storage from two ends of the same buffer:

- **persist** grows up from the base. It is for things that live as long as the region does.
- **interim** grows down from the top. It is the working space for one operation.

They grow toward each other. A take that would put them past one another fails and returns `NULL`
rather than overrunning. `mmgr_confin_octas_praesto` reports the gap still between them — "bytes at
hand". It is not a release; it answers _how much is left_.

Nothing in a confinium is ever individually freed. `persist_reddo` exists and takes a pointer, but
it only unwinds the most recent take. That is the trade the whole library is built on: giving up
free-anything-anytime is what makes the footprint decidable.

## 2. Interim is released by mark, not by pointer

Interim is a stack.

```c
size_t m = mmgr_confin_interim_mark(&c);   /* remember where the top is */
uint8_t *work = mmgr_confin_interim_capio(&c, 512, 8);
/* ... use it ... */
mmgr_confin_interim_reddo(&c, m);          /* everything since the mark is gone */
```

Nothing is reallocated and nothing moves, so `work` still points at readable memory after the
`reddo`. It is dead all the same. A pointer handed out after a mark is invalid the moment that mark
is released, and the library cannot tell you that you kept it. This is the sharpest edge in MMgr and
it is worth reading twice.

## 3. Custodiae hand out tenants

A _custodia_ is a pool over static storage, carved into one slot per worker. A _tenant_ is one
slot's worth. There are two, with deliberately near-identical surfaces:

|         | `clarus_custodiae`                    | `occultum_custodiae`                     |
| ------- | ------------------------------------- | ---------------------------------------- |
| holds   | plaintext                             | secrets                                  |
| storage | `MMGR_PLAINTEXT_CONFIN_SIZE` per slot | `MMGR_SECURE_CONFIN_SIZE` per slot       |
| release | `reset`                               | `reset`, and `wipe`                      |
| wipe    | none                                  | `volatile`, word at a time, not elidable |

They are the same shape on purpose: moving a buffer from plaintext to secure storage should be a
change of namespace, not a rewrite. The only real difference is that `occult.wipe()` writes through
a `volatile` pointer, so the compiler may not delete the stores the way it may delete a `memset`
whose result is never read.

## 4. Spans are views, and own nothing

A span is a pointer, a length and a position. `spat.from(p, cap)` borrows; it does not allocate, and
the span dies with the buffer it was given.

There are two, and the difference is not const-correctness pedantry:

- `mmgr_spat` writes. It carries `overflow`, which **latches**.
- `mmgr_fspat` reads. It carries `err`, which also latches. The `f` is _fixus_.

Latching is the design. A long run of appends is checked once at the end rather than after each
call, and no intermediate check can be forgotten, because the flag cannot be cleared by the next
successful write.

```c
verba.put(&b, "id=");
verba.u32(&b, id);
verba.put(&b, " len=");
verba.u32(&b, len);
if (!b.ok) { /* one check, covering all four */ }
```

## 5. Rings move bytes between workers

`confinium_exclusivum_infinitas` is the only part of the library that is concurrent, and only in one
shape: **single producer, single consumer**. It is header-only and built on `<stdatomic.h>`.

It offers three things: a byte ring, a segment queue for passing whole buffers by index instead of
copying them, and a 32-slot bitmap allocator. The slot count is 32 because the held mask is a
`uint32_t`; that is why `MMGR_RING_SLOTS_MAX` is not a knob you can raise by editing one number.

## Who owns what

| Thing           | Allocates       | Frees             | Lifetime                   |
| --------------- | --------------- | ----------------- | -------------------------- |
| caller's buffer | the caller      | the caller        | outlives everything below  |
| confinium       | nothing         | nothing           | the buffer's               |
| persist take    | bumps a pointer | only by unwinding | the confinium's            |
| interim take    | bumps a pointer | by mark           | until its mark is released |
| tenant          | a pool slot     | `reset`           | until reset                |
| span            | nothing         | nothing           | its target's               |
| ring slot       | a bit in a mask | `drop`            | until dropped              |

The column that matters is the third one. Nothing in MMgr frees anything in the sense a heap does;
every "free" is either unwinding a bump pointer or clearing a bit.

## What this buys, and what it costs

**Buys.** The footprint is a compile-time number. There is no fragmentation, because there is no
general free. There is no allocation failure at an arbitrary point, because every take is against a
region whose size you chose. Worst-case timing is a pointer bump.

**Costs.** You must size it yourself, up front. Get it wrong and a take returns `NULL` in production
rather than the allocator quietly finding more. The high-water counters
(`mmgr_clarus_high_water`, and `interim_used` / `persist_used` on a confinium) exist for exactly
this: run the real workload under the `checks` environment, read the marks, then size the region.

See @ref concept_zero_heap for the argument, @ref concept_ownership for the lifetime rules in
detail, and @ref ref_configuration for the knobs that set the sizes.
