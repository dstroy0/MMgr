# The Latin, decoded {#ref_glossary}

Every module, stem and verb in the library, and what it means in plain English.

## Why Latin at all

libc and `<string.h>` are everywhere, and these are not libc functions. They reach the same
conclusion for the same input, across every domain — that is all they share. How they reach it is
entirely different: the backend is machine-width parallel word processing throughout, bitmath is
extensive, and SIMD within a register is used wherever it applies.

A name that collided with libc's would advertise a drop-in replacement of the implementation as well
as of the result. The names also keep the library clear of a consumer's own vocabulary and of the
half-dozen other things in an embedded codebase already called `buffer`, `pool`, `arena` or `span`.

That is the whole reason. It is not decoration, and it is not obscurity for its own sake: a module
called `spatium` can be grepped for with confidence, and a symbol called `mmgr_spat_from` cannot be
confused with anybody else's span. The cost is this page, which is a fair trade for never having to
rename anything again.

`tools/dev_env/names.tsv` is the source of truth for the tables below.

## Modules

The stem is the name of the module's dispatch table, so it is what a call site reads.

| Latin                      | stem                                   | in English                                        |
| -------------------------- | -------------------------------------- | ------------------------------------------------- |
| `memoria_anularis`         | `anularis`                             | ring memory — the lock-free SPSC ring             |
| `memoria_externa`          | `exter`                                | external memory — the PSRAM pool                  |
| `locus_carcerum`           | —                                      | place of prisons — the region and both its ends   |
| `spatium`                  | `spat`                                 | space, extent — a span                            |
| `proximus_operor`          | `proxim`                               | nearest work — raw load and store                 |
| `verbum_scrutor`           | `lane`, `mask`, `word`                 | word examiner — the SWAR scanner                  |
| `memoria_operor`           | `memor`                                | memory work — the `mem*` family                   |
| `cellularum_laboro`        | `cellul`                               | cell work — bounded string operations             |
| `verba_scribo`             | `verba`                                | I write words — the string writer                 |
| `numeros_scribo`           | `numer`                                | I write numbers — the field formatter             |
| `transformo`               | `muto`                                 | I transform — decimal to binary scaling           |
| `fractio`                  | `fract`                                | a breaking — IEEE-754 field access                |
| `bitorum_introitus_exitus` | `bitio`                                | entry and exit of bits — the bit writer           |
| `octetus_introitus_exitus` | `byteio`                               | entry and exit of octets — byte transfers         |
| `endian`                   | `parva_extremitas`, `magna_extremitas` | _(English)_ byte order. small end, large end      |
| `ascii_persona_bitorum`    | `ascii`                                | character masks of bits — the class bitmaps       |
| `impensa_ancorae_acus`     | `ancorae`                              | cost of an anchor point — the frequency tables    |
| `memoriam_praetereo`       | `praet`                                | I pass memory by — DMA transfer submission        |
| `clz`                      | `clz`                                  | _(English)_ count leading zeros                   |
| `pow5`                     | —                                      | _(English)_ powers of five for decimal work       |

## Verbs

The verbs are where the Latin does the most work, because the English words were the ones most
likely to mislead.

The public entries are English. A handle is assembled from its slots — `[lifetime]_[security]_[what
it is]_[action]` — with each slot spent only where it disambiguates.

| slot            | values                       | what it settles                                                       |
| --------------- | ---------------------------- | --------------------------------------------------------------------- |
| lifetime        | `persistent`, `temporary`    | the sentence. How long the prisoner holds the cell                    |
| security        | `max_security`               | present only where the zeroing form has to be told from the plain one |
| what it is      | `buf`                        | bytes, as against a mark or a count                                   |
| action          | `alloc`, `release`, `return` | what the entry does to them                                           |

So `persistent_buf_alloc` is lifetime/—/buf/alloc, and `persistent_max_security_buf_release` fills
all four. `max_security_buf_return` drops the lifetime slot, because `return` restores a mark and
only the temporary tier has one.

