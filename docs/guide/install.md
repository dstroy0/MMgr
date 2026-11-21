# Adding MMgr to a build {#guide_install}

Four ways in, and one thing the library deliberately does not offer.

## CMake

```cmake
add_subdirectory(third_party/MMgr)
target_link_libraries(my_app PRIVATE mmgr_host)
```

`mmgr_host` is an interface target that links every module built at this machine's natural widths.
There is one aggregate per environment: `mmgr_host`, `mmgr_word32`, `mmgr_word16`, `mmgr_idx16`,
`mmgr_checks`. Link one of the others to build your application against a narrower width, and you
need no such hardware. See @ref ref_environments.

Turn the tests off in the build of a consumer:

```cmake
set(MMGR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/MMgr)
```

## Include paths

Two roots: `include/` carries the umbrella header, `src/` carries the modules. Both come with the
CMake target. A consumer adds neither by hand.

```c
#include "mmgr.h"                    /* the pool declarations, the guards, the widths */
#include "spatium/spatium.h"         /* just spans */
```

The path is part of the interface: the generated reference prints the include line for every entity
for that reason.

## PlatformIO

```ini
[env:myboard]
lib_deps = https://github.com/dstroy0/MMgr.git
build_flags = -std=gnu11
```

`library.json` sets `srcDir` to `src`, unflags `-std=gnu99` and adds `-std=gnu11`, and so the
manifest carries the C11 requirement and not your build file.

## Arduino

Download the repository as a zip and use **Sketch → Include Library → Add .ZIP Library**, or clone
it into your `libraries/` directory. `library.properties` declares it, and `keywords.txt` gives the
editor the module stems so `spat` and `scrut` highlight.

## Configuring it

Every knob is a preprocessor define with a default, and so you set only what you change:

```cmake
target_compile_definitions(my_app PRIVATE
    MMGR_ALIGN_BYTES=32
    MMGR_RING_LOCULI=16
)
```

Sizes are not among them. A pool states its own extent at its declaration, and so there is no build knob
that sizes one.

@ref ref_configuration lists all of them.

## Why there are no install() rules

There is no `install()`, no `export()`, no `MMgrConfig.cmake` and no `find_package` support, and
by design, and complete.

The ABI is a function of the compile-time widths. Take a `libmmgr.a` built at `EMBED_WORD_BITS=64`
and a consumer compiled at `EMBED_WORD_BITS=32`. They disagree about the size of `embed_word`, and
so about the layout of every struct that contains one. They then disagree about every offset the
library computes, and nothing in the link catches it. `embed_types.h` is full of static asserts to
police exactly this class of mistake. They fire only when one build compiles the library and its
consumer together.

A shipped binary makes that mismatch possible and hides it. A consumer that compiles the source
cannot have it. That is the trade. For a library this size, twenty small modules and one
dependency, embedded_types, a compile into your build costs almost nothing.

If you need a prebuilt artifact anyway, build the aggregate you want and vendor it together with the
exact width definitions of its build. Do not ship one without the other.
