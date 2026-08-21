# Compilers, attributes and fallbacks {#ref_compiler_support}

Every compiler conditional in the library lives in one file, so adding a toolchain is one file to
read.

## The policy

`src/config/mmgr_compiler_directives.h` holds **every** `#if` that asks about a compiler. No other file in
the library tests for GNU, Clang, MSVC or a version. That is a rule, not a tendency, and it has two
consequences worth stating:

- Porting to a new toolchain means reading one header, not grepping nineteen modules.
- A module cannot quietly acquire a compiler dependency, because there is nowhere in it to put one.

Two further rules govern what is allowed in that file.

**A directive is a request the compiler may refuse.** Everything there has a fallback that still
compiles. Where the absence of the directive would silently break the layout, the fallback is
policed by a static assert at the point of use rather than trusted.

**A directive is not an operation.** Nothing in that file computes anything. The library asks for no
`__builtin_` at all — see @ref concept_swar for the measurements behind that.

## What is in it

| macro                                 | with the attribute                                    | without                         |
| ------------------------------------- | ----------------------------------------------------- | ------------------------------- |
| `MMGR_INLINE`                         | `static inline __attribute__((always_inline))`        | `static inline`                 |
| `MMGR_NS`                             | `static const`                                        | —                               |
| `MMGR_UNUSED`                         | `__attribute__((unused))`                             | empty                           |
| `MMGR_WEAK`                           | `__attribute__((weak))`                               | empty                           |
| `MMGR_ALIAS`                          | `__attribute__((may_alias))`                          | empty                           |
| `MMGR_ALIGN(n)`                       | `__attribute__((aligned(n)))`                         | empty                           |
| `MMGR_ENUM_PACKED`                    | `__attribute__((packed))`                             | empty — **asserted**, see below |
| `MMGR_BEGIN_DECLS` / `MMGR_END_DECLS` | `extern "C" {` / `}` under C++                        | empty under C                   |
| `MMGR_STATIC_ASSERT`                  | `_Static_assert`, `static_assert` or a negative array | —                               |

`MMGR_HAS_ATTRIBUTE` is the gate. It uses `__has_attribute` where that exists and falls back to a
compiler-and-version test where it does not.

## The one that cannot be allowed to fail quietly

`MMGR_ENUM_PACKED` is the exception to "every fallback still compiles and that is enough". An enum
that silently widens to `int` moves every field after it in every struct containing one, and this
library computes borrow offsets from those structs.

So there is a probe:

```c
typedef enum MMGR_ENUM_PACKED { MMGR_ENUM_PROBE_MIN = 0, MMGR_ENUM_PROBE_MAX = 1 } MmgrEnumProbe;

MMGR_STATIC_ASSERT(sizeof(MmgrEnumProbe) == 1,
    "MMGR_ENUM_PACKED is not honored here, so no enum keeps its declared width ...");
```

If the attribute was not honored, the build stops at the source of the problem with a message that
names the cause — instead of producing a binary whose struct offsets are wrong.

@note On TI toolchains, pass `--small_enum`. The assert message says so.

## MMGR_NS_LAYOUT

The dispatch tables are addressed by offset, so a positional initializer mis-wires silently when a
member moves. `MMGR_NS_LAYOUT(T, ...)` expands to a chain of `_Static_assert`s pinning each named
member to its own slot, in order, and pinning `sizeof(T)` to exactly that many pointers.

It is variadic up to 24 members via `MMGR_NARG` and `MMGR_NS_L1`…`MMGR_NS_L24`. `MMGR_NS_LAYOUT_OPEN`
is the variant for a table with trailing state beyond the function pointers —
`clarus_custodiae` uses it for its `internal` pointer.

This is why `SORT_MEMBER_DOCS` is `NO` in `docs/Doxyfile`: the documented order is the asserted
order. See @ref concept_ns_idiom.

## Byte order

`MMGR_HW_BIG_ENDIAN` derives from `__BYTE_ORDER__` where the compiler defines it. It exists so the
library can take the cheap path when a requested order matches the host — never so a caller can ask
for "whatever this machine does". Wire formats state their order. See @ref concept_width.

## Weak hardware hooks

The DMA module's hardware entries are `MMGR_WEAK`, so a board support file overrides them by simply
defining the same symbol. Without an override they are present and inert, which is what lets `dma`
build and its tests link on a host with no DMA controller.

## Adding a toolchain

1. Read `mmgr_compiler_directives.h` top to bottom. It is the whole surface.
2. Add the branch for your compiler to `MMGR_HAS_ATTRIBUTE` and to any macro whose spelling differs.
3. Build all five environments and run the suite. The static asserts in `mmgr_types.h` and the enum
   probe are the ones that will catch a wrong answer.
4. If the enum probe fails, find the toolchain's flag for packed enums before working around it.
