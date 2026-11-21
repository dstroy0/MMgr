# The dispatch table idiom {#concept_ns_idiom}

Why a call reads `EMBED_CALL(spat.from, SpatiumCfg, .buf = p, .cap = n)` and what that costs.

## Two forms, one function

Every module declares plain C functions with a long, prefixed name. It also gathers them into a
`static const` struct of function pointers named for a short Latin stem.

Every entry takes exactly one argument: a pointer to that module's config struct. The macro
@ref EMBED_CALL builds the struct as a compound literal and passes its address. So the call site
names the members, and anything left out is zero:

```c
mmgr_span s = EMBED_CALL(spat.from, SpatiumCfg, .buf = p, .cap = n);
```

the same call as

```c
mmgr_span s = mmgr_spat_from(&(SpatiumCfg){.buf = p, .cap = n});
```

Both forms are public, and the documentation covers both. The table is what call sites use: at a
call site the module is context you already have, and a repeat of it is noise:

```c
if (EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = w) &&
    !EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = a, .other = b, .cap = n, .ci = EMBED_FALSE))
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = dst, .src = src, .bytes = n);
}
```

The library retains the free functions because a debugger, a linker map and an `nm` list show
them. A namespace built out of macros would show nothing.

With named members, nobody can call an entry with its arguments in the wrong order. An entry that
grows a member does not silently change what existing call sites do.

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

`EMBED_TABLE_STORAGE` is `static const`. `EMBED_UNUSED` lets an unreferenced table drop out of a
translation unit that does not use it.

## EMBED_TABLE_LAYOUT is the interesting part

Code reaches the table **by offset**. A positional initializer mis-wires silently when someone
inserts, removes or moves a member: the code still compiles, and `spat.from` calls something else.

`EMBED_TABLE_LAYOUT` pins it. It expands to a chain of `_Static_assert`s. They check that each named
member sits at its own loculus, in the order given, and that `sizeof` the struct is exactly that many
pointers. A member added and not listed, a member reordered, or padding between them all fail **at
the declaration**, at compile time.

That has one consequence for this documentation: **the order of the members is the layout**, and so the
reference must not sort them alphabetically. `SORT_MEMBER_DOCS` is `NO` in `docs/Doxyfile` for that
reason, and it is not a cosmetic value. A sort of `MemoriaOperorNs` would show loculus 0 as `chr`
when it is `cpy`.

## What the indirection costs

Nothing, when the code respects `const`.

Measured: the gcc compiler devirtualizes a call through a `static const <Mod>Ns` down to the inlined
body, with instructions identical to a direct call of the entry. It does **not** devirtualize the
same call through a non-const table, which becomes a real call with an 88-byte frame. The clang
compiler devirtualizes both. So the `const` in `EMBED_TABLE_STORAGE` is essential: it makes the two compilers agree.

Across module boundaries where the entry is not `EMBED_INLINE`, link-time optimization is what closes
the gap. See @ref concept_swar for the measurement.

## Modules with more than one table

`verbum_scrutor` has three: `lane`, `mask` and `word`. The count of its entries is past what
`EMBED_TABLE_LAYOUT` accepts for one table. The three tables split the entries by what they operate
on: a lane of a word, a mask of lanes, a whole word. That also reads better than one table of
everything.

`endian` is the other shape: two tables, `parva_extremitas` and `magna_extremitas`, over the same
`EndianNs` type. The table you call through chooses the byte order, and no argument does.

@ref ref_glossary lists every stem and the module it belongs to.
