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

A clean run is 240 tests and takes a few seconds. Every suite is built once per compile-time
environment, and there are five of them, so one `cmake` invocation builds every width. See
@ref ref_environments.

Three suites are skipped unless you ask for them, and the configure says so instead of dropping them
quietly:

```
-- MMgr: 3 suites NOT built, their capability is off:
-- MMgr:   test_memoria_externa (MMGR_ENABLE_EXTRAM=OFF)
-- MMgr:   test_memoriam_praetereo (MMGR_ENABLE_DMA=OFF)
-- MMgr:   test_memoria_externa_accuracy (MMGR_ENABLE_EXTRAM=OFF)
```

## The smallest useful program

Storage comes from you. This is the whole shape of the library in twenty lines.

```c
#include "mmgr.h"
#include <stdio.h>

/* A pool of bytes, then one prison site holding one cellblock over it. Both are declarations, and
   both are settled at compile time. */
ParsMemoriaeInternae(work, 4096);
LocusCarcerum(prison, MMGR_MINIMUM_SECURITY(work));

int main(void)
{

    char *const store = prison.work.persistent_buf_alloc(256);
    if (store == NULL) {
        return 1;
    }

    const size_t left = prison.work.buf_available();

    size_t at = 0;
    at = EMBED_CALL(verba.put, VerbaCfg, .out = store, .cap = 256u, .at = at,
                   .text = "bytes at hand: ");
    at = EMBED_CALL(verba.uint, VerbaCfg, .out = store, .cap = 256u, .at = at, .val = (uint32_t)left);

    const size_t len = EMBED_CALL(verba.finish, VerbaCfg, .out = store, .cap = 256u, .at = at);
    if (len == 0u) {
        return 2;
    }

    printf("%.*s\n", (int)len, store);
    return 0;
}
```

Three things in that listing are the library's whole personality:

- **The 4096 is the only size decision**, and it is made at compile time, in the pool declaration.
  `ParsMemoriaeInternae` emits the storage and its alignment; `LocusCarcerum` emits each cellblock's
  state and the static asserts that the pool is a legal size for one. Neither is a call, and nothing
  runs at startup.
- **`persistent_buf_alloc` borrows.** It allocates nothing from the heap — it hands back part of the
  cellblock's own storage and moves a boundary, and what it returns dies when that tier unwinds.
- **The error check is at the end**, not after every write, because a writer with no room returns
  `cap` and every later writer does the same, so `finish` reports it. See @ref ref_error_handling.

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
