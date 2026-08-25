# Region, pool, span, ring {#concept_architecture}

How one borrowed buffer becomes everything else in the library.

## The whole picture

There is exactly one source of storage: a buffer the caller already owns. MMgr never asks the system
for memory and never gives any back, because it was never holding any.

```
    caller's buffer
    ┌──────────────────────────────────────────────────────────────┐
    │                       carceribus region                      │
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
    carceribus [init] ──► secura / soluta [spatium] ──► operation
```

A region is carved at compile time; a custodia takes a tenancy out of one of its pools and hands
back its start; a span bounds that tenancy; and the text, scan and copy calls work against the span.
Rings are the fifth, and sit alongside that line rather than on it.

## 1. The region and its pools

`mmgr_carcer_init` is a declaration. It emits the storage, a @ref CarcerCtx per pool, and static
asserts that the region has an address and an extent. From then on a pool hands out storage from
two ends of the same buffer:

- **persist** grows up from the base. It is for things that live as long as the region does.
- **interim** grows down from the top. It is the working space for one operation.

They grow toward each other, and neither take looks at the gap. `persist_capio` and `interim_capio`
move the offset and hand back the address; there is no branch in either one and no value they can
return that means no. `mmgr_carcer_octas_praesto` reports the gap still between them — "bytes at
hand" — and reading it before a take is what keeps the two ends apart. It is not a release; it
answers _how much is left_.

Nothing in a pool is ever individually freed. `persist_reddo` exists, but it takes a byte count, not
a pointer, and subtracts it from `persist_end`. It unwinds the bottom end by however much you name
and knows nothing about which take those bytes came from. That is the trade the whole library is
built on: giving up free-anything-anytime is what makes the footprint decidable.

## 2. Interim is released by mark, not by pointer

Interim is a stack.

```c
const size_t mark = MMGR_CALL(carcer.interim_mark, CarcerCfg, .pool = pool);
uint8_t *work = MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = pool, .size = 512u);
/* ... use it ... */
MMGR_CALL(carcer.interim_reset, CarcerCfg, .pool = pool);
```

`interim_mark` reports the current top. `interim_reset` assigns the top the pool's size, releasing
every interim take at once.

`interim_reddo` is the entry for winding back to one mark rather than all of them. As the code
stands it assigns `interim_top` the value `interim_mark` reports for that same pool — the value it
already holds — and it never reads `c->size`, so handing it a mark changes nothing. Use
`interim_reset` until that is settled.

Nothing is reallocated and nothing moves, so `work` still points at readable memory after a reset.
It is dead all the same. A pointer handed out after a mark is invalid the moment that mark is
released, and the library cannot tell you that you kept it. This is the sharpest edge in MMgr and it
is worth reading twice.

## 3. Custodiae hand out tenants

A _custodia_ is a pool over static storage. A _tenant_ is what it hands out of it. There are two,
with deliberately near-identical surfaces:

|         | `custodia_soluta` (`soluta`)          | `custodia_secura` (`secura`)             |
| ------- | ------------------------------------- | ---------------------------------------- |
| holds   | plaintext                             | secrets                                  |
| storage | `MMGR_PLAINTEXT_CONFIN_SIZE`          | `MMGR_SECURE_CONFIN_SIZE`                |
| entries | `init`, `release`, `used`             | `init`, `release`, `used`, `wipe`        |
| release | returns the bytes as they are         | clears them first                        |

They are the same shape on purpose: moving a buffer from plaintext to secure storage should be a
change of namespace, not a rewrite. The one difference is `release`, and it costs a pass over the
bytes, which is why both exist rather than one that always clears.

`secura.wipe` clears in place without releasing. It is an unrolled word-at-a-time store loop, and
the stores are **not** `volatile` — a compiler that could prove the region is dead afterwards would
be entitled to drop them. It survives because the storage comes from a pool in another translation
unit, which is a property of this build rather than a guarantee. Whole words only: a length that is
not a multiple of `sizeof(uintptr_t)` leaves the trailing bytes alone.

## 4. Spans are views, and own nothing

A span is a pointer, a length and a position. `spat.init` borrows; it does not allocate, and the
span dies with the buffer it was given.

