# Byteio — bytes on a wire {#mod_byteio_guide}

Byte and wire serialization over spans.

## When to reach for it

Building or parsing a binary frame where the layout is fixed and the byte order is part of the spec.

```c
mmgr_spat s = spat.from(buf, sizeof buf);

byteio.put(&s, 0x01);                   /* one byte              */
byteio.put_be(&s, 0x1234, 2);           /* big-endian, 2 bytes   */
byteio.raw(&s, payload, payload_len);   /* opaque bytes          */

size_t at = 0u;
uint32_t v = 0;
byteio.rd_u32(frame, frame_len, &at, &v);
```

`mpint_fixed` writes an SSH-style multi-precision integer into a fixed width, which is the one entry
here that encodes a format rather than a primitive.

## Gotchas

**Nothing here checks whether the bytes fit.** The caller has the buffer and the field width in
front of it, so a field that runs past the end is a contract violation - nothing in a shipping
build, an abort in `checks`. See @ref ref_error_handling.

**`rd_str` and `mpint_fixed` do check, and return `MMGR_FALSE`.** Their lengths arrive off the wire
rather than from the caller, so whether they fit is a fact about the data and not a promise anyone
made.

@ref mod_byteio "Generated reference"

---

# Bitio — the bit writer {#mod_bitio_guide}

Bit-level output for formats that are not byte-aligned.

```c
BitorumCfg w = {.out = buf, .cap = sizeof buf};
mmgr_bitor_put(w, 0b101, 3);   /* three bits  */
mmgr_bitor_put(w, value, 12);  /* twelve bits */
```

The writer is the config. There is nothing to open it with: the caller has the buffer and its size,
which is everything the first put needs, and it carries the same object from one put to the next.

**The writer holds a partial byte.** `cnt` counts whole bytes; anything left over sits in `acc` and
is written when a later put completes it. A byte-level write to the same span in between lands in
the wrong place, and a span finished on a fragment ends `cnt` bytes long with the fragment still in
hand.

**`overflow` latches.** A put that would run past `cap` writes nothing and sets it, and it is never
cleared, so one check after a run of puts covers the whole run rather than a test after each.

@ref mod_bitio "Generated reference"

---

# Endian — stated byte order {#mod_endian_guide}

Explicit reads and writes. Twelve entries, and the naming is the documentation:

```c
endian.wr16le  endian.wr16be    endian.rd16le  endian.rd16be
endian.wr32le  endian.wr32be    endian.rd32le  endian.rd32be
endian.wr64le  endian.wr64be    endian.rd64le  endian.rd64be
```

**There is no `host` variant, deliberately.** A wire format has a byte order. The host's order is an
implementation detail of the host, and code that writes "native" order to a wire has a bug that
appears the first time the other end is a different machine.

`MMGR_HW_BIG_ENDIAN` exists so the library can take the cheap path when the requested order happens
to match the host — not so a caller can ask for whatever the machine does.

@ref mod_endian "Generated reference" · @ref concept_width

---

# Fractio — IEEE-754 fields {#mod_fract_guide}

Getting at the parts of a `double` without `<math.h>` and without a heap.

```c
uint64_t bits = fract.from_bits(x);
int      sign = fract.sign(bits);
int      exp  = fract.exp(bits);
uint64_t mant = fract.mant(bits);

double   y    = fract.merge(sign, exp, mant);
```

The `MMGR_DBL_*` constants — masks, shifts, the bias — are exposed too, so a caller doing something
the entries do not cover is not forced to re-derive them.

## When to reach for it

Formatting a float without `printf`, which is what @ref mod_verba_guide's `g` and `fixed` entries do.
Classifying a value as infinite or NaN without a library call. Anything that needs the fields rather
than the number.

## Gotchas

**This is `double`, not `float`.** There are no single-precision entries.

**It is bit manipulation, not arithmetic.** `merge` will happily build a NaN or a denormal if you
hand it those fields. It does not validate.

@ref mod_fract "Generated reference"
