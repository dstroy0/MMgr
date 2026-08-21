# Memoria operor — the mem* family {#mod_memor_guide}

Bulk memory work, a word at a time.

## When to reach for it

Anywhere you would call `memcpy`, `memmove`, `memcmp`, `memchr` or `memset` and want the SWAR
implementation and a bounded contract.

```c
memor.cpy(dst, src, n);
memor.move(dst, src, n);          /* overlapping ranges */
memor.set(p, 0xFF, n);
memor.zero(p, n);
if (memor.cmp(a, b, n) == 0) { }
uint8_t *hit = memor.chr(p, 'x', n);
```

Both spellings exist: `mmgr_memor_cpy` is the same function. See @ref concept_ns_idiom.

## Gotchas

**`cpy` does not handle overlap.** `move` does. Same rule as libc.

**LTO matters here.** `mmgr_memor_chr` over 512 bytes measured 610 cycles with the SWAR entries
out-of-line and 187 when the linker could inline them.

## Reference

@ref mod_memor "Generated reference" · @ref guide_string_shim to redirect libc's names here

---

# Cellularum laboro — bounded string work {#mod_cellul_guide}

Every entry takes a read cap and never runs past it.

## When to reach for it

Any string operation on data that came from outside your program — a wire, a file, a user. The cap
is the point: there is no entry here that scans forward until it happens to find a zero.

## Worked example

```c
size_t n = cellul.len(s, MMGR_STR_MAX);
if (cellul.starts(s, n, "GET ", 4, MMGR_FALSE)) { }

size_t at = cellul.find(hay, hay_len, "Host:", 5, MMGR_TRUE);   /* case-insensitive */

long v = 0;
if (cellul.to_long(digits, dlen, &v)) { }
```

The trailing `mmgr_bool` on the comparison entries is case-insensitivity, not a success flag.

## Gotchas

**Table-only.** `cellul` is declared `extern const CellularumLaboroNs`; there are no free functions
in the header. This is the one module where the dispatch table is the entire public surface.

**The cap is a cap, not a length.** `cellul.len(s, 64)` returns at most 64 whether or not a
terminator was found. If you need to know which happened, compare against the cap.

**`find` anchors on the rarest byte of the needle**, using @ref mod_anchor_guide. That is why it is
fast on real text and why the profile matters.

## Reference

@ref mod_cellul "Generated reference"

---

# ASCII mask — character classes {#mod_ascii_guide}

Character classes as sixteen-byte bitmaps.

```c
if (mmgr_ascii_in(mmgr_ascii_digit, c)) { }
if (mmgr_ascii_in(mmgr_ascii_hex, c))   { }
```

Available: `num`, `alpha`, `alnum`, `upper`, `lower`, `hex`, `punct`, `space`, `print`, `ctrl`.

**Why sixteen bytes and not two `uint64_t`.** Because a `uint64_t` is not available as a single
value on a 16-bit build. Byte-indexed, the same table works at every carrier width — which is the
constraint that shapes half the decisions in this library.

Header-only, no dispatch table, no state. @ref mod_ascii "Generated reference"

---

# Anchor cost — where a search starts {#mod_anchor_guide}

A byte-frequency cost table. Substring search anchors on the rarest byte of the needle rather than
the first.

## Why

Searching for `"Content-Length:"` by scanning for `'C'` finds a candidate on nearly every line of
English text, and each one costs a full comparison. Scanning for `'-'` — or whichever byte is rarest
in the expected input — finds far fewer candidates and rejects them sooner.

The table says how common each byte is, so the search can pick.

## Profiles

Mutually exclusive, generic by default:

| define                        | tuned for             |
| ----------------------------- | --------------------- |
| _(none)_                      | generic               |
| `MMGR_ANCORAE_FORMA_ENGLISH` | English prose         |
| `MMGR_ANCORAE_FORMA_URI`     | URIs and paths        |
| `MMGR_ANCORAE_FORMA_INET`    | network protocol text |
| `MMGR_ANCORAE_FORMA_ROUTE`   | routing tables        |

**Picking the wrong one costs speed and never correctness.** The search still finds what is there; it
just tests more candidates on the way. So this is a tuning knob, not a configuration you have to get
right before the code works.

Header-only. @ref mod_anchor "Generated reference"
