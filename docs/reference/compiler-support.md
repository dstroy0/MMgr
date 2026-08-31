# Compilers, attributes and fallbacks {#ref_compiler_support}

Every compiler conditional in the library lives in one file, so adding a toolchain is one file to
read.

## The policy

`deps/embedded_types/include/embed_compiler_directives.h` holds **every** `#if` that asks about a
compiler. No other file in the library tests for GNU, Clang or a version. That is a rule, not a
tendency, and it has two consequences worth stating:

- Porting to a new toolchain means reading one header, not grepping twenty modules.
- A module cannot quietly acquire a compiler dependency, because there is nowhere in it to put one.

That header belongs to `embedded_types`, a separate library MMgr consumes. The macros below carry its
`EMBED_` prefix because they are its, and the header states the reason for using them directly:
"Do not copy them into your own tree. Copies drift when this header changes."

Two further rules govern what is allowed in that file.

**A directive is a request the compiler may refuse.** Everything there has a fallback that still
compiles. Where the absence of the directive would silently break the layout, the fallback is
policed by a static assert at the point of use rather than trusted.

**A directive is not an operation.** Nothing in that file computes anything. The library asks for no
`__builtin_` at all — see @ref concept_swar for the measurements behind that.

## What is in it

| macro                                      | with the attribute                             | without                         |
| ------------------------------------------ | ---------------------------------------------- | ------------------------------- |
| `EMBED_INLINE`                             | `static inline __attribute__((always_inline))` | `static inline`                 |
| `EMBED_TABLE_STORAGE`                      | `static const`                                 | —                               |
| `EMBED_UNUSED`                             | `__attribute__((unused))`                      | empty                           |
| `EMBED_WEAK`                               | `__attribute__((weak))`                        | empty                           |
| `EMBED_ALIAS`                              | `__attribute__((may_alias))`                   | empty                           |
| `EMBED_ALIGN(bytes_)`                      | `__attribute__((aligned(bytes_)))`             | empty                           |
| `EMBED_FLATTEN`                            | `__attribute__((flatten))`                     | empty                           |
| `EMBED_ENUM_PACKED`                        | `__attribute__((packed))`                      | empty — **asserted**, see below |
| `EMBED_BEGIN_DECLS` / `EMBED_END_DECLS`    | `extern "C" {` / `}` under C++                 | empty under C                   |
| `EMBED_STATIC_ASSERT`                      | `static_assert` or `_Static_assert`            | `#error` below C11              |

MMgr adds three of its own, in `include/mmgr.h`, for what the compiler offers and this library needs:
`MMGR_ERROR_ATTR` puts our own text on a reference that must not link, `MMGR_ALLOC_SIZE` states which
argument gives the extent of what an allocation returns, and `MMGR_EXTRAM_ATTR` is the placement the
port fills in.

`EMBED_HAS_ATTRIBUTE` is the gate. It uses `__has_attribute` where that exists and falls back to
`EMBED_GNU_ATTRIBUTES` where it does not.

## The one that cannot be allowed to fail quietly

`EMBED_ENUM_PACKED` is the exception to "every fallback still compiles and that is enough". An enum
that silently widens to `int` moves every field after it in every struct containing one, and this
library computes offsets from those structs.

So there is a probe, in `embed_types.h`:

```c
typedef enum EMBED_ENUM_PACKED { EMBED_ENUM_PROBE_MIN = 0, EMBED_ENUM_PROBE_MAX = 255 } EmbedEnumProbe;

EMBED_STATIC_ASSERT(sizeof(EmbedEnumProbe) == 1,
    "EMBED_ENUM_PACKED is not honored here, so no enum keeps its declared width ...");
```

If the attribute was not honored, the build stops at the source of the problem with a message that
names the cause — instead of producing a binary whose struct offsets are wrong.

@note On TI toolchains, pass `--small_enum`. The assert message says so.

## EMBED_TABLE_LAYOUT

The dispatch tables are addressed by offset, so a positional initializer mis-wires silently when a
member moves. `EMBED_TABLE_LAYOUT(Table_, ...)` expands to a chain of static assertions pinning each
named member to its own slot, in order, and pinning `sizeof(Table_)` to exactly that many pointers.

It is variadic up to 24 members via `EMBED_NARG` and `EMBED_TABLE_SLOTS_1`…`EMBED_TABLE_SLOTS_24`.
That 24 is also the ceiling on a dispatch table: `verbum_scrutor` splits into three tables partly
because of it.

Every table is pure function pointers, so `sizeof(Table_)` against
`EMBED_NARG * EMBED_FUNCTION_POINTER_BYTES` is the whole assertion. A table carrying trailing state
would need its own variant testing `offsetof` instead.

This is why `SORT_MEMBER_DOCS` is `NO` in `docs/Doxyfile`: the documented order is the asserted
order. See @ref concept_ns_idiom.

## Byte order

`EMBED_BIG_ENDIAN` derives from `__BYTE_ORDER__` where the compiler defines it, and falls to 0 where
it does not. It exists so the library can take the cheap path when a requested order matches the host
— never so a caller can ask for "whatever this machine does". Wire formats state their order. See
@ref concept_width.

## Weak hardware hooks

The DMA module's hardware entries are `EMBED_WEAK`, so a board support file overrides them by
defining the same symbol. Without an override they are present and refuse every request, which is
what lets `memoriam_praetereo` build and its tests link on a host with no DMA controller.

## Adding a toolchain

1. Read `embed_compiler_directives.h` top to bottom. It is the whole surface.
2. Add the branch for your compiler to `EMBED_HAS_ATTRIBUTE` and to any macro whose spelling differs.
3. Build all five environments and run the suite. The static asserts in `embed_types.h` and the enum
   probe are the ones that will catch a wrong answer.
4. If the enum probe fails, find the toolchain's flag for packed enums before working around it.
