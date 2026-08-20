# Spatium — spans and slices {#mod_spat_guide}

Bounded views over memory the span does not own.

## When to reach for it

- You are passing a buffer and a length to something and want them to travel together.
- You are writing into a fixed buffer and want the overflow check in one place, not per call.
- You are parsing and want a cursor that cannot walk off the end.

## What it composes with

`spat.from` borrows whatever @ref mod_confin_guide or a pool handed you. @ref mod_verba_guide and
@ref mod_byteio_guide write **into** spans; @ref mod_cellul_guide reads out of them.

## The two types

|           | `mmgr_spat`   | `mmgr_fspat`                               |
| --------- | ------------- | ------------------------------------------ |
| direction | writes        | reads                                      |
| tracks    | `pos` written | `pos` consumed                             |
| flag      | `overflow`    | `err`                                      |
| made by   | `spat.from`   | `spat.cfrom`, `spat.produced`, `spat.read` |

`f` is _fixus_ — fixed, read-only.

## Worked example

```c
uint8_t buf[64];
mmgr_spat s = spat.from(buf, sizeof buf);

/* Carve a sub-view without copying. */
mmgr_spat body = spat.after(s, 8);        /* skip an 8-byte header */
mmgr_spat head = spat.first(s, 8);        /* just the header       */

/* What was actually written, as something readable. */
mmgr_fspat done = spat.produced(s);
if (spat.cok(done)) {
    process(done);
}
```

`after` and `first` return new spans over the same storage. Nothing is copied and nothing is
allocated.

## Gotchas

**The flag latches.** Once `overflow` is set it stays set, and later writes are safe no-ops. That is
what makes checking once at the end correct rather than merely convenient. See
@ref ref_error_handling.

**A span does not know when its target dies.** `spat.has_storage` asks whether it was given a buffer
at all, not whether that buffer is still alive — which is not a question a span can answer. Returning
a span over interim storage from a function that rewinds its own mark is a use-after-free.

**`len` and `room` are not the same question.** `len` is how much has been written; `room` is how much
is left. Neither is the capacity.

**Spans are values.** Copy them, pass them, return them — but `reset` takes a pointer, because it
mutates.

## Reference

@ref mod_spat "Generated reference for spatium"
