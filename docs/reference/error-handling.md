# How failure is reported {#ref_error_handling}

There are no error codes anywhere in this library. There are four mechanisms, and which one applies
is decided by what kind of thing failed.

## 1. NULL, from a take that will not fit

```c
uint8_t *p = mmgr_confin_persist_capio(&c, 256, 8);
if (p == NULL) { /* the region is full */ }
```

That is the allocator's only failure mode. It is not "out of memory" arriving from elsewhere in the
program — it is this region, which you sized, being full. `mmgr_confin_octas_praesto` tells you by
how much you missed.

## 2. mmgr_bool, from a predicate

```c
if (scrut.has_zero(w)) { }
if (clarus.owns(p))    { }
```

`MMGR_TRUE` and `MMGR_FALSE` are `((mmgr_bool)1)` and `((mmgr_bool)0)`. A predicate answers a
question; it does not report an error.

## 3. A latching flag, for a run of appends

`mmgr_verba` builds a string across many calls, and any one of them can run out of room. It carries
a flag that latches: once set it stays set, and no later successful append clears it.

```c
verba.put(&b, "id=");
verba.u32(&b, id);
verba.put(&b, " name=");
verba.put_clip(&b, name, 32);
verba.ch(&b, '\n');

if (!verba.finish(&b)) {
    /* one check, covering all five */
}
```

Checking after every call would be five branches that are almost never taken, and the fifth would
eventually be the one somebody forgot. Latching moves the check to one place and makes forgetting it
the only mistake available — which is a mistake a reviewer can actually see.

This applies to `verba` because what it is appending is not a length the caller had. A formatted
number is as long as it turns out to be. Where the length **is** known at the call — a four byte
field, a run of `n` bytes — the check is a contract instead, and mechanism 4 covers it.

## 4. MMGR_ASSERT, for a violated contract

A contract violation is not a runtime error, it is a bug. Passing a null where one is not allowed, a
length that cannot be right, or a write that does not fit a buffer whose size you have in front of
you. Those are policed by `MMGR_ASSERT`, which **compiles to nothing by default**:

```c
#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
#endif
```

The `sizeof` keeps the expression type-checked so it cannot rot, and then discards it. A shipping
build pays nothing.

This is why a span has no `overflow` field and a reader has no `err`. `mem` is `uint8_t[8]` and the
field is four bytes, both at the point the call is written — so a write that does not fit is a
program that should not have been built, and there is no runtime state to carry the answer:

```c
MMGR_ASSERT(w->pos < w->cap, "byte written past the span");
w->buf[w->pos] = b;
```

To make the asserts real, define `MMGR_ASSERT` to something that aborts and set
`MMGR_DEBUG_CHECKS=1`. That combination is the `checks` environment, and it is a genuine gate in CI
rather than a developer convenience — a violated precondition fails a test instead of being a no-op
nobody notices.

```sh
ctest --test-dir build -R '_checks$' --output-on-failure
```

Two bounds stay runtime checks rather than contracts, because their input is wire data and not a
promise the caller made: `byteio.rd_str`'s length prefix, and `byteio.mpint_fixed`'s `mlen`. Both come
off the wire, so whether they fit is a fact about what arrived.

## What is deliberately absent

**No errno, no error enum, no last-error.** Threading a status through every entry would double the
width of the API to report a condition that is either "the region is full" or "you have a bug", and
a global one is not usable from more than one caller.

**No exceptions and no longjmp.** This is C11 targeting devices with no unwinder.

**No allocation failure callbacks.** There is nowhere for a handler to get memory from, so the only
honest answer is `NULL`.

## Summary

| you are asking                     | it answers with                          |
| ---------------------------------- | ---------------------------------------- |
| can I have some storage            | a pointer, or `NULL`                     |
| is this true                       | `mmgr_bool`                              |
| did any of that formatting fail    | a latched flag, checked once at the end  |
| did what arrived off the wire fit  | `mmgr_bool`, at the call                 |
| did I violate a precondition       | nothing, unless you built with checks on |
