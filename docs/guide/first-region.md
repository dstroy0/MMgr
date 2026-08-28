# Your first region {#guide_first_region}

Borrowing a buffer, taking from both ends of it, and giving the interim back.

## Borrowing

```c
Carceribus(prison, MMGR_SOLUTA(work, 2048), MMGR_SECURA(keys, 2048));
```

One line declares the region, its pools, their storage and their alignment. It is a declaration,
not a call: everything it emits is initialized data, so nothing runs at startup and a pool's
first byte is an address the linker resolved.

`MMGR_SOLUTA` is the loose watch and `MMGR_SECURA` the close one. The watch is fixed here and
cannot be changed later: `prison.keys` has no unwiped release to reach for, and `prison.work` has
no wiping one.

## Two ends

```c
uint8_t *cfg = prison.work.persist_capio(128);
uint8_t *tmp = prison.work.interim_capio(512);
```

- **persist** is for what lives as long as the region: configuration, tables, buffers you fill once.
- **interim** is the working space for one operation.

They grow toward each other and a take that would cross fails, returning `NULL`. Check it — that is
the only failure mode the allocator has.

```c
if (cfg == NULL || tmp == NULL) {
    return -1;
}
```

`capio` is _take_. `reddo` is _give back_. The verbs are Latin because the English ones are already
spoken for by libc; @ref ref_glossary decodes the rest.

## Releasing interim, by mark

Interim is a stack. You do not free a pointer, you rewind to a mark.

```c
size_t m = prison.work.interim_mark();
uint8_t *a = prison.work.interim_capio(256);
uint8_t *b = prison.work.interim_capio(256);

prison.work.interim_reddo(m);
```

This is the pattern for any bounded operation: mark on the way in, `reddo` on the way out, and the
interim cost of the operation is zero afterwards no matter how many takes it made.

@warning Nothing is reallocated and nothing moves, so `a` and `b` still point at readable memory
after the `reddo`. They are dead all the same, and the next `interim_capio` will hand that same
memory to someone else. A pointer that outlives its mark is the sharpest edge in this library.

## How much is left

```c
size_t left = prison.work.octas_praesto();
```

`octas_praesto` is **not** a release. It reports the gap still between the two ends. It is what you
log when a take returns `NULL` and you want to know by how much you missed.

## Sizing it honestly

Do not compute the size on paper. Measure it.

```c
const size_t left = prison.work.octas_praesto();
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
    const size_t mark = prison.work.interim_mark();

    uint8_t *const buf = prison.work.interim_capio(len);
    if (buf == NULL) {
        return MMGR_FALSE;
    }

    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = buf, .src = msg, .bytes = len);

    /* ... use buf ... */

    prison.work.interim_reddo(mark);
    return MMGR_TRUE;
}
```

Every exit path either returns before taking anything or rewinds to the mark. There is no
free-per-allocation to forget, which is most of the point.

Next: @ref concept_architecture for how pools, spans and rings sit on top of this.
