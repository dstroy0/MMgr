// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_COMPILER_DIRECTIVES_H
#define MMGR_COMPILER_DIRECTIVES_H

#include <stddef.h>

/**
 * @file mmgr_compiler_directives.h
 * @brief Every compiler conditional in the library. If a port miscompiles, look here first.
 *
 * Nothing else in src/ may spell __attribute__, _Pragma, __builtin_, __GNUC__, __clang__ or
 * __BYTE_ORDER__. Any that reappear are a bug.
 *
 * Two rules for what belongs here.
 *
 * A directive is a request the compiler may refuse. Every one below has a fallback that still
 * compiles, and the ones whose absence would silently break the layout are policed by a static
 * assert at the point of use - see MmgrEnumProbe in mmgr_types.h.
 *
 * A directive is not an operation. Nothing here computes anything. The library asks for no
 * __builtin_ at all: the base operations are plain C in the spelling the compiler pattern matches,
 * measured to be the same instruction or better. An unaligned load written as byte assembly is one
 * mov on gcc and clang alike; a lane index is a horizontal byte sum at the same speed as tzcnt; and
 * __builtin_popcountll is a call to __popcountdi2 on baseline x86-64, which the open coded fold
 * beats and which does not link at all on a freestanding target.
 */

/** @brief 1 on clang. */
#if defined(__clang__)
#define MMGR_CC_CLANG 1
#else
#define MMGR_CC_CLANG 0
#endif

/** @brief 1 on gcc proper. clang defines __GNUC__ too, so it is excluded explicitly. */
#if defined(__GNUC__) && !defined(__clang__)
#define MMGR_CC_GNU 1
#else
#define MMGR_CC_GNU 0
#endif

/** @brief 1 where GNU attribute syntax is accepted. Most vendor compilers take it. */
#if defined(__GNUC__) || defined(__clang__)
#define MMGR_CC_GNU_ATTRS 1
#else
#define MMGR_CC_GNU_ATTRS 0
#endif

/**
 * @brief Ask whether an attribute exists.
 *
 * Older compilers have no __has_attribute, so the fallback believes MMGR_CC_GNU_ATTRS rather than
 * answering no to everything.
 */
#if defined(__has_attribute)
#define MMGR_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define MMGR_HAS_ATTRIBUTE(x) MMGR_CC_GNU_ATTRS
#endif

/** @brief Ask whether a builtin exists. Nothing in the library needs one, but a port might. */
#if defined(__has_builtin)
#define MMGR_HAS_BUILTIN(x) __has_builtin(x)
#else
#define MMGR_HAS_BUILTIN(x) 0
#endif

