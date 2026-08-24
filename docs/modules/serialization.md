# Byteio — bytes on a wire {#mod_byteio_guide}

Byte and wire serialization over spans.

## When to reach for it

Building or parsing a binary frame where the layout is fixed and the byte order is part of the spec.

Two entries. Both move a value big end first, and both work in eight-byte slots.

```c
MMGR_CALL(byteio.put, OctetusCfg, .at = frame,      .val = 0x01u,   .bytes = 1u);
MMGR_CALL(byteio.put, OctetusCfg, .at = frame + 8,  .val = 0x1234u, .bytes = 2u);

uint64_t v = 0;
MMGR_CALL(byteio.take, OctetusCfg, .from = frame, .out = &v, .bytes = 1u);
```

## Gotchas

**A slot is always eight bytes, whatever `bytes` says.** `put` stores a whole 64-bit word and clears
the trailing bytes; `take` loads a whole one. So the destination needs eight bytes available even
for a one-byte field, and consecutive fields sit eight bytes apart, not `bytes` apart.

**Both accesses are aligned.** `at` and `from` must be 64-bit aligned. That, and the single store,
are why the cost does not vary with the field width — measured at about 5 cycles for every size.

**Nothing here checks whether the bytes fit.** The caller has the buffer and the field width in
front of it, so a field that runs past the end is a contract violation - nothing in a shipping
build, an abort in `checks`. See @ref ref_error_handling.

**Strings and multi-precision integers are not here.** `rd_str` and `mpint_fixed` belong to
@ref mod_cellul_guide, because their lengths arrive off the wire and have to be checked.

@ref mod_byteio "Generated reference"

---

# Bitio — the bit writer {#mod_bitio_guide}

Bit-level output for formats that are not byte-aligned.

```c
mmgr_bitor w = MMGR_CALL(bitio.init, BitorumCfg, .out = buf, .cap = sizeof buf);

MMGR_CALL(bitio.put, BitorumCfg, .writer = &w, .val = 0x5u,  .nbits = 3u);
MMGR_CALL(bitio.put, BitorumCfg, .writer = &w, .val = value, .nbits = 12u);
```

The writer is yours to hold and the cfg points at it. `init` fills it in; every `put` reads and
updates it through `writer`.

**Bits pack from the low end.** A 3-bit put of `0b101` then a 5-bit put of zero produces `0x05`, not
`0xA0`.

**The writer holds a partial byte.** `cnt` counts whole bytes written; anything short of a byte sits
in `residue` and goes out when a later put completes it. A byte-level write to the same buffer in
between lands in the wrong place, and a writer left on a fragment has `nbits` bits still in hand
that were never stored.

**`overflow` latches.** A put whose completed bytes would run past `cap` writes nothing, sets the
flag and clears the residue, and nothing clears it again — so one check after a run of puts covers
the whole run.

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
