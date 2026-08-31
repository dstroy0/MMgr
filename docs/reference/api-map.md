# Where a symbol lives {#ref_api_map}

You saw `spat.from` in a diff. This page says which module it came from.

## The naming law

Every public function is:

```
mmgr_<infix>_<tail>
```

`<infix>` is the module's stem. So a symbol tells you its module without a lookup:

| symbol                | infix    | module           |
| --------------------- | -------- | ---------------- |
| `mmgr_spat_from`      | `spat`   | `spatium`        |
| `mmgr_scrut_has_zero` | `scrut`  | `verbum_scrutor` |
| `mmgr_memor_cmp`      | `memor`  | `memoria_operor` |
| `mmgr_cellul_len`     | `cellul` | `cellularum_laboro` |

`locus_carcerum` is the exception. Its public entries are named for what they do rather than for the
module — `mmgr_persistent_buf_alloc`, `mmgr_temporary_buf_mark`, `mmgr_buf_available` — so there is
no `carcer` infix to read a module off. Reach them through a declared region's pools instead
(`prison.work.persistent_buf_alloc`), which is how the header presents them.

And the dispatch table is named for the same stem, so `spat.from` and `mmgr_spat_from` are the same
function reached two ways. See @ref concept_ns_idiom.

## Stem to module

| stem                                    | module                     | what it does                             |
| --------------------------------------- | -------------------------- | ---------------------------------------- |
| _(none, see above)_                     | `locus_carcerum`           | the double-ended region and its pools    |
| `anularis`                              | `memoria_anularis`         | SPSC ring, segment queue, loculus bitmap |
| `exter`                                 | `memoria_externa`          | DRAM against PSRAM placement             |
| `spat`                                  | `spatium`                  | a bounded view over caller memory        |
| `proxim`                                | `proximus_operor`          | unaligned, aligned and may-alias access  |
| `lane` / `mask` / `word`                | `verbum_scrutor`           | SWAR lane primitives                     |
| `memor`                                 | `memoria_operor`           | the `mem*` family                        |
| `cellul`                                | `cellularum_laboro`        | bounded string operations                |
| `verba`                                 | `verba_scribo`             | string and number writing                |
| `numer`                                 | `numeros_scribo`           | field-spec formatter                     |
| `muto`                                  | `transformo`               | decimal to binary scaling                |
| `fract`                                 | `fractio`                  | IEEE-754 field access                    |
| `clz`                                   | `clz`                      | leading zero count                       |
| `bitio`                                 | `bitorum_introitus_exitus` | bit writer                               |
| `byteio`                                | `octetus_introitus_exitus` | byte transfers, big end first            |
| `parva_extremitas` / `magna_extremitas` | `endian`                   | explicit byte order                      |
| `ascii`                                 | `ascii_persona_bitorum`    | character classes as bitmaps             |
| `ancorae`                               | `impensa_ancorae_acus`     | anchor cost tables for search            |
| `praet`                                 | `memoriam_praetereo`       | transfer submission, gated               |

## The exceptions

`proximus_operor` has one table, `proxim`, and keeps its three **strategies** apart in the entry
names instead: `load` and `put` are unaligned, `al_load` and `al_put` are aligned, and the may-alias
part is `mmgr_migro_word`, the type they all move. Merging them is a miscompile the compiler cannot
report, so the naming keeps them apart. `aequus` and `migro` are backend prefixes inside the `.c`
and a type name — neither is a table a caller reaches for.

`verbum_scrutor` splits the other way: three tables — `lane`, `mask`, `word` — over one module, named
for what each operates on, while every function keeps the module's own `scrut` infix.

`pow5` has no stem because it exposes only data: the two tables of powers of five a decimal
conversion needs. See @ref qa_numeric for what it is for.

## Types

| prefix        | is                                                              |
| ------------- | --------------------------------------------------------------- |
| `mmgr_<stem>` | a data type — `mmgr_span`, `mmgr_bitor`                         |
| `<Pascal>Cfg` | the argument struct an entry takes — `SpatiumCfg`, `MemoriaCfg` |
| `<Pascal>Ctx` | the state a module operates on — `CarcerCellBlock`               |
| `<Pascal>Ns`  | a dispatch table type — `SpatiumNs`, `ScrutLaneNs`              |
| `MMGR_<NAME>` | a macro or a constant this library owns                         |
| `EMBED_<NAME>` | a macro or a constant from `embedded_types`                    |

Every entry takes one `const <Pascal>Cfg *`, built at the call site with `EMBED_CALL`. A module
whose state outlives a call names that state `<Pascal>Ctx` and the caller holds it; the cfg carries
a pointer to it rather than the state itself.

## Finding it in the reference

- **API by module** groups every entity under the seven sections in @ref nav_modules.
- **Data Structures** lists every struct and typedef.
- **Files** lists every header with its include path, which is the path you actually write.

The search box indexes all three.

@note `tools/dev_env/names.tsv` is the machine-readable version of this page — the table the naming
law is enforced from.
