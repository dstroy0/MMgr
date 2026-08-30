# Your first prison site {#guide_first_region}

**Purpose:** Declare a prison site, allocate from both tiers of one of its cellblocks, and release
the temporary tier by mark.
**Scope:** `src/carceribus/carceribus.h`, `src/carceribus/carceribus.c`
**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-08-29

## Borrowing

```c
LocusCarcerum(prison, MMGR_MINIMUM_SECURITY(work, 2048), MMGR_MAXIMUM_SECURITY(keys, 2048));
```

One line declares the prison site, its cellblocks, their storage and their alignment. It is a
declaration, not a call: "Everything it emits is initialized data. Nothing runs at startup, and a
cellblock's first byte is the address of its own storage, which the linker resolves"
(`src/carceribus/carceribus.h:438-439`).

`MMGR_MINIMUM_SECURITY` and `MMGR_MAXIMUM_SECURITY` pick which guards hold a cellblock, and the
choice is settled at the declaration: "The warden is const, so nothing after the declaration can
change the level a cellblock runs at" (`src/carceribus/carceribus.h:19-20`). The two guard types
declare the same eight entries under different behavior, so `prison.keys` has no unzeroed release to
reach for and `prison.work` has no zeroing one
(`src/carceribus/carceribus.h:93-105`, `src/carceribus/carceribus.h:116-128`).

## Two tiers

```c
uint8_t *config = prison.work.persistent_buf_alloc(128);
uint8_t *working = prison.work.temporary_buf_alloc(512);
```

Both return the first byte of a cell `[RETURNS OWNERSHIP]`. The persistent tier takes it back through
`prison.work.persistent_buf_release`; the temporary tier takes a whole run back through
`prison.work.temporary_buf_release` or `prison.work.temporary_buf_reset`.

- **persistent** is for what lives as long as the cellblock: configuration, tables, buffers you fill
  once.
- **temporary** is the working space for one operation.

The two tiers grow toward each other out of the same gap, and a request the gap cannot meet returns
`NULL`: "Fails closed. A request the gap cannot meet moves no boundary at all"
(`src/carceribus/carceribus.c:380`).

```c
if (config == NULL || working == NULL) {
    return -1;
}
```

"The public entries are English and the internals are Latin, this filename included"
(`src/carceribus/carceribus.h:21`). @ref ref_glossary decodes the Latin you will meet inside.

## Releasing interim, by mark

Interim is a stack. You do not free a pointer, you rewind to a mark.

```c
size_t m = prison.work.temporary_buf_mark();
uint8_t *a = prison.work.temporary_buf_alloc(256);
uint8_t *b = prison.work.temporary_buf_alloc(256);

prison.work.temporary_buf_release(m);
```

This is the pattern for any bounded operation: mark on the way in, `reddo` on the way out, and the
interim cost of the operation is zero afterwards no matter how many takes it made.

@warning Nothing is reallocated and nothing moves, so `a` and `b` still point at readable memory
after the `reddo`. They are dead all the same, and the next `temporary_buf_alloc` will hand that same
memory to someone else. A pointer that outlives its mark is the sharpest edge in this library.

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
library's checks and selects the trapping `MMGR_ASSERT`, so a precondition you broke fails loudly
instead of being a no-op:

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R '_checks$' --output-on-failure
```

Then size the region to the peak plus whatever margin your failure policy wants. @ref concept_zero_heap
makes the argument for why this is the bill worth paying.

## Putting it together

```c
static mmgr_bool handle(const uint8_t *msg, size_t len)
{
    const size_t mark = prison.work.temporary_buf_mark();

    uint8_t *const buf = prison.work.temporary_buf_alloc(len);
    if (buf == NULL) {
        return MMGR_FALSE;
    }

    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = buf, .src = msg, .bytes = len);

    /* ... use buf ... */

    prison.work.temporary_buf_release(mark);
    return MMGR_TRUE;
}
```

Every exit path either returns before taking anything or rewinds to the mark. There is no
free-per-allocation to forget, which is most of the point.

Next: @ref concept_architecture for how pools, spans and rings sit on top of this.
