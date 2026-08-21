# The dispatch table idiom {#concept_ns_idiom}

Why a call reads `spat.from(buf, cap)` and what that costs.

## Two spellings, one function

Every module declares plain C functions with a long, prefixed name, and also gathers them into a
`static const` struct of function pointers named for a short Latin stem.

```c
mmgr_spat s = mmgr_spat_from(buf, cap);   /* the free function */
mmgr_spat s = spat.from(buf, cap);        /* the same thing, through the table */
```

Both are public and both are documented. The table is what call sites use, because at a call site
the module is context you already have and repeating it is noise:

```c
if (scrut.has_zero(w) && !cellul.eq(a, b, n, MMGR_FALSE)) {
    memor.cpy(dst, src, n);
}
```

against

```c
if (mmgr_scrut_has_zero(w) && !mmgr_cellul_eq(a, b, n, MMGR_FALSE)) {
    mmgr_memor_cpy(dst, src, n);
}
```

The second is not clearer, it is just longer. The library keeps both because the free function is
what a debugger, a linker map and a `nm` listing show you, and a namespace built out of macros would
show nothing.

## How it is declared

Three pieces, in every module header:

```c
typedef struct
{
    mmgr_spat (*from)(uint8_t *p, size_t cap);
    mmgr_bool (*ok)(mmgr_spat s);
    /* ... */
} SpatiumNs;
MMGR_NS_LAYOUT(SpatiumNs, from, ok, has_storage, len, room, reset, /* ... */);

MMGR_NS SpatiumNs spat MMGR_UNUSED = {.from = mmgr_spat_from, .ok = mmgr_spat_ok, /* ... */};
```

`MMGR_NS` is `static const`. `MMGR_UNUSED` is what lets an unreferenced table drop out of a
translation unit that does not use it.

## MMGR_NS_LAYOUT is the interesting part

The table is addressed **by offset**. A positional initializer mis-wires silently when a member is
inserted, removed or moved — the code still compiles, and `spat.from` calls something else.

`MMGR_NS_LAYOUT` pins it. It expands to a chain of `_Static_assert`s checking that each named member
sits at its own loculus, in the order given, and that `sizeof` the struct is exactly that many
pointers. A member added and not listed, a member reordered, or padding appearing between them all
fail **at the declaration**, at compile time.

That has one consequence for this documentation: **the order of the members is the layout**, so the
reference must not sort them alphabetically. `SORT_MEMBER_DOCS` is `NO` in `docs/Doxyfile` for that
reason, and it is not a cosmetic setting — sorting would show loculus 0 of `SpatiumNs` as `after` when
it is `from`.

## What the indirection costs

Nothing, when `const` is respected.

Measured: gcc devirtualizes a call through a `static const <Mod>Ns` down to the inlined body —
identical instructions to calling the entry directly — and does **not** devirtualize the same call
through a non-const table, which becomes a real call with an 88-byte frame. clang devirtualizes
both. So the `const` in `MMGR_NS` is load-bearing: it is what makes the two compilers agree.

Across module boundaries where the entry is not `MMGR_INLINE`, link-time optimization is what closes
the gap. See @ref concept_swar for the measurement.

## Modules without a table

`confinium` has none, deliberately. Its surface is a set of verbs on an explicit `mmgr_confin *`
rather than a set of free functions over a global, so a table would add a level of indirection
without removing a word from the call site.

`cellularum_laboro` is the opposite case: it exposes **only** the table. `cellul` is declared
`extern const CellularumLaboroNs` and there are no free functions in the header at all.

Both are noted in their guides. @ref ref_glossary lists every stem and the module it belongs to.
