# Byteio: bytes on a wire {#mod_byteio_guide}

Byte and wire serialization over spans.

## When to reach for it

When you build or parse a binary frame where the layout does not change and the byte order is part
of the spec.

Six entries over @ref mod_spat_guide's two span types. Three append into a span that a writer fills: `put`
for one byte, `put_be` for a value big end first, `raw` for a run as it is. Three read out of one:
`take_be` for a value, `rd_str` for a length-prefixed run, `mpint_fixed` for an integer
right-aligned into a fixed field.

```c
mmgr_span w = EMBED_CALL(spat.from, SpatiumCfg, .buf = frame, .cap = sizeof frame);

EMBED_CALL(byteio.put, OctetusCfg, .write_span = &w, .byte = 0x01u);
EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = 0x1234u, .bytes = 2u);
EMBED_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = payload, .bytes = n);

mmgr_cspan r = EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = frame, .cap = got);
uint64_t v = 0;

if (!EMBED_CALL(byteio.take_be, OctetusCfg, .read_span = &r, .out = &v, .bytes = 2u)) {
    /* the frame was short */
}
```

Fields sit `bytes` apart, and the span carries the cursor, and so nothing here needs an offset passed in.

## Gotchas

**The appends answer nothing and the takes all answer.** This is the complete failure model. An append
that does not fit is a build failure. The build fixes both what a writer emits and how big its
buffer is. So the append asserts, stores nothing, and latches the span's `overflow` to retain a
wrong program off the end.

A short read is a runtime fact about whatever sent the bytes. So it sets `err`, leaves the cursor
alone and returns `EMBED_FALSE` for a caller to act on. See @ref ref_error_handling.

**A value moves a word at a time, not a byte at a time.** `put_be` reverses once through
@ref mod_endian_guide and then stores at the widest step the count allows. Eight bytes is one store,
seven is three, and only an odd final byte ever goes out alone. `take_be` is the mirror.

**Neither access needs alignment.** They reach @ref mod_proxim_guide's unaligned entries. A
field may start anywhere in the span. The cost does vary with the field width, because the number of
stores does.

**`rd_str` copies nothing.** It points `blob` into the span's own bytes, and so what it passes back lives
exactly as long as the buffer does. A length that the take reads, with no payload after it, puts
the cursor back where it started: a partial read is not a read.

**`mpint_fixed` writes the field whole and does not append to it.** It zero-fills ahead of the
value and leaves the cursor at `cap`. It skips the leading zero bytes of the integer before it tests
the width. A value that carries a sign byte still fits a field of its own size.

@ref mod_byteio "Generated reference"

---

# Bitio: the bit writer {#mod_bitio_guide}

Bit-level output for formats without byte alignment.

```c
mmgr_bitor w = EMBED_CALL(bitio.init, BitorumCfg, .out = buf, .cap = sizeof buf);

EMBED_CALL(bitio.put, BitorumCfg, .writer = &w, .val = 0x5u,  .bit_count = 3u);
EMBED_CALL(bitio.put, BitorumCfg, .writer = &w, .val = value, .bit_count = 12u);

EMBED_CALL(bitio.align, BitorumCfg, .writer = &w);   /* 15 bits written; without this, 8 */
```

The writer is yours to hold and the cfg points at it. `init` fills it in; every `put` reads and
updates it through `writer`.

**`align` is how a stream ends, and without it the stream loses the tail.** `put` writes whole
bytes only. Bits that do not fill one stay in `residue`, and nothing stores them on their own. Any
stream whose length is not a multiple of eight needs `align` before anything reads its buffer. It
pads with zeros above the bits it holds, and does nothing when the residue is empty. So a call on a
stream that happened to end on a byte, or a second call, costs nothing.

**Bits pack from the low end.** A 3-bit put of `0b101` then a 5-bit put of zero produces `0x05`, not
`0xA0`.

**The writer holds a partial byte.** `bytes_written` counts whole bytes written; anything short of a byte sits
in `residue` and goes out when a later put completes it. A byte-level write to the same buffer in
between lands in the wrong place.

**`overflow` latches.** A put whose completed bytes would run past `cap` writes nothing, sets the
flag and clears the residue. Nothing clears the flag again, and so one check after a run of puts
covers the whole run. As with a span, a latched flag means the buffer has the wrong size for a
stream whose length the build knew.

**This is a writer only.** There is no bit reader; a format that needs one reads bytes through
@ref mod_byteio_guide and shifts them itself.

@ref mod_bitio "Generated reference"

---

# Endian: stated byte order {#mod_endian_guide}

Explicit reads and writes. Two namespaces over three entries each, and the namespace is the byte
order:

```c
EMBED_CALL(parva_extremitas.wr, EndianCfg, .dst = p, .val = v, .width = MMGR_ENDIAN_32);
EMBED_CALL(magna_extremitas.wr, EndianCfg, .dst = p, .val = v, .width = MMGR_ENDIAN_32);

const uint64_t v16 = EMBED_CALL(magna_extremitas.rd, EndianCfg, .src = p, .width = MMGR_ENDIAN_16);
```

`parva_extremitas` is little end first, `magna_extremitas` is big. The width is an argument and not
part of the entry name. The values of `mmgr_endian_width` are the byte counts, and so the same
number advances your cursor.

**There is no `host` variant, deliberately.** A wire format has a byte order. The host's order is an
implementation detail of the host. Code that writes "native" order to a wire has a bug that appears
the first time the other end is a different machine.

`EMBED_BIG_ENDIAN` exists so the library can take the cheap path when the requested order happens to
match the host. It is not there for a caller to ask for whatever the machine does.

@ref mod_endian "Generated reference" · @ref concept_width

---

# Fractio: IEEE-754 fields {#mod_fract_guide}

Access to the parts of a `double` without `<math.h>` and without a heap.

```c
const embed_u64 bits = EMBED_CALL(fract.to_bits, FractioCfg, .val  = x);
const embed_u64 sign = EMBED_CALL(fract.sign,    FractioCfg, .bits = bits);
const embed_u64 exp  = EMBED_CALL(fract.exp,     FractioCfg, .bits = bits);
const embed_u64 mant = EMBED_CALL(fract.mant,    FractioCfg, .bits = bits);

const embed_u64 back = EMBED_CALL(fract.merge,     FractioCfg, .sign = sign, .exp = exp, .mant = mant);
const double   y    = EMBED_CALL(fract.from_bits, FractioCfg, .bits = back);
```

`to_bits` and `from_bits` are the two directions of the same union, and `merge` returns a bit
pattern, not a `double`: put it through `from_bits` to get the value back.

The library also exposes the `MMGR_DBL_*` constants: masks, shifts, the bias. A caller who does
something the entries do not cover need not re-derive them.

## When to reach for it

Format a float without `printf`, as @ref mod_verba_guide's `g` and `fixed` entries do. Classify a
value as infinite or NaN without a library call. Anything that needs the fields and not the number.

## Gotchas

**This is `double`, not `float`.** There are no single-precision entries.

**It is bit manipulation and not arithmetic.** `merge` will happily build a NaN or a denormal from the
fields you hand it. It masks each one to its width and does not reject it. A value too large
for its field loses the high bits silently.

@ref mod_fract "Generated reference"
