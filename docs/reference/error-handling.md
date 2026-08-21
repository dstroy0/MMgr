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
if (spat.ok(s)) { }
if (scrut.has_zero(w)) { }
```

`MMGR_TRUE` and `MMGR_FALSE` are `((mmgr_bool)1)` and `((mmgr_bool)0)`. A predicate answers a
question; it does not report an error.

## 3. A latching flag, for a run of operations

This is the one worth understanding, because it is the one that looks wrong at first.

| type         | flag                        | set when                |
| ------------ | --------------------------- | ----------------------- |
| `mmgr_spat`  | `overflow`                  | a write ran out of room |
| `mmgr_fspat` | `err`                       | a read ran past the end |
| `mmgr_verba` | checked by `verba.finish()` | any append failed       |

The flag **latches**. Once set it stays set, and no later successful operation clears it.

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

The operations after an overflow are safe. They write nothing and advance nothing; they are no-ops
against a span that is already full. That is what makes deferring the check correct rather than
merely convenient.

## 4. MMGR_ASSERT, for a violated contract

A contract violation is not a runtime error, it is a bug — passing a null where one is not allowed,
or a length that cannot be right. Those are policed by `MMGR_ASSERT`, which **compiles to nothing by
default**:

```c
#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
#endif
```

The `sizeof` keeps the expression type-checked so it cannot rot, and then discards it. A shipping
build pays nothing.

To make them real, define `MMGR_ASSERT` to something that aborts and set `MMGR_DEBUG_CHECKS=1`. That
combination is the `checks` environment, and it is a genuine gate in CI rather than a developer
convenience — a violated precondition fails a test instead of being a no-op nobody notices.

```sh
ctest --test-dir build -R '_checks$' --output-on-failure
```

## What is deliberately absent

**No errno, no error enum, no last-error.** A global error loculus is not usable from more than one
worker, and threading a status through every entry would double the width of the API to report a
condition that is either "the region is full" or "you have a bug".

**No exceptions and no longjmp.** This is C11 targeting devices with no unwinder.

**No allocation failure callbacks.** There is nowhere for a handler to get memory from, so the only
honest answer is `NULL`.

## Summary

| you are asking               | it answers with                          |
| ---------------------------- | ---------------------------------------- |
| can I have some storage      | a pointer, or `NULL`                     |
| is this true                 | `mmgr_bool`                              |
| did any of that fail         | a latched flag, checked once at the end  |
| did I violate a precondition | nothing, unless you built with checks on |
