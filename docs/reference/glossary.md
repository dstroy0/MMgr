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

| Latin                            | stem                                   | in English                                        |
| -------------------------------- | -------------------------------------- | ------------------------------------------------- |
| `carceribus`                     | `carcer`                               | prisons — the region and its pools                |
| `confinium_exclusivum_infinitas` | `iteratio_infinita`                    | exclusive endless enclosure — the lock-free ring  |
| `confinium_externum`             | `exter`                                | external enclosure — the PSRAM pool               |
| `carceribus`                     | `carcer`                               | cells, confinement — the region and both its ends |
| `spatium`                        | `spat`                                 | space, extent — a span                            |
| `proximus_operor`                | `proxim`                               | nearest work — raw load and store                 |
| `verbum_scrutor`                 | `lane`, `mask`, `word`                 | word examiner — the SWAR scanner                  |
| `memoria_operor`                 | `memor`                                | memory work — the `mem*` family                   |
| `cellularum_laboro`              | `cellul`                               | cell work — bounded string operations             |
| `verba_scribo`                   | `verba`                                | I write words — the string writer                 |
| `numeros_scribo`                 | `numer`                                | I write numbers — the field formatter             |
| `transformo`                     | `muto`                                 | I transform — decimal to binary scaling           |
| `fractio`                        | `fract`                                | a breaking — IEEE-754 field access                |
| `bitorum_introitus_exitus`       | `bitio`                                | entry and exit of bits — the bit writer           |
| `octetus_introitus_exitus`       | `byteio`                               | entry and exit of octets — byte transfers         |
| `endian`                         | `parva_extremitas`, `magna_extremitas` | _(English)_ byte order. small end, large end      |
| `ascii_persona_bitorum`          | `ascii`                                | character masks of bits — the class bitmaps       |
| `impensa_ancorae_acus`           | `ancorae`                              | cost of an anchor point — the frequency tables    |
| `memoriam_praetereo`             | `praet`                                | I pass memory by — DMA transfer submission        |
| `clz`                            | `clz`                                  | _(English)_ count leading zeros                   |
| `pow5`                           | —                                      | _(English)_ powers of five for decimal work       |

## Verbs

The verbs are where the Latin does the most work, because the English words were the ones most
likely to mislead.

| verb            | means                        | why not the English                                                                       |
| --------------- | ---------------------------- | ----------------------------------------------------------------------------------------- |
| `capio`         | take                         | `alloc` implies a heap, and there isn't one                                               |
| `reddo`         | give back                    | `free` implies the memory returns to a pool for arbitrary reuse; it does not — it unwinds |
| `interim`       | the working half of a region | `scratch` was the pre-rename word and is gone. `interim` pairs with `persist`             |
| `persist`       | lives as long as the region  | —                                                                                         |
| `octas_praesto` | bytes at hand                | `free_bytes` reads as a verb — as though it frees something. It reports a count           |
| `mark`          | a position to rewind to      | —                                                                                         |
| `reset`         | return to empty              | —                                                                                         |

So `mmgr_carcer_interim_capio` is _take interim_, and `mmgr_carcer_persist_reddo` is _give back the
last persistent take_.

## Types

| type         | is                                                                 |
| ------------ | ------------------------------------------------------------------ |
| `CarcerCtx`  | a pool's state: base, size, both ends, and the hardware cap        |
| `mmgr_span`  | a buffer, its capacity and a cursor. `pos` is how much was written |
| `mmgr_bitor` | a bit writer: buffer, capacity, count, residue and overflow        |
| `mmgr_word`  | the machine word, unsigned — the SWAR carrier                      |
| `mmgr_iword` | the machine word, signed. the same register as `mmgr_word`         |
| `mmgr_idx`   | an index into a region, narrower than a word on some builds        |
| `mmgr_bool`  | `MMGR_TRUE` or `MMGR_FALSE`                                        |

## Other terms this documentation uses

| term            | means                                                                   |
| --------------- | ----------------------------------------------------------------------- |
| **borrow**      | storage the library was handed and does not own. Everything is a borrow |
| **tenant**      | a pool's region over its static buffer                                  |
| **custodia**    | a guarded pool that hands out tenants — `soluta` or `secura`            |
| **carrier**     | the integer a SWAR operation runs on. Always the machine word           |
| **lane**        | one byte inside the carrier                                             |
| **environment** | one set of compile-time widths. See @ref ref_environments               |
| **latch**       | a flag that stays set once set, so a run is checked once at the end     |

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