It carries no flag. Whether a write fits is decided where the call is written - the caller has the
buffer and the length in front of it - so a write past the end is a program that should not have
been built, and `MMGR_ASSERT` says so: nothing in a shipping build, an abort in `checks`. There is
no state to carry the answer and nothing to check afterwards.

A read is a buffer, how far it may go, and where it is. Those are the members
@ref mod_cellul_guide names `src`, `cap` and `at`. A struct holding the three added a second
spelling and no information.

```c
size_t at = 0;
at = MMGR_CALL(verba.put, VerbaCfg, .out = buf, .cap = n, .at = at, .text = "id=");
at = MMGR_CALL(verba.u32, VerbaCfg, .out = buf, .cap = n, .at = at, .val = id);
at = MMGR_CALL(verba.put, VerbaCfg, .out = buf, .cap = n, .at = at, .text = " len=");
at = MMGR_CALL(verba.u32, VerbaCfg, .out = buf, .cap = n, .at = at, .val = len);

/* One check, covering all four: a writer with no room returns cap, and so does every writer
   after it, so finish reports zero. */
if (MMGR_CALL(verba.finish, VerbaCfg, .out = buf, .cap = n, .at = at) == 0u) { }
```

## 5. Rings move bytes between a producer and a consumer

`confinium_exclusivum_infinitas` is the only part of the library that is concurrent, and only in one
shape: **single producer, single consumer**. Every atomic access goes through the module's own
`MMGR_ATOMIC_LOAD`, `MMGR_ATOMIC_STORE` and `MMGR_ATOMIC_CLEAR`, which today expand to the
`_explicit` forms from `<stdatomic.h>`.

It offers three things: a byte ring, drain runs that hand out whole segments by index instead of
copying them, and one exclusive writer grant.

The ring's size is yours to pick. `cap` is the bytes in the buffer you hand `mmgr_infin_init`, and
any non-zero power of two is accepted — the target's word width has nothing to say about it. What
the word width bounds is `nsegs`, the number of segments that ring is divided into: a run's holds
live one bit each in the single `_Atomic mmgr_word` you supply as `held`, so `MMGR_RING_LOCULI_MAX`
is `MMGR_WORD_BITS` — 64 segments on a 64-bit build, 16 on a 16-bit one. `init` refuses an `nsegs`
that is above that, above `cap`, or not a power of two. That one bound is not a knob you can raise
by editing a number, because the whole mask has to be claimed in one atomic operation.

## Who owns what

| Thing           | Allocates       | Frees             | Lifetime                   |
| --------------- | --------------- | ----------------- | -------------------------- |
| caller's buffer | the caller      | the caller        | outlives everything below  |
| pool            | nothing         | nothing           | the region's               |
| persist take    | bumps a pointer | only by unwinding | the pool's                 |
| interim take    | bumps a pointer | `interim_reset`   | until the next reset       |
| tenant          | a pool's buffer | `release`         | until released             |
| span            | nothing         | nothing           | its target's               |
| ring segment    | a bit in a mask | the drain run     | until that run finishes    |

The column that matters is the third one. Nothing in MMgr frees anything in the sense a heap does;
every "free" is either unwinding a bump pointer or clearing a bit.

## What this buys, and what it costs

**Buys.** The footprint is a compile-time number. There is no fragmentation, because there is no
general free. There is no allocation failure at an arbitrary point, because every take is against a
region whose size you chose. Worst-case timing is a pointer bump.

**Costs.** You must size it yourself, up front. Get it wrong and nothing tells you: the take returns
an address either way, and the two ends walk past each other. No allocator quietly finds more, and
no return value reports the miss. The usage counters exist for exactly this: run the real workload
under the `checks` environment, read them, then size the region.

`mmgr_carcer_persist_used`, `mmgr_soluta_used` and `mmgr_secura_used` report what is outstanding
right now, and `mmgr_carcer_octas_praesto` reports what is left.

For the peak rather than the current value, turn on `MMGR_ENABLE_HW_MEM_CAPACITY_CB`. Both takes
then keep the largest they have seen in the `hw` field of the pool's @ref CarcerCtx, which is the
hardware heap and stack cap: persist records `persist_end`, interim records `size - interim_top`.
It is off by default, and there is no entry that returns it — read the field.

See @ref concept_zero_heap for the argument, @ref concept_ownership for the lifetime rules in
detail, and @ref ref_configuration for the knobs that set the sizes.
