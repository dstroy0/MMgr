# Spatium: spans {#mod_spat_guide}

A bounded view over memory the span does not own, with a cursor and a flag that latches.

## When to reach for it

- You pass a buffer and a length to something and want them to travel together.
- You fill a fixed buffer through several steps and want to test once at the end.
- You want the compiler to refuse a buffer you may write where a read-only one belongs.

## What it composes with

`spat.from` borrows whatever @ref mod_confin_guide passed you. The module @ref mod_byteio_guide
writes into that storage and reads back out of it; @ref mod_verba_guide and @ref mod_cellul_guide work against the
same bytes.

## Two types, deliberately

```c
typedef struct {
    uint8_t  *buf;
    size_t    cap;
    size_t    pos;
    embed_bool overflow;
} mmgr_span;

typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         pos;
    embed_bool      err;
} mmgr_cspan;
```

|          | `mmgr_span` | `mmgr_cspan` |
| -------- | ----------- | ------------ |
| for      | filling     | reading      |
| `buf`    | writable    | `const`      |
| extent   | `cap`       | `len`        |
| the flag | `overflow`  | `err`        |

Each type gives the extent its own name, on purpose. `const` alone would stop the write, but a
reader could still confuse the two wherever a name appears. So the field names differ too, and a
mix-up is a compile error and not a puzzle.

## The two flags are not the same type of thing

**`overflow` is a build failure.** The build settles both what a writer emits and how big its
buffer is. So the two are either compatible, or the program is wrong. There is no runtime condition
under which a correct writer overruns a correctly sized span. `MMGR_ASSERT` says so: nothing in a
build you ship, an abort in `checks`. See @ref ref_error_handling.

The flag is what a shipped build does with a wrong program, not a path to design for. It latches:
the first bad append stores nothing, and every append after it is a no-op. That retains a wrong
writer off the end of the buffer, and the writer does not walk down it. Read it to learn that
something is broken, not to decide what to do next.

**`err` is a runtime fact.** A read span runs out because whatever sent the bytes sent fewer, and
nobody built anything wrong. For that reason every take answers `embed_bool` and no append returns
anything. A short read is a case to handle, and an overrun append is a bug to fix.

Either way the flag is sticky. A caller walks a span through several steps and tests once at the
end and not after each:

```c
uint8_t buf[64];
mmgr_span w = EMBED_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);

EMBED_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = head, .bytes = sizeof head);
EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = id, .bytes = 4u);
EMBED_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = body, .bytes = n);

/* One test, covering all three */
MMGR_ASSERT(EMBED_CALL(spat.ok, SpatiumCfg, .span = w), "frame does not fit buf");
```

A take that reaches past the end leaves the cursor where it was. A caller that reads on after a
failure still knows where it is. `spat.reset` is the only call that clears a flag.

## The ten entries

Two build one: `from`, `cfrom`. Two ask whether it is still usable: `ok`, `cok`. One asks whether
it covers any bytes at all: `has_storage`. One rewinds: `reset`. Two narrow: `after` a count of
bytes, and `first` so many. Two turn a filled span back into a readable one: `produced` for
everything written, `read` for a named prefix of it.

There is no `len` and no `room`. The span is the caller's own value, and so `s.pos` and `s.cap - s.pos`
are already in hand. A call to fetch them would be a second way to write a member read.

## Gotchas

**A span travels inside the argument pack, not as a pointer to one.** `SpatiumCfg` holds an
`mmgr_span` in `.span` and an `mmgr_cspan` in `.cspan`. A walk reads a copy and cannot change the
span you gave it. `reset` is the exception and takes `.at`, a pointer, because it is the single one entry
that must change one.

**`from` takes `.buf` and `cfrom` takes `.cbuf`.** They are separate members deliberately: a
`const uint8_t *` cannot reach the fill constructor. A buffer you may not write cannot turn
into a span you may write through.

**`.cap` carries the extent for both constructors, and `.count` the byte count for `after`, `first`
and `read`.** One member per job and not one per entry.

**`after` and `first` return a new span; they do not move the one you passed.** Assign the result.

**A `.count` of exactly `cap` is not a failure.** It gives an empty span that has not failed: no
bytes stay, and nothing went wrong either. Past `cap` is the failure.

**`produced` carries the fill span's `overflow` across as the read span's `err`.** A read back of
a span that overran does not launder the failure.

**`pos` counts on past `cap` once a span overruns.** So the number left behind says how far past
the end a wrong writer went, and a post-mortem can read it. It is not a way to measure size. A
length you must measure with a run-time overrun of a buffer is a length you already knew when you
wrote the code. The assert fires long before you get to read it.

**A span does not know when its target dies.** It holds a pointer the caller gave it. A function
that gives the cell back and returns a span over temporary-tier storage commits a use-after-free.
No field in the span can tell you.

**Spans are values.** Copy them, pass them, return them. Nothing in the library holds a pointer to
one.

## Reference

@ref mod_spat "Generated reference for spatium" · @ref concept_ownership for the lifetime rules
