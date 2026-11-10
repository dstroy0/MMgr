# Your first prison site {#guide_first_region}

**Purpose:** Declare a prison site, allocate from both tiers of one of its cellblocks, and give each
cell back.
**Scope:** `include/mmgr.h`, `src/locus_carcerum/locus_carcerum.h`, `src/locus_carcerum/locus_carcerum.c`
**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-08-29

## Borrowing

```c
ParsMemoriaeInternae(work, 2048);
ParsMemoriaeInternae(keys, 2048);

LocusCarcerum(prison, MMGR_MINIMUM_SECURITY(work), MMGR_MAXIMUM_SECURITY(keys));
```

The first two lines declare the storage and its alignment. The third dresses each pool as a
cellblock and emits the site. All three are declarations and none is a call: everything they emit is
initialized data, nothing runs at startup, and a cellblock's first byte is the address of the pool
it was declared over, which the linker resolves.

A cellblock is reached by the name of its pool, and `prison.work` is the cellblock over `work`. Pool
names stand for one region each, and no two cellblocks anywhere in a translation unit can
share a name.

`MMGR_MINIMUM_SECURITY` and `MMGR_MAXIMUM_SECURITY` pick which guards hold a cellblock, and the
choice is settled at the declaration: "The warden is const, so nothing after the declaration can
change the level a cellblock runs at" (`src/locus_carcerum/locus_carcerum.h:24`). The two guard types
declare the same sixteen entries under different behavior, and `prison.keys` has no unzeroed release to
reach for and `prison.work` has no zeroing one
(`src/locus_carcerum/locus_carcerum.h:436`, `src/locus_carcerum/locus_carcerum.h:477`).

## Two tiers

```c
#define PROGRAM_OWNERS(owner_) owner_(reader, "reader")
MMGR_OWNERS(PROGRAM_OWNERS)

uint8_t *config = prison.work.persistent_buf_alloc(128, reader);
uint8_t *working = prison.work.temporary_buf_alloc(512, reader);
```

Every take names the owner its bytes are counted to, a const pointer into the program's owner table that
MMGR_OWNERS declares once, as data, beside the pools. Both return the first byte of a cell `[RETURNS OWNERSHIP]`.
Either tier takes it back through `prison.work.persistent_buf_release`: the cell's offset says which tier it lies in.

- **persistent** is for what lives as long as the cellblock: configuration, tables, buffers you fill
  once.
- **temporary** is the working space for one operation. Give the cell back when the operation ends.
  See @ref concept_ownership.

The two tiers grow toward each other out of the same gap, and a request neither the gap nor a free
run of the persistent tier can meet returns `NULL`: a bump the gap cannot meet records its bytes as the tier's overrun, which the exclusive
path that follows moves no boundary either.

```c
if (config == NULL || working == NULL) {
    return -1;
}
```

"The public entries are English and the internals are Latin, this filename included"
(`src/locus_carcerum/locus_carcerum.h:25`). @ref ref_glossary decodes the Latin you will meet inside.

## Releasing interim

Interim is persist with its base at the top. A cell is given back by itself, in any order.

```c
uint8_t *a = prison.work.temporary_buf_alloc(256, reader);
uint8_t *b = prison.work.temporary_buf_alloc(256, reader);

prison.work.persistent_buf_release(a, reader);
prison.work.persistent_buf_release(b, reader);
```

This is the pattern for any bounded operation: take on the way in, release on the way out. The interim
cost of the operation is zero afterwards no matter how many takes it made. A cell released below the top
stays free in the tier. It stays until the top comes back over it or a take the gap cannot meet merges it.

@warning Nothing is reallocated and nothing moves, and `a` and `b` still point at readable memory
after their release. They are dead all the same, and the next `temporary_buf_alloc` will hand that same
memory to someone else.

## How much is left

```c
size_t left = prison.work.buf_available();
```

`buf_available` is **not** a release. It reports the gap still between the two ends. It is what you
log when a take returns `NULL` and you want to know by how much you missed.

## Sizing it honestly

Do not compute the size on paper. Measure it.

```c
const size_t left = prison.work.buf_available();
```

Build the `checks` environment, run your real workload, and read those. `checks` compiles in the
library's checks and selects the trapping `MMGR_ASSERT`. A precondition you broke fails loudly
instead of being a no-op:

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R '_checks$' --output-on-failure
```

Then size the region to the peak plus whatever margin your failure policy wants. @ref concept_zero_heap
argues why this is the bill worth paying.

## Putting it together

```c
static embed_bool handle(const uint8_t *msg, size_t len)
{
    uint8_t *const buf = prison.work.temporary_buf_alloc(len, reader);
    if (buf == NULL) {
        return EMBED_FALSE;
    }

    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = buf, .src = msg, .bytes = len);

    /* ... use buf ... */

    prison.work.persistent_buf_release(buf, reader);
    return EMBED_TRUE;
}
```

Every exit path either returns before taking anything or gives back what it took.

Next: @ref concept_architecture for how pools, spans and rings sit on top of this.
