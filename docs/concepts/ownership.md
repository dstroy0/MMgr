# Who owns what {#concept_ownership}

Every pointer in this library belongs to something with a shorter life than you expect. This page is
the rules.

## The one rule

**MMgr owns nothing.** Every byte it hands out is inside a buffer you gave it. Every "allocation" is
a bump of a pointer into that buffer, and every "free" either unwinds that bump or clears a bit.

So the question is never "has this been freed" — it is "is the thing it points into still valid, and
has the position it was taken from moved back past it".

## Lifetimes, shortest first

| what you hold       | dies when                                  |
| ------------------- | ------------------------------------------ |
| an interim pointer  | its mark is released, or the region resets |
| a span over interim | same, and it does not know                 |
| a tenant pointer    | the pool slot is `reset`                   |
| a persist pointer   | the region does                            |
| the region          | your buffer does                           |
| your buffer         | you say so                                 |

Read that table downward: everything above a row is invalidated by the row below it. A span over a
tenant dies when the tenant is reset even though the span was never told.

## Interim is a stack, and a mark is the only handle

```c
size_t m = mmgr_confin_interim_mark(&c);
uint8_t *p = mmgr_confin_interim_capio(&c, 256, 8);
mmgr_confin_interim_reddo(&c, m);
/* p is dead here. It still points at readable memory. */
```

Nothing is reallocated and nothing is scrubbed, so `p` dereferences without faulting and returns
whatever is there — which, after the next take, is somebody else's data. This is the single most
likely way to misuse this library.

The discipline that prevents it: **a mark and its `reddo` live in the same function**, and no pointer
taken between them is returned or stored anywhere that outlives the function. If you need the result
to survive, copy it into persist or into a caller-supplied span before rewinding.

## Spans borrow and do not know

```c
mmgr_spat s = spat.from(p, 256);
```

`s` holds `p`. It does not own it, cannot extend it, and will not notice when it dies.
`spat.has_storage(s)` asks whether the span was given a buffer at all — not whether that buffer is
still alive, which is not a question a span can answer.

Consequence worth stating: a span is safe to copy by value, safe to pass by value, and safe to
return **only if its target outlives the return**. Returning a span over interim from a function that
rewinds its own mark is a use-after-free with extra steps.

## Tenants and worker slots

A pool carves its static storage into one slot per worker plus a ghost slot:

```
MMGR_WORKER_COUNT = 4

  slot 0   slot 1   slot 2   slot 3   slot 4 (ghost)
  worker0  worker1  worker2  worker3  no owner
```

`mmgr_worker_self()` decides which slot a call gets. At `MMGR_WORKER_COUNT == 1` it is a compile-time
constant `0` and there is no platform hook to supply; above 1 you must provide
`mmgr_platform_context_id()`.

`MMGR_GHOST_WORKER_SLOT` is where a call with no owning worker lands — an interrupt, or
initialization before the scheduler exists. It is a real slot with real storage, not an error value.

Two entries answer ownership questions directly:

```c
clarus.owns(p)      /* is this pointer inside this pool at all */
clarus.slot_of(p)   /* which worker's slot, if so */
```

They exist for asserts and for debugging, not for control flow. Code that has to ask which slot a
pointer came from usually wants to be passing the slot around instead.

## Concurrency

There isn't any, unless you configured it.

At `MMGR_WORKER_COUNT == 1` there is no synchronization anywhere in the library, because there is
nothing to synchronize. Above 1, the pools are partitioned by slot — each worker touches its own
tenant and no locking is needed **as long as a pointer does not cross workers**. Nothing enforces
that.

The one genuinely concurrent module is `confinium_exclusivum_infinitas`, and it is
**single-producer, single-consumer only**. Two producers on one ring is not a slower correct program,
it is a broken one. See @ref mod_infin_guide.

## What the checks environment catches

Build with `MMGR_DEBUG_CHECKS=1` and an aborting `MMGR_ASSERT` and the contract asserts compile in:
a take against a region that was never initialized, an alignment larger than
`MMGR_CONFIN_MAX_ALIGN`, a `reddo` to a mark that is ahead of the current position.

It does **not** catch a stale interim pointer. Nothing can — the memory is readable, the value is
plausible, and there is no bit anywhere recording that the mark moved. That one is on you, and it is
why the mark-and-rewind pattern is worth being rigid about.
