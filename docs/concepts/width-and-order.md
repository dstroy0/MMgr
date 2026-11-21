# Width, alignment and byte order {#concept_width}

The three machine properties this library refuses to assume.

## The two types everything is built on

```c
embed_word   embed_index
```

They are separate by design. A word is what the scanner loads; an index is what addresses a
region. On a 64-bit host they are 64 and 32 bits, and code that conflates them works there and
breaks on the first machine where they differ. `idx16` is the environment that exists to reach that
case.

`embed_types.h` carries static asserts that police the combination. An index must address the
largest region the configuration allows, and the word must be at least as wide as the index. The
fixed-width types must be the widths they claim. They fire at compile time, in the build that got it
wrong, and name the pair.

## Enums must retain their declared width

An enum that silently becomes an `int` moves every field after it in every struct that contains
one, and this library addresses borrows by offset. Every offset computed from such a struct is then
wrong.

So `EMBED_ENUM_BYTE` declares every enum a struct holds, and it gives each one byte on every
compiler. Where the compiler can pin the enum itself to a byte, it uses `: unsigned char` or
`__attribute__((packed))`. Where it cannot, as on MSVC's C, it gives the constants with an
`unsigned char` name. A probe at the source asserts the width:

```c
EMBED_STATIC_ASSERT(sizeof(EmbedEnumProbe) == 1,
    "EMBED_ENUM_BYTE gave an enum more than one byte here, so no enum retains its declared width ...");
```

It fails only where a compiler accepts the packed attribute and ignores it. On TI toolchains, pass
`--small_enum`.

## Three access strategies, not one

`proximus_operor` exposes three families that look interchangeable and are not:

| infix    | strategy  | when                                                           |
| -------- | --------- | -------------------------------------------------------------- |
| `proxim` | unaligned | the address may be anything                                    |
| `aequus` | aligned   | you know the alignment holds                                   |
| `migro`  | may alias | the pointer may alias another live pointer of a different type |

A merge of them is a miscompile the compiler cannot report. An aligned load emitted for an address
without that alignment is a fault on some machines and a silently wrong value on others. A load
without the may-alias mark lets the optimizer reorder it against a store it genuinely conflicts with.

This is also why `tools/dev_env/names.tsv` retains three infixes for one module and does not fold
them onto the stem. One stem for `rawmemcpy`'s three merges seven symbol pairs, the aligned and
unaligned load among them.

## Byte order is stated, never inherited

```c
endian.wr32be(p, v);    /* big-endian, on every machine */
uint32_t v = endian.rd32le(p);
```

There is no `endian.wr32host()`. A wire format has a byte order, and the host's order is an
implementation detail of the host. Code that writes "native" order to a wire has a bug that only
appears when the other end is different.

`EMBED_BIG_ENDIAN` derives from `__BYTE_ORDER__`. It exists so the library can pick the cheap path
when the requested order happens to match the host. It is not there for callers to ask for "whatever
this machine does".

## Alignment of an allocation

```c
uint8_t *p = prison.work.persistent_buf_alloc(256, reader);
```

An allocation does not name an alignment, because there is only one. Every cell comes back aligned
to `MMGR_CARCER_ALIGN`: the larger of `sizeof(embed_word)` and the alignment of the atomic size word.
It derives from the two types and is not a written number. Every cell carries an atomic size word at
its header and at its foot. A grain narrower than that word would put the foot off its alignment. At
a 16-bit or 32-bit word the grain is the atomic word's; at a 64-bit word the two are the same.

`locus_carcerum.h` asserts that `MMGR_CARCER_ALIGN` is a power of two. An offset rounds with a mask,
and a mask lands on a multiple only for a power of two. It also asserts that `MMGR_ALIGN_BYTES`, the
alignment a pool declaration puts on its storage, is a multiple of it. `mmgr.h` asserts that
`MMGR_ALIGN_BYTES` is a power of two. All three stop the build.

The width costs bytes twice over. A request rounds up to a whole word before the cellblock carves
it. Every cell also carries three words beside its payload, each rounded to the same alignment. Its
size sits ahead of it, and behind it sit its owner and its tail, the cell's own offset. On a 64-bit
build that is 24 bytes per cell; on a 16-bit build it is 6. A cellblock full of small cells pays that
per cell, and so in a 4 KB pool the width deserves thought.

## What to do with all this

Build all five environments. The default build already does:

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
```

`word16` reaches the scan tails four times sooner than `host`. `idx16` reaches the width asserts.
Between them they cover the two mistakes this page is about. See @ref ref_environments.

@note Doxygen produces the generated reference on this site at one configuration, 64-bit word and
32-bit index. Doxygen has to resolve `embed_word` to a single concrete type. The other environments
differ only in these typedefs.
