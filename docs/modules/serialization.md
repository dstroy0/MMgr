# Byteio — bytes on a wire {#mod_byteio_guide}

Byte and wire serialization over spans.

## When to reach for it

Building or parsing a binary frame where the layout is fixed and the byte order is part of the spec.

Six entries over @ref mod_spat_guide's two span types. Three append into a span being filled — `put`
for one byte, `put_be` for a value big end first, `raw` for a run as it stands. Three read out of one
— `take_be` for a value, `rd_str` for a length-prefixed run, `mpint_fixed` for an integer
right-aligned into a fixed field.

```c
mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = frame, .cap = sizeof frame);

MMGR_CALL(byteio.put, OctetusCfg, .w = &w, .byte = 0x01u);
MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = 0x1234u, .bytes = 2u);
MMGR_CALL(byteio.raw, OctetusCfg, .w = &w, .src = payload, .bytes = n);

mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = frame, .cap = got);
uint64_t v = 0;

if (!MMGR_CALL(byteio.take_be, OctetusCfg, .r = &r, .out = &v, .bytes = 2u)) {
    /* the frame was short */
}
```

Fields sit `bytes` apart, and the span carries the cursor, so nothing here needs an offset passed in.

## Gotchas

**The appends answer nothing and the takes all answer.** That is the whole failure model. An append
that does not fit is a build failure — what a writer emits and how big its buffer is are both fixed
before the build — so it asserts, stores nothing, and latches the span's `overflow` to keep a wrong
program off the end. A short read is a runtime fact about whatever sent the bytes, so it sets `err`,
leaves the cursor alone and returns `MMGR_FALSE` for a caller to act on. See @ref ref_error_handling.

**A value moves a word at a time, not a byte at a time.** `put_be` reverses once through
@ref mod_endian_guide and then stores at the widest step the count allows: eight bytes is one store,
seven is three, and only an odd final byte is ever written alone. `take_be` is the mirror.

**Neither access needs to be aligned.** They reach @ref mod_proxim_guide's unaligned entries, so a
field may start anywhere in the span. The cost does vary with the field width, because the number of
stores does.

**`rd_str` copies nothing.** It points `blob` into the span's own bytes, so what it hands back lives
exactly as long as the buffer does. A length that is read but not followed by its payload puts the
cursor back where it started — a partial read is not a read.

**`mpint_fixed` writes the field whole rather than appending to it.** It zero-fills ahead of the
value and leaves the cursor at `cap`. Leading zero bytes of the integer are skipped before the width
is tested, so a value carrying a sign byte still fits a field of its own size.

@ref mod_byteio "Generated reference"

---

# Bitio — the bit writer {#mod_bitio_guide}

Bit-level output for formats that are not byte-aligned.

```c
mmgr_bitor w = MMGR_CALL(bitio.init, BitorumCfg, .out = buf, .cap = sizeof buf);

MMGR_CALL(bitio.put, BitorumCfg, .writer = &w, .val = 0x5u,  .nbits = 3u);
MMGR_CALL(bitio.put, BitorumCfg, .writer = &w, .val = value, .nbits = 12u);

MMGR_CALL(bitio.align, BitorumCfg, .writer = &w);   /* 15 bits written; without this, 8 */
```

The writer is yours to hold and the cfg points at it. `init` fills it in; every `put` reads and
updates it through `writer`.

**`align` is how a stream ends, and leaving it out loses the tail.** `put` writes whole bytes only,
so bits that do not fill one stay in `residue` and are never stored on their own. Any stream whose
length is not a multiple of eight needs `align` before its buffer is read. It pads with zeros above
the bits it holds, and does nothing when the residue is empty — so calling it on a stream that
happened to end on a byte, or calling it twice, costs nothing.

**Bits pack from the low end.** A 3-bit put of `0b101` then a 5-bit put of zero produces `0x05`, not
`0xA0`.

**The writer holds a partial byte.** `cnt` counts whole bytes written; anything short of a byte sits
in `residue` and goes out when a later put completes it. A byte-level write to the same buffer in
between lands in the wrong place.

**`overflow` latches.** A put whose completed bytes would run past `cap` writes nothing, sets the
flag and clears the residue, and nothing clears it again — so one check after a run of puts covers
the whole run. As with a span, reaching it means the buffer was sized wrong for a stream whose
length was known before the build.

**This is a writer only.** There is no bit reader; a format that needs one reads bytes through
@ref mod_byteio_guide and shifts them itself.

@ref mod_bitio "Generated reference"

---

# Endian — stated byte order {#mod_endian_guide}

Explicit reads and writes. Two namespaces over three entries each, and the namespace is the byte
order:

```c
MMGR_CALL(parva_extremitas.wr, EndianCfg, .dst = p, .val = v, .width = MMGR_ENDIAN_32);
MMGR_CALL(magna_extremitas.wr, EndianCfg, .dst = p, .val = v, .width = MMGR_ENDIAN_32);

const uint64_t v16 = MMGR_CALL(magna_extremitas.rd, EndianCfg, .src = p, .width = MMGR_ENDIAN_16);
```

`parva_extremitas` is little end first, `magna_extremitas` is big. The width is an argument rather
than part of the entry name, and `mmgr_endian_width`'s values are the byte counts, so the same
number advances your cursor.

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
const mmgr_u64 bits = MMGR_CALL(fract.to_bits, FractioCfg, .val  = x);
const mmgr_u64 sign = MMGR_CALL(fract.sign,    FractioCfg, .bits = bits);
const mmgr_u64 exp  = MMGR_CALL(fract.exp,     FractioCfg, .bits = bits);
const mmgr_u64 mant = MMGR_CALL(fract.mant,    FractioCfg, .bits = bits);

const mmgr_u64 back = MMGR_CALL(fract.merge,     FractioCfg, .sign = sign, .exp = exp, .mant = mant);
const double   y    = MMGR_CALL(fract.from_bits, FractioCfg, .bits = back);
```

`to_bits` and `from_bits` are the two directions of the same union, and `merge` returns a bit
pattern, not a `double` — put it through `from_bits` to get the value back.

The `MMGR_DBL_*` constants — masks, shifts, the bias — are exposed too, so a caller doing something
the entries do not cover is not forced to re-derive them.

## When to reach for it

Formatting a float without `printf`, which is what @ref mod_verba_guide's `g` and `fixed` entries do.
Classifying a value as infinite or NaN without a library call. Anything that needs the fields rather
than the number.

## Gotchas

**This is `double`, not `float`.** There are no single-precision entries.

**It is bit manipulation, not arithmetic.** `merge` will happily build a NaN or a denormal from the
fields you hand it. It masks each one to its width rather than rejecting it, so a value too large
for its field loses the high bits silently.

@ref mod_fract "Generated reference"
