# How failure is reported {#ref_error_handling}

There are no error codes anywhere in this library. There are four mechanisms, and which one applies
is decided by what kind of thing failed.

## 1. NULL, from a take that will not fit

```c
void *const cell = prison.work.persistent_buf_alloc(256u, &reader);
if (cell == NULL) { }
```

Checking after every call would be five branches that are almost never taken, and the fifth would
eventually be the one somebody forgot. Latching moves the check to one place and makes forgetting it
the only mistake available, which is a mistake a reviewer can actually see.

This applies to `verba` because what it is appending is not a length the caller had. A formatted
number is as long as it turns out to be. Where the length **is** known at the call, a four byte
field, a run of `n` bytes, the answer was settled before the build, and mechanism 4 covers it.

## 4. MMGR_ASSERT, for a broken precondition

A broken precondition is not a runtime error, it is a bug. Passing a null where one is not allowed, a
length that cannot be right, or a write that does not fit a buffer whose size you have in front of
you. Those are policed by `MMGR_ASSERT`, which **compiles to nothing by default**:

```c
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
```

The `sizeof` keeps the expression type-checked so it cannot rot, and then discards it. A shipping
build pays nothing.

Set `MMGR_DEBUG_CHECKS=1` and `test/support/mmgr_host_traps.h` supplies the other form instead: a
report to `stderr` naming the expectation, the file and the line, then `abort()`. Nothing else is
needed to arm them.
That is the `checks` environment, and it is a gate in CI: a broken precondition fails a test instead
of being a no-op nobody notices.

```sh
ctest --test-dir build -R '_checks$' --output-on-failure
```

Define `MMGR_ASSERT` yourself before including the header and neither form is used. A target with no
`stderr` and no `abort()` defines its own.

### Where a span's flags fit

A span does carry an `overflow` and a read span an `err`, and the two do not mean the same kind of
thing.

`overflow` belongs to mechanism 4. What a writer emits and how big its buffer is are both settled
before the build. A correct writer cannot overrun a correctly sized span: the append asserts. The
flag settles what a **shipping** build does with a wrong program: the first bad append stores
nothing, latches, and every append after it is a no-op. A writer that was built wrong is kept off the
end of the buffer and never walks down it. Read it to find out something is broken, never to decide
what to do next.

`err` is a runtime answer. A read span runs out because whatever sent the bytes sent fewer, and
nothing was built wrong. Every take returns `embed_bool` and no append returns anything for that
reason: a short read is a case to handle, an overrun append is a bug to fix.

The same line divides two bounds inside `byteio` that stay runtime checks: `rd_str`'s length prefix
and `mpint_fixed`'s value length. Both come off the wire. Whether they fit is a fact about what
arrived, never a promise the caller made.

## What is deliberately absent

**No errno, no error enum, no last-error.** Threading a status through every entry would double the
width of the API to report a condition that is either "the region is full" or "you have a bug", and
a global one is not usable from more than one caller.

**No exceptions and no longjmp.** This is C11 targeting devices with no unwinder.

**No allocation failure callbacks.** There is nowhere for a handler to get memory from, and the
answer is `NULL`.

## Summary

| you are asking                    | it answers with                          |
| --------------------------------- | ---------------------------------------- |
| can I have some storage           | a pointer, or `NULL`                     |
| is this true                      | `embed_bool`                             |
| did any of that formatting fail   | a latched flag, checked once at the end  |
| did what arrived off the wire fit | `embed_bool`, at the call                |
| did I violate a precondition      | nothing, unless you built with checks on |