/** @brief static_assert without <assert.h>, which the include budget does not cover. */
#if defined(__cplusplus)
#define MMGR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MMGR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define MMGR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/** @brief Open and close extern "C". */
#ifdef __cplusplus
#define MMGR_BEGIN_DECLS                                                                                               \
    extern "C"                                                                                                         \
    {
#define MMGR_END_DECLS }
#else
#define MMGR_BEGIN_DECLS
#define MMGR_END_DECLS
#endif

/** @brief Paste two tokens after expanding both. */
#define MMGR_CAT_(a, b) a##b
#define MMGR_CAT(a, b) MMGR_CAT_(a, b)

/** @brief Count variadic arguments, up to 24. */
#define MMGR_NARG(...)                                                                                                 \
    MMGR_NARG_(__VA_ARGS__, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define MMGR_NARG_(...) MMGR_ARG_N(__VA_ARGS__)
#define MMGR_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21,     \
                   _22, _23, _24, N, ...)                                                                              \
    N

/**
 * @brief Call a backend with a compound literal, so the aggregate never appears at the call site.
 *
 *     #define mmgr_cellul_find(...) MMGR_CALL(mmgr_cellul_find_ctx, CellulFindArgs, __VA_ARGS__)
 *     mmgr_cellul_find(.hay = h, .needle = n, .ci = 1);
 *
 * Costs nothing. Measured on gcc and clang, the macro, a bare compound literal and a call through
 * the namespace table all reach the same five instructions; naming only two of six fields reaches
 * four, because the zeroed fields fold away. After inlining the aggregate does not exist, and when
 * the backend is not inlined it is one struct on the stack and one pointer in a register - the same
 * traffic the flat argument list needed anyway.
 *
 * Two properties come from the standard rather than the compiler, so they hold everywhere: unnamed
 * fields are zero initialized, which is how a default costs nothing and is never spelled; and a
 * compound literal in argument position lives until the end of the enclosing block, so the backend
 * may hold the pointer for the whole call.
 *
 * Designated initializers are the intended spelling. A positional call still compiles, but -Wextra
 * reports the fields it left behind and designated calls it does not.
 */
#define MMGR_CALL(backend, ArgsType, ...) backend(&(ArgsType){__VA_ARGS__})

/** @brief Size of a function pointer. Not void *, which is the wrong ruler where code and data
 *         pointers differ in width. */
#define MMGR_FP_SIZE (sizeof(void (*)(void)))

/** @brief Assert one member sits at one dispatch slot. */
#define MMGR_NS_SLOT(T, member, slot)                                                                                  \
    MMGR_STATIC_ASSERT(offsetof(T, member) == (size_t)(slot) * MMGR_FP_SIZE,                                           \
                       #T "." #member " is not at dispatch slot " #slot)

/*
 * Unrolled rather than recursive, because a failed assert should name the member and the slot and
 * nothing else. One line per arity.
 */
#define MMGR_NS_L1(T, a) MMGR_NS_SLOT(T, a, 0);
#define MMGR_NS_L2(T, a, b) MMGR_NS_L1(T, a) MMGR_NS_SLOT(T, b, 1);
#define MMGR_NS_L3(T, a, b, c) MMGR_NS_L2(T, a, b) MMGR_NS_SLOT(T, c, 2);
#define MMGR_NS_L4(T, a, b, c, d) MMGR_NS_L3(T, a, b, c) MMGR_NS_SLOT(T, d, 3);
#define MMGR_NS_L5(T, a, b, c, d, e) MMGR_NS_L4(T, a, b, c, d) MMGR_NS_SLOT(T, e, 4);
#define MMGR_NS_L6(T, a, b, c, d, e, f) MMGR_NS_L5(T, a, b, c, d, e) MMGR_NS_SLOT(T, f, 5);
#define MMGR_NS_L7(T, a, b, c, d, e, f, g) MMGR_NS_L6(T, a, b, c, d, e, f) MMGR_NS_SLOT(T, g, 6);
#define MMGR_NS_L8(T, a, b, c, d, e, f, g, h) MMGR_NS_L7(T, a, b, c, d, e, f, g) MMGR_NS_SLOT(T, h, 7);
#define MMGR_NS_L9(T, a, b, c, d, e, f, g, h, i) MMGR_NS_L8(T, a, b, c, d, e, f, g, h) MMGR_NS_SLOT(T, i, 8);
#define MMGR_NS_L10(T, a, b, c, d, e, f, g, h, i, j) MMGR_NS_L9(T, a, b, c, d, e, f, g, h, i) MMGR_NS_SLOT(T, j, 9);
#define MMGR_NS_L11(T, a, b, c, d, e, f, g, h, i, j, k)                                                                \
    MMGR_NS_L10(T, a, b, c, d, e, f, g, h, i, j) MMGR_NS_SLOT(T, k, 10);
#define MMGR_NS_L12(T, a, b, c, d, e, f, g, h, i, j, k, l)                                                             \
    MMGR_NS_L11(T, a, b, c, d, e, f, g, h, i, j, k) MMGR_NS_SLOT(T, l, 11);
#define MMGR_NS_L13(T, a, b, c, d, e, f, g, h, i, j, k, l, m)                                                          \
    MMGR_NS_L12(T, a, b, c, d, e, f, g, h, i, j, k, l) MMGR_NS_SLOT(T, m, 12);
#define MMGR_NS_L14(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n)                                                       \
    MMGR_NS_L13(T, a, b, c, d, e, f, g, h, i, j, k, l, m) MMGR_NS_SLOT(T, n, 13);
#define MMGR_NS_L15(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)                                                    \
    MMGR_NS_L14(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n) MMGR_NS_SLOT(T, o, 14);
#define MMGR_NS_L16(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)                                                 \
    MMGR_NS_L15(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) MMGR_NS_SLOT(T, p, 15);
#define MMGR_NS_L17(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q)                                              \
    MMGR_NS_L16(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) MMGR_NS_SLOT(T, q, 16);
#define MMGR_NS_L18(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r)                                           \
    MMGR_NS_L17(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) MMGR_NS_SLOT(T, r, 17);
#define MMGR_NS_L19(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s)                                        \
    MMGR_NS_L18(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) MMGR_NS_SLOT(T, s, 18);
#define MMGR_NS_L20(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t)                                     \
    MMGR_NS_L19(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) MMGR_NS_SLOT(T, t, 19);
#define MMGR_NS_L21(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u)                                  \
    MMGR_NS_L20(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) MMGR_NS_SLOT(T, u, 20);
#define MMGR_NS_L22(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v)                               \
    MMGR_NS_L21(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) MMGR_NS_SLOT(T, v, 21);
#define MMGR_NS_L23(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w)                            \
    MMGR_NS_L22(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) MMGR_NS_SLOT(T, w, 22);
#define MMGR_NS_L24(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x)                         \
    MMGR_NS_L23(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) MMGR_NS_SLOT(T, x, 23);

/**
 * @brief Pin every dispatch slot of a table that is nothing but function pointers.
 *
 *     MMGR_NS_LAYOUT(VerbumScrutorNs, ge, le, spread, sub7, has_zero, eq, xor_, zero_lane, load);
 *
 * A `<Mod>Ns` is a run of function pointers the library addresses by offset, and a positional
 * initializer mis-wires silently when a member is inserted, removed or moved. This asserts each
 * named member is at its own slot, in the order given, and that sizeof is exactly that many
 * pointers - so a member added and not listed, a member reordered, or padding appearing between
 * them all fail the build at the declaration rather than at a wrong call.
 *
 * Costs nothing at run time.
 */
#define MMGR_NS_LAYOUT(T, ...)                                                                                         \
    MMGR_CAT(MMGR_NS_L, MMGR_NARG(__VA_ARGS__))(T, __VA_ARGS__)                                                        \
        MMGR_STATIC_ASSERT(sizeof(T) == (size_t)MMGR_NARG(__VA_ARGS__) * MMGR_FP_SIZE,                                 \
                           #T " has a member that is not in its dispatch list, or is padded")

/**
 * @brief Pin the dispatch slots of a table that carries state after its entries.
 *
 * clarus and occultum each keep a pointer to their own internals behind the dispatch run. sizeof
 * cannot pin the count there because the tail is not function pointers, so @p tail names the first
 * member after the run and its offset does the same job: insert or drop an entry and the tail
 * moves, which fails.
 */
#define MMGR_NS_LAYOUT_OPEN(T, tail, ...)                                                                              \
    MMGR_CAT(MMGR_NS_L, MMGR_NARG(__VA_ARGS__))(T, __VA_ARGS__)                                                        \
        MMGR_STATIC_ASSERT(offsetof(T, tail) == (size_t)MMGR_NARG(__VA_ARGS__) * MMGR_FP_SIZE,                         \
                           #T "." #tail " does not begin where the dispatch run ends")

/**
 * @brief Storage for a dispatch table. The const is load bearing.
 *
 * Measured: gcc devirtualizes a call through a static const `<Mod>Ns` down to the inlined body,
 * identical instructions to calling the entry directly, and does not devirtualize the same call
 * through a non-const one - that becomes a real call with an 88 byte frame. clang devirtualizes
 * both, so const is what makes the two agree.
 */
#define MMGR_NS static const


/**
 * @brief Pack an enum to its declared width.
 *
 * Losing this is not survivable and not detectable at the use site, so mmgr_types.h asserts on
 * sizeof a probe enum rather than trusting the fallback. An enum that silently becomes an int moves
 * every field after it in every struct, and the library addresses borrows by offset.
 */
#if MMGR_HAS_ATTRIBUTE(packed)
#define MMGR_ENUM_PACKED __attribute__((packed))
#else
#define MMGR_ENUM_PACKED
#endif

/** @brief Align a type or object to @p n bytes. */
#if MMGR_HAS_ATTRIBUTE(aligned)
#define MMGR_ALIGN(n) __attribute__((aligned(n)))
#else
#define MMGR_ALIGN(n)
#endif

/**
 * @brief Exempt a type from strict aliasing.
 *
 * The library no longer reads or writes through punned pointers - loads and stores are byte
 * assembly, which is conforming and lowers to the same instruction - so this is for a caller that
 * hands us a punned type, not for our own use.
 */
#if MMGR_HAS_ATTRIBUTE(may_alias)
#define MMGR_ALIAS __attribute__((may_alias))
#else
#define MMGR_ALIAS
#endif

/** @brief Suppress unused warnings. Every module ends in a namespace most callers only partly
 *         use. */
#if MMGR_HAS_ATTRIBUTE(unused)
#define MMGR_UNUSED __attribute__((unused))
#else
#define MMGR_UNUSED
#endif

/**
 * @brief Weak definition, so a board file can override it.
 *
 * The hardware hooks in dma/ use this and a host build links without one. Without weak symbol
 * support a port must supply its own definitions.
 */
#if MMGR_HAS_ATTRIBUTE(weak)
#define MMGR_WEAK __attribute__((weak))
#else
#define MMGR_WEAK
#endif

/**
 * @brief Inline, not a plea.
 *
 * The SWAR entries are four to seven instructions each and a call frame costs more than the body.
 * Measured, mmgr_memor_chr over 512 bytes is 610 cycles when these are out of line calls and 187
 * when they are inlined.
 */
#ifndef MMGR_INLINE
#if MMGR_HAS_ATTRIBUTE(always_inline)
#define MMGR_INLINE static inline __attribute__((always_inline))
#else
#define MMGR_INLINE static inline
#endif
#endif

/**
 * @brief Per translation unit pragma.
 *
 * gcc only on purpose. clang parses `GCC optimize` and ignores it with a warning.
 */
#if MMGR_CC_GNU
#define MMGR_TU_PRAGMA(directive) _Pragma(#directive)
#else
#define MMGR_TU_PRAGMA(directive)
#endif
#define MMGR_OPTIMIZE_O2 MMGR_TU_PRAGMA(GCC optimize("O2"))

/**
 * @def MMGR_DIAG_PUSH
 * @brief Save the diagnostic state.
 * @def MMGR_DIAG_POP
 * @brief Restore it.
 * @def MMGR_DIAG_IGNORE
 * @brief Silence one warning by its -W name, as a string.
 *
 * For a warning raised inside somebody else's macro, where the code that would have to change is
 * not ours. A type-generic macro from <math.h> that narrows in an arm it does not take is the case
 * this exists for. Never for a warning about code in this library - fix that instead.
 *
 * clang spells the pragma the same way but under its own name, so both are given.
 */
#if defined(__clang__)
#define MMGR_DIAG_PUSH _Pragma("clang diagnostic push")
#define MMGR_DIAG_POP _Pragma("clang diagnostic pop")
#define MMGR_DIAG_IGNORE(w) _Pragma(MMGR_DIAG_STR(clang diagnostic ignored w))
#define MMGR_DIAG_STR(x) #x
#elif defined(__GNUC__)
#define MMGR_DIAG_PUSH _Pragma("GCC diagnostic push")
#define MMGR_DIAG_POP _Pragma("GCC diagnostic pop")
#define MMGR_DIAG_STR(x) #x
#define MMGR_DIAG_IGNORE(w) _Pragma(MMGR_DIAG_STR(GCC diagnostic ignored w))
#else
#define MMGR_DIAG_PUSH
#define MMGR_DIAG_POP
#define MMGR_DIAG_IGNORE(w)
#endif

/**
 * @brief 1 on a big endian target.
 *
 * Detection, not policy - a build may pin it. The library reads and writes explicit byte order
 * everywhere it matters, so this only selects which end of a SWAR mask holds the first byte in
 * address order.
 */
#ifndef MMGR_HW_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MMGR_HW_BIG_ENDIAN 1
#else
#define MMGR_HW_BIG_ENDIAN 0
#endif
#endif

#endif
