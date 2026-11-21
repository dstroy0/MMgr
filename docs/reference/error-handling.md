# How failure is reported {#ref_error_handling}

There are no error codes anywhere in this library. There are four mechanisms, and what type of thing
failed decides which one applies.

## 1. NULL, from a take that will not fit

```c
void *const cell = prison.work.persistent_buf_alloc(256u, reader);
if (cell == NULL) { }
```

A check after every call would be five branches that almost never fire, and the fifth would
eventually be the one somebody forgot. A latch moves the check to one place. Then the only mistake
available is to forget that check, and a reviewer can actually see that mistake.

This applies to `verba` because what it appends is not a length the caller had. A formatted number
is as long as it turns out to be. Sometimes the caller **knows** the length at the call: a four byte
field, a run of `n` bytes. There the build settles the answer, and mechanism 4 covers it.

## 4. MMGR_ASSERT, for a broken precondition

A broken precondition is not a runtime error, it is a bug. Examples are a null where the entry
allows none and a length that cannot be right. Another is a write that does not fit a buffer whose
size you have in front of you. `MMGR_ASSERT` polices those, and it **compiles to nothing by
default**:

```c
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
```

The `sizeof` retains the expression type-checked so it cannot rot, and then discards it. A build you
ship pays nothing.

Set `MMGR_DEBUG_CHECKS=1`, and `test/support/mmgr_host_traps.h` supplies the other form instead.
That form writes a report to `stderr` that names the expectation, the file and the line, then calls
`abort()`. They need nothing else to arm.
That is the `checks` environment, and it is a gate in CI: a broken precondition fails a test and is
not a no-op nobody notices.

```sh
ctest --test-dir build -R '_checks$' --output-on-failure
```

Define `MMGR_ASSERT` yourself before you include the header, and the library uses neither form. A target with no
`stderr` and no `abort()` defines its own.

### Where a span's flags fit

A span does carry an `overflow` and a read span an `err`, and the two do not mean the same type of
thing.

`overflow` belongs to mechanism 4. The build settles both what a writer emits and how big its
buffer is. A correct writer cannot overrun a correctly sized span: the append asserts. The flag
settles what a **shipped** build does with a wrong program: the first bad append stores nothing and
latches. Every later append is a no-op, and so a writer built wrong stays off the end of the buffer
and never walks down it. Read it to find out something is broken, never to decide what to do next.

`err` is a runtime answer. A read span runs out because whatever sent the bytes sent fewer, and
nobody built anything wrong. For that reason every take returns `embed_bool` and no append returns
anything. A short read is a case to handle, and an overrun append is a bug to fix.

The same line divides two bounds inside `byteio` that stay runtime checks: `rd_str`'s length prefix
and `mpint_fixed`'s value length. Both come off the wire. Whether they fit is a fact about what
arrived, never a promise the caller made.

## What is deliberately absent

**No errno, no error enum, no last-error.** A status through every entry would double the width of
the API. It would report a condition that is either "the region is full" or "you have a bug". A
global one is not usable from more than one caller.

**No exceptions and no longjmp.** This is C11 for devices with no unwinder.

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
