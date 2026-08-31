# Who owns what {#concept_ownership}

Every pointer in this library belongs to something with a shorter life than you expect. This page is
the rules.

## The one rule

**MMgr owns nothing.** Every byte it hands out is inside a buffer you gave it. Every "allocation" is
a bump of a pointer into that buffer, and every "free" either unwinds that bump or clears a bit.

So the question is never "has this been freed" — it is "is the thing it points into still valid, and
has the position it was taken from moved back past it".

## Lifetimes, shortest first

| what you hold         | dies when                                                          |
| --------------------- | ------------------------------------------------------------------ |
| a temporary pointer   | its mark is rewound, or the temporary tier is reset                |
| a span over temporary | same, and it does not know                                         |
| a persistent pointer  | `persistent_buf_release` takes it, or its cellblock dies           |
| the cellblock         | its declaration goes out of scope, which is never                  |
| the pool              | the same, and it outlives the cellblock dressed over it            |

Read that table downward: everything above a row is invalidated by the row below it. A span over a
cell dies when that cell is released even though the span was never told.

## The temporary tier is a stack, and a mark is the only handle

```c
const size_t mark = prison.work.temporary_buf_mark();
void *const cell = prison.work.temporary_buf_alloc(256u);

/* ... use cell ... */

prison.work.temporary_buf_release(mark);
```

Nothing is reallocated, so `cell` dereferences without faulting and returns whatever is there —
which, after the next allocation, is somebody else's data. This is the single most likely way to
misuse this library. On a maximum-security cellblock the release zeroes back to the mark first, so
what is read afterwards is zeros instead of the previous prisoner; on a minimum-security one nothing
is scrubbed.

The discipline that prevents it: **a mark and its release live in the same function**, and no pointer
taken between them is returned or stored anywhere that outlives the function. If you need the result
to survive, copy it into the persistent tier or into a caller-supplied span before rewinding.

## Spans borrow and do not know

```c
mmgr_span s = EMBED_CALL(spat.from, SpatiumCfg, .buf = p, .cap = 256u);
```

`s` holds `p`. It does not own it, cannot extend it, and will not notice when it dies.
`s.buf` says which buffer it was given. Whether that buffer is still alive is not a question a span
can answer.

Consequence worth stating: a span is safe to copy by value, safe to pass by value, and safe to
return **only if its target outlives the return**. Returning a span over interim from a function that
rewinds its own mark is a use-after-free with extra steps.

## A cellblock does not ask who is calling

A cellblock is one dressing over one pool. It does not ask which context reached it — a memory
manager has no concept of a worker, and the moment it tries to have one it needs a count of them, an
index per allocation, a registry mapping platform contexts to that index, and a spare cellblock for
when the index is wrong.

One entry answers an ownership question directly:

```c
/* is this pointer inside this cellblock at all */
prison.work.who_owns_buf(cell);
```

One unsigned subtract and one compare. It exists for asserts and for debugging, not for control
flow — it says the pointer is inside the cellblock's storage, not that it is live.

If two execution contexts must not share, declare two pools and hand each context its own. Neither
cellblock learns there were two, so there is nothing to count, nothing to index, and nothing to
check. See @ref mod_confin_guide.

## Concurrency

There isn't any, and there is no knob that adds it.

There is no synchronization anywhere in a cellblock, because there is nothing to synchronize. It is a
pool, an extent and two tier boundaries, reached only through the entries bound to it. Two contexts
that must not share get two pools.

The one genuinely concurrent module is `memoria_anularis`, and it is
**single-producer, single-consumer only**. Two producers on one ring is not a slower correct program,
it is a broken one. See @ref mod_anular_guide.

## What a check would catch, and what none of them do

The preconditions that are asserted at all are in `spatium`, `bitorum_introitus_exitus`,
`memoria_anularis`, `memoriam_praetereo` and `ascii_persona_bitorum` — a span with no
buffer, a bit writer with no capacity, a ring with no storage, a channel that does not exist, a
character class out of range.

In a shipping build none of them fire. The default `MMGR_ASSERT` in `mmgr.h` type-checks the
condition with `sizeof` and then discards it, so it costs nothing and cannot rot.

The `checks` environment is where they are checks. `MMGR_DEBUG_CHECKS=1` selects the trapping form in
`test/support/mmgr_host_traps.h`, which reports the expectation, the file and the line and then
aborts — so an expectation a caller broke fails a test there instead of being a no-op nobody notices.
It is the only environment where an assert is evaluated at all, which means an expectation never
exercised under `checks` is one nothing has ever tested. A target with no `stderr` and no `abort`
defines `MMGR_ASSERT` itself before including the header, and neither form is used.

`locus_carcerum` reaches for `MMGR_FATAL` instead, which holds in every build: a release handed a
prisoner from another cellblock has shown the caller does not know which memory it is holding, and
continuing would move this cellblock's boundaries using a header read out of another's bytes.

What no check catches, in any configuration, is a **stale temporary-tier pointer**. Nothing can — the
memory is readable, the value is plausible, and there is no bit anywhere recording that the mark
moved. That one is on you, and it is why the mark-and-rewind pattern is worth being rigid about.
