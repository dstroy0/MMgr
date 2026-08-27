# Your first region {#guide_first_region}

Borrowing a buffer, taking from both ends of it, and giving the interim back.

## Borrowing

```c
mmgr_carcer_init(g_ram, 4096u, MMGR_POOL(g_scratch, 4096u));

CarcerCtx *const pool = MMGR_CARCER_POOL(g_ram, g_scratch);
```

`mmgr_carcer_init` is a declaration, not a call. It emits the storage, one @ref CarcerCtx per pool,
and static asserts that the region has an address and an extent and that the pools fit inside it.
Nothing runs at startup, and a region that does not add up fails to compile.

Each `MMGR_POOL` names a pool and its size. They are laid out back to back from the base of the
region, so the second starts where the first ends.

## Two ends

```c
uint8_t *cfg  = mmgr_carcer_persist_capio(&c, 128, 8);   uint8_t *tmp  = mmgr_carcer_interim_capio(&c, 512, 8);   ```

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
size_t m = mmgr_carcer_interim_mark(&c);       
uint8_t *a = mmgr_carcer_interim_capio(&c, 256, 8);
uint8_t *b = mmgr_carcer_interim_capio(&c, 256, 8);

mmgr_carcer_interim_reddo(&c, m);              ```

This is the pattern for any bounded operation: mark on the way in, `reddo` on the way out, and the
interim cost of the operation is zero afterwards no matter how many takes it made.

@warning Nothing is reallocated and nothing moves, so `a` and `b` still point at readable memory
after the `reddo`. They are dead all the same, and the next `interim_capio` will hand that same
memory to someone else. A pointer that outlives its mark is the sharpest edge in this library.

## How much is left

```c
size_t left = mmgr_carcer_octas_praesto(&c);   ```

`octas_praesto` is **not** a release. It reports the gap still between the two ends. It is what you
log when a take returns `NULL` and you want to know by how much you missed.

## Sizing it honestly

Do not compute the size on paper. Measure it.

```c
const size_t persist_now = MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = pool);
const size_t left       = MMGR_CALL(carcer.octas_praesto, CarcerCfg, .pool = pool);
const size_t peak       = pool->hw;   /* MMGR_ENABLE_HW_MEM_CAPACITY_CB only */
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
static mmgr_bool handle(CarcerCtx *pool, const uint8_t *msg, size_t len)
{
    const size_t mark = MMGR_CALL(carcer.interim_mark, CarcerCfg, .pool = pool);

    uint8_t *const work = MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = pool, .size = len);
    if (work == NULL) {
        return MMGR_FALSE;
    }

    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = work, .src = msg, .bytes = len);

    /* ... use work ... */

    MMGR_CALL(carcer.interim_reddo, CarcerCfg, .pool = pool, .size = mark);
    return MMGR_TRUE;
}
```

Every exit path either returns before taking anything or rewinds to the mark. There is no
free-per-allocation to forget, which is most of the point.

Next: @ref concept_architecture for how pools, spans and rings sit on top of this.
