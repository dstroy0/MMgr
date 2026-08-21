# Quick start {#guide_quickstart}

Build it, run the suite, and write something that uses it.

## Build and test

```sh
git clone https://github.com/dstroy0/MMgr.git
cd MMgr
cmake -S . -B build -DMMGR_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

A clean run is 80 tests and takes a couple of seconds. That is 16 suites across five compile-time
environments plus the five environment suites themselves — one `cmake` invocation builds every
width. See @ref ref_environments.

Two suites are skipped unless you ask for them, and they say so rather than disappearing quietly:

```
-- MMgr: skipping test_dma (needs MMGR_ENABLE_DMA)
-- MMgr: skipping test_confinium_externum (needs MMGR_ENABLE_PSRAM_POOL)
```

## The smallest useful program

Storage comes from you. This is the whole shape of the library in twenty lines.

```c
#include "mmgr.h"
#include <stdio.h>

/* The one buffer. Static, sized at compile time, owned by the caller. */
static uint8_t region[4096];

int main(void)
{
    mmgr_confin c;
    mmgr_confin_init(&c, region, sizeof region);

    /* Take 256 bytes that live as long as the region does, 8-byte aligned. */
    uint8_t *store = mmgr_confin_persist_capio(&c, 256, 8);
    if (store == NULL) {
        return 1;                       /* it did not fit. That is the only failure mode. */
    }

    /* A span is a bounded view over storage it does not own. */
    mmgr_spat s = spat.from(store, 256);

    /* Build a string without printf and without a heap. */
    mmgr_verba b = verba.from(s);
    verba.put(&b, "bytes at hand: ");
    verba.u32(&b, (uint32_t)mmgr_confin_octas_praesto(&c));

    /* One check, at the end. verba latches, because a formatted number is as
       long as it turns out to be. */
    if (!verba.finish(&b)) {
        return 2;
    }

    printf("%.*s\n", (int)b.out.pos, (const char *)store);
    return 0;
}
```

Three things in that listing are the library's whole personality:

- **`sizeof region` is the only size decision**, and it is made at compile time.
- **`spat.from` borrows.** It allocates nothing and dies with `store`.
- **The error check is at the end**, not after every append, because the flag latches. See
  @ref ref_error_handling.

## Linking it

`src/` is the include root, so a consumer adds one include directory and reaches everything as
`<module>/<module>.h`, or takes the lot through `mmgr.h`.

```cmake
add_subdirectory(third_party/MMgr)
target_link_libraries(my_app PRIVATE mmgr_host)
```

`mmgr_host` is the aggregate for the host environment. @ref guide_install covers PlatformIO,
Arduino, and why there are no `install()` rules.

## Where to go from here

- @ref guide_first_region — persist against interim, and releasing by mark
- @ref concept_architecture — how a region becomes pools, spans and rings
- @ref nav_modules — the module you actually need
