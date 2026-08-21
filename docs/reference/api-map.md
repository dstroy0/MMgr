# Where a symbol lives {#ref_api_map}

You saw `spat.produced` in a diff. This page says which module it came from.

## The naming law

Every public function is:

```
mmgr_<infix>_<tail>
```

`<infix>` is the module's stem. So a symbol tells you its module without a lookup:

| symbol                      | infix    | module               |
| --------------------------- | -------- | -------------------- |
| `mmgr_spat_from`            | `spat`   | `spatium`            |
| `mmgr_confin_persist_capio` | `confin` | `confinium`          |
| `mmgr_scrut_has_zero`       | `scrut`  | `verbum_scrutor`     |
| `mmgr_occult_wipe`          | `occult` | `occultum_custodiae` |

And the dispatch table is named for the same stem, so `spat.from` and `mmgr_spat_from` are the same
function reached two ways. See @ref concept_ns_idiom.

## Stem to module

| stem                          | module                           | what it does                           |
| ----------------------------- | -------------------------------- | -------------------------------------- |
| `confin`                      | `confinium`                      | the double-ended region                |
| `infin`                       | `confinium_exclusivum_infinitas` | SPSC ring, segment queue, loculus bitmap  |
| `exter`                       | `confinium_externum`             | DRAM against PSRAM placement           |
| `clarus`                      | `clarus_custodiae`               | plaintext pool                         |
| `occult`                      | `occultum_custodiae`             | secure pool, with a wipe               |
| `spat` / `fspat`              | `spatium`                        | writable span / read-only span         |
| `proxim` / `aequus` / `migro` | `proximus_operor`                | unaligned / aligned / may-alias access |
| `scrut`                       | `verbum_scrutor`                 | SWAR lane primitives                   |
| `memor`                       | `memoria_operor`                 | the `mem*` family                      |
| `cellul`                      | `cellularum_laboro`              | bounded string operations              |
| `verba`                       | `verba_scribo`                   | string builder                         |
| `numer`                       | `numeros_scribo`                 | field-spec formatter                   |
| `fract`                       | `fractio`                        | IEEE-754 field access                  |
| `bitio`                       | `bitio`                          | bit writer                             |
| `byteio`                      | `byteio`                         | byte and wire serialization            |
| `endian`                      | `endian`                         | explicit byte order                    |
| `dma`                         | `dma`                            | transfer submission, gated             |
| `worker`                      | `confinium`                      | worker identity, used by the pools     |

## The three exceptions

`proxim`, `aequus` and `migro` all belong to `proximus_operor`. They are three infixes for one module
because they name three **strategies**, not three spellings of one thing: unaligned, aligned, and
may-alias. Merging them is a miscompile the compiler cannot report, so the naming keeps them apart.

`worker` is declared by `confinium` but used by the pools, so it names a cross-cutting concept rather
than a module.

Three modules have no stem at all because they expose only data, not entries: `ascii_persona_bitorum`
(character classes as bitmaps), `impensa_ancorae_acus` (byte-frequency tables) and `pow5` (the powers of five
a decimal conversion needs). All three are generated - see @ref qa_numeric for what `pow5` is for.

## Types

| prefix        | is                                                     |
| ------------- | ------------------------------------------------------ |
| `mmgr_<stem>` | a data type — `mmgr_spat`, `mmgr_confin`, `mmgr_verba` |
| `<Pascal>Ns`  | a dispatch table type — `SpatiumNs`, `VerbumScrutorNs` |
| `MMGR_<NAME>` | a macro or a constant                                  |

A type takes the **stem**, not the long Latin: `mmgr_confin`, not `mmgr_confinium`. A type spelled
`mmgr_confinium` beside a function spelled `mmgr_confin_set_add` reads as two different modules.

## Finding it in the reference

- **API by module** groups every entity under the seven sections in @ref nav_modules.
- **Data Structures** lists every struct and typedef.
- **Files** lists every header with its include path, which is the path you actually write.

The search box indexes all three.

@note `tools/dev_env/names.tsv` is the machine-readable version of this page — the table the naming
law is enforced from.
