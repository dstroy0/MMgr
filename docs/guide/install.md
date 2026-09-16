# Adding MMgr to a build {#guide_install}

Four ways in, and one thing the library deliberately does not offer.

## CMake

```cmake
add_subdirectory(third_party/MMgr)
target_link_libraries(my_app PRIVATE mmgr_host)
```

`mmgr_host` is an interface target that links every module built at this machine's natural widths.
There is one aggregate per environment — `mmgr_host`, `mmgr_word32`, `mmgr_word16`, `mmgr_idx16`,
`mmgr_checks` — and linking one of the others is how you build your application against a narrower
width without owning the hardware. See @ref ref_environments.

Turn the tests off in a consuming build:

```cmake
set(MMGR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/MMgr)
```

## Include paths

Two roots: `include/` carries the umbrella header, `src/` carries the modules. Both come with the
CMake target, so a consumer adds neither by hand.

```c
#include "mmgr.h"                    /* the pool declarations, the guards, the widths */
#include "spatium/spatium.h"         /* just spans */
```

The path is part of the interface — the generated reference prints the include line for every entity
for that reason.

## PlatformIO

```ini
[env:myboard]
lib_deps = https://github.com/dstroy0/MMgr.git
build_flags = -std=gnu11
```

`library.json` sets `srcDir` to `src`, unflags `-std=gnu99` and adds `-std=gnu11`, so the C11
requirement is carried by the manifest rather than by your build file.

## Arduino

Download the repository as a zip and use **Sketch → Include Library → Add .ZIP Library**, or clone
it into your `libraries/` directory. `library.properties` declares it, and `keywords.txt` gives the
editor the module stems so `spat` and `scrut` highlight.

## Configuring it

Every knob is a preprocessor define with a default, so you set only what you are changing:

```cmake
target_compile_definitions(my_app PRIVATE
    MMGR_ALIGN_BYTES=32
    MMGR_RING_LOCULI=16
)
```

Sizes are not among them. A pool states its own extent at its declaration, so there is no build knob
that sizes one.

@ref ref_configuration lists all of them.

## Why there are no install() rules

There is no `install()`, no `export()`, no `MMgrConfig.cmake` and no `find_package` support, and
that is deliberate rather than unfinished.

The ABI is a function of the compile-time widths. A `libmmgr.a` built at `EMBED_WORD_BITS=64` and a
consumer compiled at `EMBED_WORD_BITS=32` disagree about the size of `embed_word`, therefore about
the layout of every struct containing one, therefore about every offset the library computes — and
nothing in the link would catch it. `embed_types.h` is full of static asserts to police exactly this
class of mistake, and they only fire when the library and its consumer are compiled together.

Shipping a binary would make that mismatch possible while making it invisible. Consuming the source
makes it impossible. That is the trade, and for a library this size — twenty small modules, no
external dependencies — compiling it into your build costs almost nothing.

If you need a prebuilt artifact anyway, build the aggregate you want and vendor it together with the
exact width definitions it was built against. Do not ship one without the other.
