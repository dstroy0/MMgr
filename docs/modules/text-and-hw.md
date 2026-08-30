# Verba scribo — the string builder {#mod_verba_guide}

Text out, without `printf` and without a heap.

## When to reach for it

Any time you would reach for `snprintf` and would rather not link a formatter, or cannot afford one.

## Worked example

```c
size_t at = 0;

at = MMGR_CALL(verba.put,   VerbaCfg, .out = buf, .cap = sizeof buf, .at = at, .text = "id=");
at = MMGR_CALL(verba.uint,   VerbaCfg, .out = buf, .cap = sizeof buf, .at = at, .val = id);
at = MMGR_CALL(verba.put,   VerbaCfg, .out = buf, .cap = sizeof buf, .at = at, .text = " rate=");
at = MMGR_CALL(verba.fixed, VerbaCfg, .out = buf, .cap = sizeof buf, .at = at, .real = rate,
               .decimals = 2);
at = MMGR_CALL(verba.ch,    VerbaCfg, .out = buf, .cap = sizeof buf, .at = at, .ch = '\n');

const size_t len = MMGR_CALL(verba.finish, VerbaCfg, .out = buf, .cap = sizeof buf, .at = at);
if (len == 0u) {
    }
```

The writers hold no state. Each one takes the position it should write at and returns the position
after what it wrote, so the cursor is the caller's `at` and nothing is carried between calls. A
writer with no room returns `cap`, which every later writer also returns, so an overflow propagates
to `finish` without a flag and `finish` reports it as a length of zero.

There is no format string anywhere. Nothing parses `%d` at runtime, so nothing can disagree with the
argument you passed.

## The entries, by job

| job            | entries                                         |
| -------------- | ----------------------------------------------- |
| raw text       | `put`, `put_n`, `put_clip`, `ch`                |
| unsigned       | `uint`, `u32`, `u32w`, `u64`, `u64_clip`, `hex` |
| signed         | `i64`                                           |
| floating point | `g`, `fixed`, `sign_bit`, `is_inf`, `is_nan`    |
| escaping       | `xml`, `json`                                   |
| finishing      | `finish`                                        |

`put_n` takes the length with the text, so a literal costs no scan: pass
`sizeof "literal" - 1u`. `put` measures what it is given.

## Gotchas

**Overflow propagates, so check once at the end.** A writer with no room returns `cap` and every
later writer returns `cap` too, so the writes after an overflow are safe no-ops and `finish`
reports zero. See @ref ref_error_handling.

**`xml` and `json` escape, they do not quote.** You supply the surrounding quotes.

**`put_clip` truncates deliberately.** It is for a field with a maximum width, and it is not the same
as running out of room.

**Float formatting is exact bit work, not `printf`.** It goes through @ref mod_fract_guide.

@ref mod_verba "Generated reference"

---

# Numeros scribo — the field formatter {#mod_numer_guide}

A declarative layout. The format is data, not a string.

## When to reach for it

A record with a fixed shape emitted many times — a log line, a telemetry frame, a fixed-width table.
Describe the layout once as an array and fill values in.

## Worked example

Two arrays. The spec says what the record looks like and holds no data. The values are supplied
separately and are consumed in order.

```c
static const mmgr_field row[] = {
    {MMGR_FK_LIT, 0, 3, "id="},
    MMGR_U32,
    {MMGR_FK_LIT, 0, 5, " hex="},
    {MMGR_FK_HEX, 0u, 0u, NULL},
    MMGR_END
};

const mmgr_fval vals[] = { MMGR_VU32(id), MMGR_VHEX(flags) };

MMGR_CALL(numer.build, NumerosCfg, .out = buf, .cap = sizeof buf,
          .spec = row, .vals = vals, .nvals = 2u);
```

A literal is a `MMGR_FK_LIT` field carrying its own text and length, so it costs no scan. Every
other spec entry is a bare kind taking no argument. There is a one-word macro for some of them —
`MMGR_STR`, `MMGR_U32`, `MMGR_U64`, `MMGR_I64`, `MMGR_CH`, `MMGR_JSON`, `MMGR_XML`, `MMGR_END`
(`src/numeros_scribo/numeros_scribo.h:60-85`) — and the rest are written as the brace form above.
`MMGR_FK_HEX`, `MMGR_FK_DEC`, `MMGR_FK_OCT`, `MMGR_FK_G` and `MMGR_FK_FIX` have no such macro.

Four entries: `build` writes a record from a spec, `emit` writes values with no spec at all,
and `append` and `emit_append` add to a record already in the buffer.

## Why a spec array rather than a format string

A format string is parsed at runtime and nothing checks it against the arguments. Here the spec is a
`const` array in flash, the kind is an enum, and every value carries its own kind tag. `build`
compares the two and refuses the record if they disagree.

## Gotchas

**`MMGR_END` terminates the spec.** Leaving it off runs off the end of the array.

**A kind mismatch returns 0 and empties the buffer.** So does too few values, or too many. The
record is written or it is not; there is no partial record.

**`emit` takes no spec.** Each value carries its own kind and width, so it is the entry to use when
the layout is not fixed.

@ref mod_numer "Generated reference"

---

# Memoriam praetereo — transfer submission {#mod_praet_guide}

@note Compiled only when `MMGR_ENABLE_DMA` is set, which defaults off. With it off the module is not
in the library and its suite is skipped with a message rather than passing empty.

## What it is

A thin, portable surface over a DMA controller: open a channel, submit a transfer, poll or take a
callback, close it.

```c
/* peripheral is a plain uint8_t the port assigns a meaning to; the library carries no
   enum of peripherals, because which ones exist is the part's business and not its own. */
const PraetCfg ch = {
    .channel = 0,
    .peripheral = PORT_UART0,
};

if (MMGR_CALL(praet.open, PraetCfg, .channel = ch.channel, .peripheral = ch.peripheral))
{
    MMGR_CALL(praet.tx_submit, PraetTransferCfg, .channel = 0, .buf = buf, .len = len);
    MMGR_CALL(praet.poll, PraetCfg, .channel = 0);
    MMGR_CALL(praet.close, PraetTransferCfg, .channel = 0);
}
```

There is no handle. The channel number identifies the transfer, so nothing has to be stored between
`open` and `close`. `open` and `tx_submit` return false when the channel is busy or out of range.

A completion callback is optional: point `PraetCfg::on_complete` at a @ref PraetCallbackCfg and it
is called with a @ref mmgr_praet_event describing what finished.

## The hardware hooks are weak

The functions that actually touch a controller are `MMGR_WEAK`. A board support file overrides one
by defining a symbol with the same name — no registration, no function pointer table, no init order
to get right.

Without an override they are present and inert, which is what lets `memoriam_praetereo` compile and
its tests link on a host with no DMA controller at all.

## Gotchas

**A submitted buffer must stay valid until the transfer completes** [BORROWS]. The module does not
copy it and cannot tell when the controller is done — that is what `poll` and the completion
callback report. Everywhere else in MMgr a lifetime ends at a mark you control; here it ends when
the hardware says so.

**Cache coherency is not handled here.** On a part with a data cache and a DMA engine that does not
snoop it, the clean and invalidate are the board file's job.

**`MMGR_PRAET_CHANNELS` and `MMGR_PRAET_BUF_SIZE` only exist when the gate is on.** See
@ref ref_configuration.

@ref mod_praet "Generated reference"