| entry           | means                   | why not the English                                                             |
| --------------- | ----------------------- | ------------------------------------------------------------------------------- |
| `buf_available` | bytes at hand           | `free_bytes` reads as a verb, as though it frees something. It reports a count  |
| `mark`          | a position to rewind to | —                                                                               |
| `reset`         | return to empty         | —                                                                               |

`temporary_buf_alloc` takes a cell from the temporary tier. `persistent_buf_release` takes **the cell
itself** and gives it back — in any order, because the cell's own header carries its extent, so the
release does not have to be told which one it is or when it was taken.

## Types

| type               | is                                                                     |
| ------------------ | ---------------------------------------------------------------------- |
| `CarcerCellBlock`  | a cellblock's state: the pool it covers, its extent, and both tier ends |
| `CarcerCell`       | one cell's header: the payload behind it, and whether it is held        |
| `mmgr_span`        | a buffer, its capacity and a cursor. `pos` is how much was written     |
| `mmgr_cspan`       | the read counterpart. `len` is the extent, `err` latches a short read  |
| `mmgr_ring`        | opaque storage a caller declares for one ring                          |
| `embed_word`       | the machine word, unsigned. The SWAR carrier                           |
| `embed_iword`      | the machine word, signed. The same register as `embed_word`            |
| `embed_bool`       | `EMBED_TRUE` or `EMBED_FALSE`                                          |

The `embed_` types come from `embedded_types`, a separate library MMgr consumes. See
@ref ref_compiler_support.

## Other terms this documentation uses

The prison is a teaching metaphor, and the vocabulary follows it end to end. Where memory management
already has the exact word — `persistent`, `temporary`, `alignment`, `header` — that word ships, and
the metaphor stays behind the name.

| term               | means                                                                          |
| ------------------ | ------------------------------------------------------------------------------ |
| **borrow**         | storage the library was handed and does not own. Everything is a borrow        |
| **pool**           | a block of bytes, declared by `ParsMemoriaeInternae` or `ParsMemoriaeExternum` |
| **facility**       | one prison site, declared by `LocusCarcerum`                                   |
| **cellblock**      | a housing unit inside a facility, dressed over one pool                        |
| **custodia**       | the guards posted to a cellblock, `soluta` or `secura`                         |
| **soluta**         | general population. Open, and a release leaves the bytes as they are           |
| **secura**         | solitary. Controlled, and a release zeroes the cell first                      |
| **cell**           | one allocation, its header and the payload behind it                           |
| **prisoner**       | what an allocation hands back. Never a *tenant*                                |
| **claim**          | dressing a pool. It happens once, at the declaration, and the build enforces it |
| **carrier**        | the integer a SWAR operation runs on. Always the machine word                  |
| **lane**           | one byte inside the carrier                                                    |
| **environment**    | one set of compile-time widths. See @ref ref_environments                      |
| **latch**          | a flag that stays set once set, so a run is checked once at the end            |

The two axes are independent, which is the part worth holding on to. **Which custodia** is the
environment. It is fixed at the cellblock's declaration and governs how a release behaves.
**Persistent or temporary** is the sentence, how long the prisoner holds the cell, and it is chosen
per allocation. A long sentence in general population is an ordinary thing, and so is a short stay in
solitary.

## The naming law

Every public function is `mmgr_<infix>_<tail>`, where `<infix>` is the module's stem. `spat.from` is
`mmgr_spat_from`, `memor.cpy` is `mmgr_memor_cpy`.

`verbum_scrutor` is the exception: its three tables are named for what they operate on — `lane`,
`mask`, `word` — while the functions all carry the module's own infix, so `lane.has_zero` is
`mmgr_scrut_has_zero`.

Three infixes do not name their module, and that is deliberate rather than an oversight — they name
distinct _concepts_ inside `proximus_operor`, and collapsing them onto one stem would merge things
that are not the same thing:

| infix    | is                       |
| -------- | ------------------------ |
| `proxim` | an unaligned access      |
| `aequus` | an aligned access        |
| `migro`  | an access that may alias |

Merging the aligned and unaligned load is a miscompile the compiler cannot report. See
@ref mod_proxim_guide.
