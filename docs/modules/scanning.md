# Memoria operor — the mem* family {#mod_memor_guide}

Bulk memory work, a word at a time.

## When to reach for it

Anywhere you would call `memcpy`, `memmove`, `memcmp`, `memchr` or `memset` and want the SWAR
implementation and a bound you state at the call.

```c
MMGR_CALL(memor.cpy, MemoriaCfg, .dst = dst, .src = src, .bytes = n);
MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = 0xFFu, .bytes = n);

if (MMGR_CALL(memor.cmp, MemoriaCfg, .src = a, .other = b, .bytes = n) == 0) { }

const void *hit = MMGR_CALL(memor.chr, MemoriaCfg, .src = p, .val = (uint8_t)'x', .bytes = n);
```

Both spellings exist: `mmgr_memor_cpy` is the same function. See @ref concept_ns_idiom.

## Gotchas

**`cpy` does not handle overlap, and there is no single `move`.** There are two, and which one is
safe depends on which way the regions lie: `move_up` when `dst` is above `src`, `move_down` when it
is below. `move_down` is `cpy` — copying forward is already the safe direction that way — so the
table points both at the same function.

**There is no `zero` entry.** Use `set` with `.val = 0`.

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
const size_t n = MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = s, .cap = MMGR_STR_MAX);

if (MMGR_CALL(cellul.starts, CatenaFinitaCfg, .src = s, .other = "GET ", .cap = n,
              .ci = MMGR_FALSE)) { }

const char *host = MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = hay, .cap = hay_len,
                             .other = "Host:", .other_cap = 5u, .ci = MMGR_TRUE);

const char *end = NULL;
const mmgr_iword v = MMGR_CALL(cellul.to_long, TransfiguroCfg, .src = digits, .end = &end);
```

`.ci` is case-insensitivity, not a success flag.

## Gotchas

**`find` returns a pointer, not an offset.** NULL when the needle is not there.

**The parse entries report where they stopped, not whether they succeeded.** `to_long` and the rest
write the first byte they did not consume to `.end`. Equal to `.src` means nothing was parsed.

**`to_long` and `to_ulong` return `mmgr_iword` and `mmgr_word`,** so they parse to the target's own
word width. On a 16-bit build `to_ulong` saturates at 65535, not at 4294967295.

**The cap is a cap, not a length.** `len` with `.cap = 64` returns at most 64 whether or not a
terminator was found. If you need to know which happened, compare against the cap.

**`find` anchors on the rarest byte of the needle**, using @ref mod_anchor_guide. That is why it is
fast on real text and why the profile matters.

**Except for a needle of one or two bytes**, which is settled by a mask chain instead: one broadcast
per needle byte, every start position in a word decided at once, nothing to verify afterwards. There
is no rare byte to find in two bytes and no rest to prove, so the anchor's cost table would be read
before a haystack byte is. The chain also costs the same whatever the haystack holds, where an
anchor costs more when its byte is common — measured against a needle whose first byte turns up every
fifteen bytes, MMgr does not move and ROM `strstr` loses 10%. Case folding always takes the anchor
path; the chain compares raw bytes. See `MMGR_FIND_CHAIN_MAX` in @ref ref_configuration.

## Reference

@ref mod_cellul "Generated reference"

---

# ASCII mask — character classes {#mod_ascii_guide}

Character classes as sixteen-byte bitmaps.

```c
if (MMGR_CALL(ascii.in, AsciiCfg, .kind = MMGR_ASCII_NUM, .byte = c)) { }
if (MMGR_CALL(ascii.in, AsciiCfg, .kind = MMGR_ASCII_HEX, .byte = c)) { }
```

Available: `MMGR_ASCII_NUM`, `MMGR_ASCII_ALPHA`, `MMGR_ASCII_ALNUM`, `MMGR_ASCII_UPPER`,
`MMGR_ASCII_LOWER`, `MMGR_ASCII_HEX`, `MMGR_ASCII_PUNCT`, `MMGR_ASCII_SPACE`, `MMGR_ASCII_PRINT`,
`MMGR_ASCII_CTRL`.

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

One CMake option taking a value, not a set of defines — `-DMMGR_ANCORAE_FORMA=english`. It selects
the single translation unit that carries the table
(`src/impensa_ancorae_acus/CMakeLists.txt:7-15`), so the four not chosen are not in the image:

| value     | tuned for             |
| --------- | --------------------- |
| `generic` | generic, the default  |
| `english` | English prose         |
| `uri`     | URIs and paths        |
| `inet`    | network protocol text |
| `route`   | routing tables        |

**Picking the wrong one costs speed and never correctness.** The search still finds what is there; it
just tests more candidates on the way. So this is a tuning knob, not a configuration you have to get
right before the code works.

Header-only. @ref mod_anchor "Generated reference"
