# The dispatch table idiom {#concept_ns_idiom}

Why a call reads `EMBED_CALL(spat.from, SpatiumCfg, .buf = p, .cap = n)` and what that costs.

## Two spellings, one function

Every module declares plain C functions with a long, prefixed name, and also gathers them into a
`static const` struct of function pointers named for a short Latin stem.

Every entry takes exactly one argument: a pointer to that module's config struct. @ref EMBED_CALL
builds the struct as a compound literal and passes its address, so the members are named at the
call site and anything left out is zero:

```c
mmgr_span s = EMBED_CALL(spat.from, SpatiumCfg, .buf = p, .cap = n);
```

which is the same call as

```c
mmgr_span s = mmgr_spat_from(&(SpatiumCfg){.buf = p, .cap = n});
```

Both spellings are public and both are documented. The table is what call sites use, because at a
call site the module is context you already have and repeating it is noise:

```c
if (EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = w) &&
    !EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = a, .other = b, .cap = n, .ci = EMBED_FALSE))
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = dst, .src = src, .bytes = n);
}
```

The library keeps the free functions because they are what a debugger, a linker map and a `nm`
listing show you, and a namespace built out of macros would show nothing.

Naming the members also means an entry cannot be called with its arguments in the wrong order, and
an entry that grows a member does not silently change the meaning of existing call sites.

## How it is declared

Three pieces, in every module header:

```c
typedef struct
{
    mmgr_span (*from)(const SpatiumCfg *c);
    mmgr_cspan (*cfrom)(const SpatiumCfg *c);
} SpatiumNs;
EMBED_TABLE_LAYOUT(SpatiumNs, from, cfrom);

EMBED_TABLE_STORAGE SpatiumNs spat EMBED_UNUSED = {.from = mmgr_spat_from, .cfrom = mmgr_spat_cfrom};
```

`EMBED_TABLE_STORAGE` is `static const`. `EMBED_UNUSED` is what lets an unreferenced table drop out of a
translation unit that does not use it.

## EMBED_TABLE_LAYOUT is the interesting part

The table is addressed **by offset**. A positional initializer mis-wires silently when a member is
inserted, removed or moved — the code still compiles, and `spat.from` calls something else.

`EMBED_TABLE_LAYOUT` pins it. It expands to a chain of `_Static_assert`s checking that each named member
sits at its own loculus, in the order given, and that `sizeof` the struct is exactly that many
pointers. A member added and not listed, a member reordered, or padding appearing between them all
fail **at the declaration**, at compile time.

That has one consequence for this documentation: **the order of the members is the layout**, so the
reference must not sort them alphabetically. `SORT_MEMBER_DOCS` is `NO` in `docs/Doxyfile` for that
reason, and it is not a cosmetic setting — sorting `MemoriaOperorNs` would show loculus 0 as `chr`
when it is `cpy`.

## What the indirection costs

Nothing, when `const` is respected.

Measured: gcc devirtualizes a call through a `static const <Mod>Ns` down to the inlined body —
identical instructions to calling the entry directly — and does **not** devirtualize the same call
through a non-const table, which becomes a real call with an 88-byte frame. clang devirtualizes
both. So the `const` in `EMBED_TABLE_STORAGE` is load-bearing: it is what makes the two compilers agree.

Across module boundaries where the entry is not `EMBED_INLINE`, link-time optimization is what closes
the gap. See @ref concept_swar for the measurement.

## Modules with more than one table

`verbum_scrutor` has three: `lane`, `mask` and `word`. The entries were one table until the count
passed what `EMBED_TABLE_LAYOUT` accepts, and splitting them by what they operate on — a lane of a word,
a mask of lanes, a whole word — reads better than one table of everything did.

`endian` is the other shape: two tables, `parva_extremitas` and `magna_extremitas`, over the same
`EndianNs` type. The byte order is chosen by which table you call through, not by an argument.

@ref ref_glossary lists every stem and the module it belongs to.
