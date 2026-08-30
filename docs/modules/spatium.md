# Spatium — spans {#mod_spat_guide}

A bounded view over memory the span does not own, with a cursor and a flag that latches.

## When to reach for it

- You are passing a buffer and a length to something and want them to travel together.
- You are filling a fixed buffer through several steps and want to test once at the end.
- You want the compiler to stop a buffer you may write from being handed where a read-only one belongs.

## What it composes with

`spat.from` borrows whatever @ref mod_confin_guide handed you. @ref mod_byteio_guide writes into that
storage and reads back out of it; @ref mod_verba_guide and @ref mod_cellul_guide work against the
same bytes.

## Two types, on purpose

```c
typedef struct {
    uint8_t  *buf;
    size_t    cap;
    size_t    pos;
    mmgr_bool overflow;
} mmgr_span;

typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         pos;
    mmgr_bool      err;
} mmgr_cspan;
```

|          | `mmgr_span` | `mmgr_cspan` |
| -------- | ----------- | ------------ |
| for      | filling     | reading      |
| `buf`    | writable    | `const`      |
| extent   | `cap`       | `len`        |
| the flag | `overflow`  | `err`        |

The extent is named differently in each on purpose. `const` alone would stop the write but let the
two be confused wherever a name is read, so the field names differ too, and a mix-up is a compile
error rather than a puzzle.

## The two flags are not the same kind of thing

**`overflow` is a build failure.** What a writer emits and how big its buffer is are both settled
before the build, so the two are either compatible or the program is wrong. There is no runtime
condition under which a correct writer overruns a correctly sized span. `MMGR_ASSERT` says so —
nothing in a shipping build, an abort in `checks`. See @ref ref_error_handling.

The flag is what a shipping build does with a wrong program, not a path to design for. It latches so
that the first bad append stores nothing and every append after it is a no-op, which keeps a wrong
writer off the end of the buffer instead of walking down it. Read it to learn that something is
broken, not to decide what to do next.

**`err` is a runtime fact.** A read span runs out because whatever sent the bytes sent fewer, and
nothing was built wrong. That is why every take answers `mmgr_bool` and no append returns anything:
a short read is a case to handle, an overrun append is a bug to fix.

Either way the flag is sticky, so a caller walks a span through several steps and tests once at the
end rather than after each:

```c
uint8_t buf[64];
mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

MMGR_CALL(byteio.raw, OctetusCfg, .w = &w, .src = head, .bytes = sizeof head);
MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = id, .bytes = 4u);
MMGR_CALL(byteio.raw, OctetusCfg, .w = &w, .src = body, .bytes = n);

/* One test, covering all three */
MMGR_ASSERT(MMGR_CALL(spat.ok, SpatiumCfg, .span = w), "frame does not fit buf");
```

A take that reaches past the end leaves the cursor where it was, so a caller that keeps reading after
a failure still knows where it is. `spat.reset` is the only call that clears a flag.

## The ten entries

Two build one — `from`, `cfrom`. Two ask whether it is still usable — `ok`, `cok`. One asks whether
it covers any bytes at all — `has_storage`. One rewinds — `reset`. Two narrow — `after` a count of
bytes, and `first` so many. Two turn a filled span back into a readable one — `produced` for
everything written, `read` for a named prefix of it.

There is no `len` and no `room`. The span is the caller's own value, so `s.pos` and `s.cap - s.pos`
are already in hand, and a call to fetch them would be a second way to spell a member read.

## Gotchas

**A span travels inside the argument pack, not as a pointer to one.** `SpatiumCfg` holds an
`mmgr_span` in `.s` and an `mmgr_cspan` in `.cs`, so a walk reads a copy and cannot change the span
you gave it. `reset` is the exception and takes `.at`, a pointer, because it is the one entry that
must change one.

**`from` takes `.buf` and `cfrom` takes `.cbuf`.** They are separate members on purpose: a
`const uint8_t *` cannot reach the fill constructor, so a buffer you may not write cannot be turned
into a span you may write through.

**`.cap` carries the extent for both constructors, and `.n` the count for `after`, `first` and
`read`.** One member per job rather than one per entry.

**`after` and `first` return a new span; they do not move the one you passed.** Assign the result.

**An `off` of exactly `cap` is not a failure.** It gives an empty span that has not failed: nothing
is left, but nothing went wrong either. Past `cap` is the failure.

**`produced` carries the fill span's `overflow` across as the read span's `err`.** Reading back a
span that overran does not launder the failure.

**`pos` keeps counting past `cap` once a span has overrun.** So the number left behind says how far
past the end a wrong writer went, which is worth reading in a post-mortem. It is not a sizing pass:
a length you have to measure by overrunning a buffer at run time is a length you already knew when
you wrote the code, and the assert fires long before you get to read it.

**A span does not know when its target dies.** It holds a pointer the caller gave it. Returning a
span over interim storage from a function that rewinds its own mark is a use-after-free, and no field
in the span can tell you.

**Spans are values.** Copy them, pass them, return them. Nothing in the library holds a pointer to
one.

## Reference

@ref mod_spat "Generated reference for spatium" · @ref concept_ownership for the lifetime rules
