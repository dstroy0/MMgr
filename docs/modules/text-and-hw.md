# Verba scribo — the string builder {#mod_verba_guide}

Text out, without `printf` and without a heap.

## When to reach for it

Any time you would reach for `snprintf` and would rather not link a formatter, or cannot afford one.

## Worked example

```c
mmgr_verba b = verba.from(spat.from(buf, sizeof buf));

verba.put(&b, "id=");
verba.u32(&b, id);
verba.put(&b, " rate=");
verba.fixed(&b, rate, 2);          /* two decimal places */
verba.ch(&b, '\n');

if (!verba.finish(&b)) {
    /* one check, covering all five appends */
}
```

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

`mmgr_verba_lit(b, "literal")` is a macro that passes the length with the string, so a literal costs
no `strlen`.

## Gotchas

**The flag latches, so check once.** Appends after an overflow are safe no-ops. See
@ref ref_error_handling.

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

```c
static const mmgr_field row[] = {
    MMGR_STR("id="),  MMGR_U32(0),
    MMGR_STR(" hex="), MMGR_VHEX(1),
    MMGR_STR(" g="),   MMGR_VG(2),
    MMGR_END
};

mmgr_fval vals[] = { {.u32 = id}, {.u32 = flags}, {.d = ratio} };
numer.build(&b, row, vals, 3);
```

Two entries: `build` writes a record, `append` adds to one already started.

## Why a spec array rather than a format string

A format string is parsed at runtime and its relationship to the arguments is unchecked — the
classic `%d` against a `long` bug. Here the layout is a `const` array in flash, the kinds are an
enum, and the values are a tagged union. There is nothing to parse and nothing to mismatch at
runtime.

The cost is that the layout is less readable at a glance than a format string. For a record emitted
in one place and read in a thousand, that is the right trade.

## Gotchas

**`MMGR_END` terminates the spec.** Leaving it off runs off the end of the array.

**The index in `MMGR_U32(0)` is into the value array**, not a position in the output.

**The union is tagged by the spec, not by itself.** A `MMGR_VG` reading a slot you filled as `.u32`
is a bug the compiler cannot see.

@ref mod_numer "Generated reference"

---

# DMA — transfer submission {#mod_dma_guide}

@note Compiled only when `MMGR_ENABLE_DMA` is set. It defaults off, and its test suite is skipped
loudly rather than silently.

## What it is

A thin, portable surface over a DMA controller: open a channel, submit a transfer, poll or take a
callback, close it.

```c
mmgr_dma_config cfg = {
    .periph  = MMGR_DMA_UART,
    .dir     = MMGR_DMA_TX,
    .channel = 0,
};

mmgr_dma_h h = mmgr_dma_open(&cfg);
mmgr_dma_tx_submit(h, buf, len);
mmgr_dma_poll(h);
mmgr_dma_close(h);
```

## The hardware hooks are weak

The functions that actually touch a controller are `MMGR_WEAK`. A board support file overrides one
by defining a symbol with the same name — no registration, no function pointer table, no init order
to get right.

Without an override they are present and inert, which is what lets `dma` compile and its tests link
on a host with no DMA controller at all.

## Gotchas

**A buffer submitted to DMA must outlive the transfer.** The module cannot know when the controller
is finished with it — that is what `poll` and the completion callback are for. This is the one place
in MMgr where a lifetime is decided by hardware rather than by a mark.

**Cache coherency is not handled here.** On a part with a data cache and a DMA engine that does not
snoop it, the clean and invalidate are the board file's job.

**`MMGR_DMA_CHANNELS` and `MMGR_DMA_BUF_SIZE` only exist when the gate is on.** See
@ref ref_configuration.

@ref mod_dma "Generated reference"
