# Who owns what {#concept_ownership}

Every pointer in this library belongs to something with a shorter life than you expect. This page is
the rules.

## The one rule

**MMgr owns nothing.** Every byte it hands out is inside a buffer you gave it. Every "allocation" is
a bump of a pointer into that buffer, and every "free" either unwinds that bump or clears a bit.

So the question is never "has this been freed" — it is "is the thing it points into still valid, and
has the position it was taken from moved back past it".

## Lifetimes, shortest first

| what you hold       | dies when                                             |
| ------------------- | ----------------------------------------------------- |
| an interim pointer  | its mark is rewound, or the interim end is reset      |
| a span over interim | same, and it does not know                            |
| a tenant pointer    | the pool releases it                                  |
| a persist pointer   | `persist_reddo` unwinds past it, or the region dies   |
| the region          | its declaration goes out of scope, which is never     |
| your buffer         | you say so                                            |

Read that table downward: everything above a row is invalidated by the row below it. A span over a
tenant dies when the tenant is reset even though the span was never told.

## Interim is a stack, and a mark is the only handle

```c
const size_t mark = MMGR_CALL(carcer.interim_mark,  CarcerCfg, .pool = pool);
char *const  p    = MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = pool, .size = 256u);

/* ... use p ... */

MMGR_CALL(carcer.interim_reddo, CarcerCfg, .pool = pool, .size = mark);
```

Nothing is reallocated and nothing is scrubbed, so `p` dereferences without faulting and returns
whatever is there — which, after the next take, is somebody else's data. This is the single most
likely way to misuse this library.

The discipline that prevents it: **a mark and its `reddo` live in the same function**, and no pointer
taken between them is returned or stored anywhere that outlives the function. If you need the result
to survive, copy it into persist or into a caller-supplied span before rewinding.

## Spans borrow and do not know

```c
mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = p, .cap = 256u);
```

`s` holds `p`. It does not own it, cannot extend it, and will not notice when it dies.
`s.buf` says which buffer it was given. Whether that buffer is still alive is not a question a span
can answer.

Consequence worth stating: a span is safe to copy by value, safe to pass by value, and safe to
return **only if its target outlives the return**. Returning a span over interim from a function that
rewinds its own mark is a use-after-free with extra steps.

## Tenants

A pool is one tenant over one static buffer. It does not carve that buffer up and it does not ask
who is calling — a memory manager has no concept of a worker, and the moment it tries to have one it
needs a count of them, an index per take, a registry mapping platform contexts to that index, and a
spare tenant for when the index is wrong.

One entry answers an ownership question directly:

```c
/* is this pointer inside this pool at all */
MMGR_CALL(carcer.owns, CarcerCfg, .pool = pool, .at = p);
```

One unsigned subtract and one compare. It exists for asserts and for debugging, not for control
flow — it says the pointer is inside the pool's storage, not that it is live.

If two execution contexts must not share, declare two regions and hand each context its own. The
region never learns there were two, so there is nothing to count, nothing to index, and nothing to
check. See @ref mod_confin_guide.

## Concurrency

There isn't any, unless you configured it.

There is no synchronization anywhere in the region, because there is nothing to synchronize. A
`CarcerCtx` is a base, an extent and two cursors, and it is used by whoever holds it. Two contexts
that must not share get two regions.

The one genuinely concurrent module is `confinium_exclusivum_infinitas`, and it is
**single-producer, single-consumer only**. Two producers on one ring is not a slower correct program,
it is a broken one. See @ref mod_infin_guide.

## What a check would catch, and what none of them do

The preconditions that are asserted at all are in `spatium`, `bitorum_introitus_exitus`,
`confinium_exclusivum_infinitas`, `memoriam_praetereo` and `ascii_persona_bitorum` — a span with no
buffer, a bit writer with no capacity, a ring with no storage, a channel that does not exist, a
character class out of range.

None of them fire today. `MMGR_ASSERT` is defined once, in `mmgr_config.h`, and its definition
type-checks the condition with `sizeof` and then discards it. Nothing in the build overrides it, so
the `checks` environment produces the same object code as `host`. Define `MMGR_ASSERT` yourself, to
something that aborts, if you want those checks to be checks.

`carceribus` has no asserts at all, and that is the design rather than an omission: every size in a
region is fixed by `mmgr_carcer_init` at compile time, and the static asserts there have already
run before a single instruction executes.

What no check catches, in any configuration, is a **stale interim pointer**. Nothing can — the
memory is readable, the value is plausible, and there is no bit anywhere recording that the mark
moved. That one is on you, and it is why the mark-and-rewind pattern is worth being rigid about.
