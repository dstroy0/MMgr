# Your first region {#guide_first_region}

Borrowing a buffer, taking from both ends of it, and giving the interim back.

## Borrowing

```c
static uint8_t region[4096];

mmgr_confin c;
mmgr_confin_init(&c, region, sizeof region);
```

`mmgr_confin_init` takes the base and the length **on trust**. It cannot check them, so this is the
one line in your program where a mistake is unrecoverable — see @ref proj_security.

The buffer can be anything you own: a static array, a slice of a linker-placed section, a block from
a heap you already have. MMgr does not care where it came from, only that it outlives the confinium.

## Two ends

```c
uint8_t *cfg  = mmgr_confin_persist_capio(&c, 128, 8);   /* grows up from the base   */
uint8_t *tmp  = mmgr_confin_interim_capio(&c, 512, 8);   /* grows down from the top  */
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
size_t m = mmgr_confin_interim_mark(&c);       /* where the top is now */

uint8_t *a = mmgr_confin_interim_capio(&c, 256, 8);
uint8_t *b = mmgr_confin_interim_capio(&c, 256, 8);
/* ... work with a and b ... */

mmgr_confin_interim_reddo(&c, m);              /* both are gone, in one call */
```

This is the pattern for any bounded operation: mark on the way in, `reddo` on the way out, and the
interim cost of the operation is zero afterwards no matter how many takes it made.

@warning Nothing is reallocated and nothing moves, so `a` and `b` still point at readable memory
after the `reddo`. They are dead all the same, and the next `interim_capio` will hand that same
memory to someone else. A pointer that outlives its mark is the sharpest edge in this library.

## How much is left

```c
size_t left = mmgr_confin_octas_praesto(&c);   /* "bytes at hand" */
```

`octas_praesto` is **not** a release. It reports the gap still between the two ends. It is what you
log when a take returns `NULL` and you want to know by how much you missed.

## Sizing it honestly

Do not compute the size on paper. Measure it.

```c
size_t persist_peak = mmgr_confin_persist_used(&c);
size_t interim_peak = mmgr_confin_interim_used(&c);
```

Build the `checks` environment, run your real workload, and read those. `checks` compiles in the
contract asserts and points `MMGR_ASSERT` at something that aborts, so a precondition you violated
fails loudly instead of being a no-op:

```sh
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -R '_checks$' --output-on-failure
```

Then size the region to the peak plus whatever margin your failure policy wants. @ref concept_zero_heap
makes the argument for why this is the bill worth paying.

## Putting it together

```c
static uint8_t region[4096];

static mmgr_bool handle(mmgr_confin *c, const uint8_t *msg, size_t len)
{
    const size_t m = mmgr_confin_interim_mark(c);

    uint8_t *work = mmgr_confin_interim_capio(c, len, 8);
    if (work == NULL) {
        return MMGR_FALSE;                    /* no cleanup needed: nothing was taken */
    }

    memor.cpy(work, msg, len);
    size_t at = 0u;
    /* ... parse from work, bounded by len, at cursor `at` ... */

    mmgr_confin_interim_reddo(c, m);          /* one line, whatever happened above */
    return MMGR_TRUE;
}
```

Every exit path either returns before taking anything or rewinds to the mark. There is no
free-per-allocation to forget, which is most of the point.

Next: @ref concept_architecture for how pools, spans and rings sit on top of this.
