# Width, alignment and byte order {#concept_width}

The three machine properties this library refuses to assume.

## The two types everything is built on

```c
mmgr_word   mmgr_idx    ```

They are separate on purpose. A word is what the scanner loads; an index is what addresses a
region. On a 64-bit host they are 64 and 32 bits, and code that conflates them works there and
breaks on the first machine where they differ. `idx16` is the environment that exists to reach that
case.

`mmgr_types.h` carries static asserts policing the combination — that an index can address the
largest region the configuration allows, that the word is at least as wide as the index, that the
fixed-width types are the widths they claim. They fire at compile time, in the build that got it
wrong, naming the pairing.

## Enums must keep their declared width

An enum that silently becomes an `int` moves every field after it in every struct containing one —
and this library addresses borrows by offset, so every offset computed from such a struct is then
wrong.

That is not detectable at the use site, so it is asserted at the source:

```c
MMGR_STATIC_ASSERT(sizeof(MmgrEnumProbe) == 1,
    "MMGR_ENUM_PACKED is not honored here, so no enum keeps its declared width ...");
```

`MMGR_ENUM_PACKED` is `__attribute__((packed))` where the attribute exists and empty where it does
not — and the probe is what turns "the fallback was taken" from a silent behavior change into a
build failure. On TI toolchains, pass `--small_enum`.

## Three access strategies, not one

`proximus_operor` exposes three families that look interchangeable and are not:

| infix    | strategy  | when                                                           |
| -------- | --------- | -------------------------------------------------------------- |
| `proxim` | unaligned | the address may be anything                                    |
| `aequus` | aligned   | you know the alignment holds                                   |
| `migro`  | may alias | the pointer may alias another live pointer of a different type |

Merging them is a miscompile the compiler cannot report. An aligned load emitted for an address that
is not aligned is a fault on some machines and a silently wrong value on others; a load without the
may-alias marking lets the optimizer reorder it against a store it genuinely conflicts with.

This is also why `tools/dev_env/names.tsv` keeps three infixes for one module rather than collapsing
them onto the stem: collapsing `rawmemcpy`'s three onto one merged seven symbol pairs, including the
aligned and unaligned load.

## Byte order is stated, never inherited

```c
endian.wr32be(p, v);    /* big-endian, on every machine */
uint32_t v = endian.rd32le(p);
```

There is no `endian.wr32host()`. A wire format has a byte order; the host's order is an
implementation detail of the host, and code that writes "native" order to a wire has a bug that only
appears when the other end is different.

`MMGR_HW_BIG_ENDIAN` derives from `__BYTE_ORDER__` and exists so the library can pick the cheap path
when the requested order happens to match the host — not so callers can ask for "whatever this
machine does".

## Alignment of a take

```c
uint8_t *p = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = pool, .size = 256u);
```

A take does not name an alignment, because there is only one. Every tenancy comes back aligned to
`MMGR_CARCER_ALIGN`, which is `sizeof(mmgr_word)` — derived from the width rather than written as a
number, so a build at another width gets the alignment that width needs.

Three static asserts in `carceribus.h` hold it together: `MMGR_CARCER_ALIGN` is a power of two, so an
offset rounds by masking; `MMGR_ALIGN_BYTES` is one too; and `MMGR_ALIGN_BYTES >= MMGR_CARCER_ALIGN`,
so a region cannot be aligned less than the tenancies it hands out. All three fail the build, not a
test run.

The width costs bytes twice over. A request rounds up to a whole word before it is carved, and every
block carries a header of two `size_t` ahead of its payload, itself rounded to the same alignment. On
a 64-bit build that is 16 bytes of header per tenancy; on a 16-bit build it is 4. A region full of
small takes pays that overhead per take, which is what makes the width worth thinking about in a 4 KB
region.

## What to do with all this

Build all five environments, which is what the default build already does:

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
```

`word16` reaches the scan tails four times sooner than `host`. `idx16` reaches the width asserts.
Between them they cover the two mistakes this page is about. See @ref ref_environments.

@note The generated reference on this site is produced at one configuration — 64-bit word, 32-bit
index — because Doxygen has to resolve `mmgr_word` to a single concrete type. The other environments
differ only in these typedefs.
