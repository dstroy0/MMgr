# The Latin, decoded {#ref_glossary}

Every module, stem and verb in the library, and what it means in plain English.

## Why Latin at all

The names are category names, and they were chosen so that nothing in this library can collide with
libc, with a consumer's own vocabulary, or with the half-dozen other things in an embedded codebase
already called `buffer`, `pool`, `arena` or `span`.

That is the whole reason. It is not decoration, and it is not obscurity for its own sake: a module
called `spatium` can be grepped for with confidence, and a symbol called `mmgr_spat_from` cannot be
confused with anybody else's span. The cost is this page, which is a fair trade for never having to
rename anything again.

`tools/dev_env/names.tsv` is the source of truth for the tables below.

## Modules

| Latin                            | stem     | PascalCase                     | in English                                       |
| -------------------------------- | -------- | ------------------------------ | ------------------------------------------------ |
| `confinium`                      | `confin` | `Confinium`                    | boundary, enclosure — the double-ended region    |
| `confinium_exclusivum_infinitas` | `infin`  | `ConfiniumExclusivumInfinitas` | exclusive endless enclosure — the lock-free ring |
| `confinium_externum`             | `exter`  | `ConfiniumExternum`            | external enclosure — the PSRAM pool              |
| `clarus_custodiae`               | `clarus` | `ClarusCustodiae`              | clear guardianship — the plaintext pool          |
| `occultum_custodiae`             | `occult` | `OccultumCustodiae`            | hidden guardianship — the secure pool            |
| `spatium`                        | `spat`   | `Spatium`                      | space, extent — a span                           |
| `proximus_operor`                | `proxim` | `ProximusOperor`               | nearest work — raw load and store                |
| `verbum_scrutor`                 | `scrut`  | `VerbumScrutor`                | word examiner — the SWAR scanner                 |
| `memoria_operor`                 | `memor`  | `MemoriaOperor`                | memory work — the `mem*` family                  |
| `cellularum_laboro`              | `cellul` | `CellularumLaboro`             | cell work — bounded string operations            |
| `verba_scribo`                   | `verba`  | `VerbaScribo`                  | I write words — the string builder               |
| `numeros_scribo`                 | `numer`  | `NumerosScribo`                | I write numbers — the field formatter            |
| `fractio`                        | `fract`  | `Fractio`                      | a breaking — IEEE-754 field access               |
| `bitio`                          | `bitio`  | `Bitio`                        | _(English)_ bit I/O                              |
| `byteio`                         | `byteio` | `Byteio`                       | _(English)_ byte I/O                             |
| `endian`                         | `endian` | `Endian`                       | _(English)_ byte order                           |
| `ascii_persona_bitorum`                     | —        | —                              | _(English)_ character classes as bitmaps         |
| `impensa_ancorae_acus`                    | —        | —                              | _(English)_ byte-frequency table for search      |
| `pow5`                           | —        | —                              | _(English)_ powers of five for decimal work      |
| `dma`                            | `dma`    | `Dma`                          | _(English)_ direct memory access                 |

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

So `mmgr_confin_interim_capio` is _take interim_, and `mmgr_confin_persist_reddo` is _give back the
last persistent take_.

## Types

| type          | is                                                                     |
| ------------- | ---------------------------------------------------------------------- |
| `mmgr_confin` | a region: base, both ends, and the limits                              |
| `mmgr_spat`   | a writable span. `pos` is how much was written; `overflow` latches     |
| `mmgr_verba`  | a string builder over a span                                           |
| `mmgr_word`   | the machine word — the SWAR carrier                                    |
| `mmgr_idx`    | an index into a region, narrower than a word on some builds            |
| `mmgr_bool`   | `MMGR_TRUE` or `MMGR_FALSE`                                            |

## Other terms this documentation uses

| term            | means                                                                   |
| --------------- | ----------------------------------------------------------------------- |
| **borrow**      | storage the library was handed and does not own. Everything is a borrow |
| **tenant**      | a pool's region over its static buffer                                    |
| **custodia**    | a guarded pool that hands out tenants — `clarus` or `occult`            |
| **carrier**     | the integer a SWAR operation runs on. Always the machine word           |
| **lane**        | one byte inside the carrier                                             |
| **environment** | one set of compile-time widths. See @ref ref_environments               |
| **latch**       | a flag that stays set once set, so a run is checked once at the end     |

## The naming law

Every public function is `mmgr_<infix>_<tail>`, where `<infix>` is the module's stem. `spat.from` is
`mmgr_spat_from`; `scrut.has_zero` is `mmgr_scrut_has_zero`.

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
