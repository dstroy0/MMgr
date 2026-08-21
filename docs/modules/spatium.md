# Spatium — spans {#mod_spat_guide}

A bounded view over memory the span does not own.

## When to reach for it

- You are passing a buffer and a length to something and want them to travel together.
- You are writing into a fixed buffer and want a cursor that carries where it has reached.

## What it composes with

`spat.from` borrows whatever @ref mod_confin_guide or a pool handed you. @ref mod_verba_guide and
@ref mod_byteio_guide write **into** spans; @ref mod_cellul_guide reads out of them.

## The type

```c
typedef struct {
    uint8_t *buf;
    size_t   cap;
    size_t   pos;
} mmgr_spat;
```

`buf` and `cap` are what the caller handed over. `pos` is how far the writing has reached. `cap` is
read by the contract checks and by nothing else.

## The module

One entry. `spat.from(p, cap)` normalises a buffer into a span.

Everything a caller wants to know about a span is a field of it, read where it is wanted.
@ref mod_byteio_guide, the only module in the library that writes through one, does exactly that -
it reads `w->pos` and `w->cap` at the point it needs them, because a call to fetch a subtraction the
compare needs anyway is not worth making. There were once eleven accessors here — `ok`, `len`,
`room`, `has_storage`, `after`, `first`, `produced`, `read`, `reset`, and a read-only twin type —
and none of them had a caller inside the library.

## Worked example

```c
uint8_t buf[64];
mmgr_spat s = spat.from(buf, sizeof buf);

byteio.put_be(&s, 0x11223344u, 4);
byteio.raw(&s, payload, sizeof payload);

/* How much was written, and what is left. Both are fields. */
const size_t written = s.pos;
const size_t left    = s.cap - s.pos;
```

## Gotchas

**A write past the end is a build failure, not a flag.** The caller has the buffer and the field
width in front of it when it writes the call, so the two are either compatible or the program is
wrong. `MMGR_ASSERT` says so: nothing in a shipping build, an abort in the `checks` build. There is
no `overflow` field to check afterwards and no sizing pass that counts without writing — a length
you have to measure at run time is a length you already knew. See @ref ref_error_handling.

**A span does not know when its target dies.** It holds a pointer the caller gave it. Returning a
span over interim storage from a function that rewinds its own mark is a use-after-free, and no
field in the span can tell you.

**Spans are values.** Copy them, pass them, return them. The entries that write take a pointer,
because they move `pos`.

## Reference

@ref mod_spat "Generated reference for spatium"
