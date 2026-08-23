#ifndef MMGR_COMPILER_DIRECTIVES_H
#define MMGR_COMPILER_DIRECTIVES_H

#include <stddef.h>


#if defined(__clang__)
#define MMGR_CC_CLANG 1
#else
#define MMGR_CC_CLANG 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define MMGR_CC_GNU 1
#else
#define MMGR_CC_GNU 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MMGR_CC_GNU_ATTRS 1
#else
#define MMGR_CC_GNU_ATTRS 0
#endif

#if defined(__has_attribute)
#define MMGR_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define MMGR_HAS_ATTRIBUTE(x) MMGR_CC_GNU_ATTRS
#endif

#if defined(__has_builtin)
#define MMGR_HAS_BUILTIN(x) __has_builtin(x)
#else
#define MMGR_HAS_BUILTIN(x) 0
#endif

#if defined(__cplusplus)
#define MMGR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MMGR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define MMGR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

#ifdef __cplusplus
#define MMGR_INCIPE_DECLS                                                                                               \
    extern "C"                                                                                                         \
    {
#define MMGR_FINIS_DECLS }
#else
#define MMGR_INCIPE_DECLS
#define MMGR_FINIS_DECLS
#endif

#define MMGR_CAT_(a, b) a##b
#define MMGR_CAT(a, b) MMGR_CAT_(a, b)

#define MMGR_NARG(...)                                                                                                 \
    MMGR_NARG_(__VA_ARGS__, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define MMGR_NARG_(...) MMGR_ARG_N(__VA_ARGS__)
#define MMGR_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21,     \
                   _22, _23, _24, N, ...)                                                                              \
    N

#define MMGR_CALL(backend, ArgsType, ...) backend(&(ArgsType){__VA_ARGS__})

#define MMGR_FP_SIZE (sizeof(void (*)(void)))

#define MMGR_NS_LOCULUS(T, member, loculus)                                                                                  \
    MMGR_STATIC_ASSERT(offsetof(T, member) == (size_t)(loculus) * MMGR_FP_SIZE,                                           \
                       #T "." #member " is not at dispatch loculus " #loculus)

#define MMGR_NS_L1(T, a) MMGR_NS_LOCULUS(T, a, 0);
#define MMGR_NS_L2(T, a, b) MMGR_NS_L1(T, a) MMGR_NS_LOCULUS(T, b, 1);
#define MMGR_NS_L3(T, a, b, c) MMGR_NS_L2(T, a, b) MMGR_NS_LOCULUS(T, c, 2);
#define MMGR_NS_L4(T, a, b, c, d) MMGR_NS_L3(T, a, b, c) MMGR_NS_LOCULUS(T, d, 3);
#define MMGR_NS_L5(T, a, b, c, d, e) MMGR_NS_L4(T, a, b, c, d) MMGR_NS_LOCULUS(T, e, 4);
#define MMGR_NS_L6(T, a, b, c, d, e, f) MMGR_NS_L5(T, a, b, c, d, e) MMGR_NS_LOCULUS(T, f, 5);
#define MMGR_NS_L7(T, a, b, c, d, e, f, g) MMGR_NS_L6(T, a, b, c, d, e, f) MMGR_NS_LOCULUS(T, g, 6);
#define MMGR_NS_L8(T, a, b, c, d, e, f, g, h) MMGR_NS_L7(T, a, b, c, d, e, f, g) MMGR_NS_LOCULUS(T, h, 7);
#define MMGR_NS_L9(T, a, b, c, d, e, f, g, h, i) MMGR_NS_L8(T, a, b, c, d, e, f, g, h) MMGR_NS_LOCULUS(T, i, 8);
#define MMGR_NS_L10(T, a, b, c, d, e, f, g, h, i, j) MMGR_NS_L9(T, a, b, c, d, e, f, g, h, i) MMGR_NS_LOCULUS(T, j, 9);
#define MMGR_NS_L11(T, a, b, c, d, e, f, g, h, i, j, k)                                                                \
    MMGR_NS_L10(T, a, b, c, d, e, f, g, h, i, j) MMGR_NS_LOCULUS(T, k, 10);
#define MMGR_NS_L12(T, a, b, c, d, e, f, g, h, i, j, k, l)                                                             \
    MMGR_NS_L11(T, a, b, c, d, e, f, g, h, i, j, k) MMGR_NS_LOCULUS(T, l, 11);
#define MMGR_NS_L13(T, a, b, c, d, e, f, g, h, i, j, k, l, m)                                                          \
    MMGR_NS_L12(T, a, b, c, d, e, f, g, h, i, j, k, l) MMGR_NS_LOCULUS(T, m, 12);
#define MMGR_NS_L14(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n)                                                       \
    MMGR_NS_L13(T, a, b, c, d, e, f, g, h, i, j, k, l, m) MMGR_NS_LOCULUS(T, n, 13);
#define MMGR_NS_L15(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)                                                    \
    MMGR_NS_L14(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n) MMGR_NS_LOCULUS(T, o, 14);
#define MMGR_NS_L16(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)                                                 \
    MMGR_NS_L15(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) MMGR_NS_LOCULUS(T, p, 15);
#define MMGR_NS_L17(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q)                                              \
    MMGR_NS_L16(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) MMGR_NS_LOCULUS(T, q, 16);
#define MMGR_NS_L18(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r)                                           \
    MMGR_NS_L17(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) MMGR_NS_LOCULUS(T, r, 17);
#define MMGR_NS_L19(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s)                                        \
    MMGR_NS_L18(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) MMGR_NS_LOCULUS(T, s, 18);
#define MMGR_NS_L20(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t)                                     \
    MMGR_NS_L19(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) MMGR_NS_LOCULUS(T, t, 19);
#define MMGR_NS_L21(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u)                                  \
    MMGR_NS_L20(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) MMGR_NS_LOCULUS(T, u, 20);
#define MMGR_NS_L22(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v)                               \
    MMGR_NS_L21(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) MMGR_NS_LOCULUS(T, v, 21);
#define MMGR_NS_L23(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w)                            \
    MMGR_NS_L22(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) MMGR_NS_LOCULUS(T, w, 22);
#define MMGR_NS_L24(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x)                         \
    MMGR_NS_L23(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) MMGR_NS_LOCULUS(T, x, 23);

#define MMGR_NS_LAYOUT(T, ...)                                                                                         \
    MMGR_CAT(MMGR_NS_L, MMGR_NARG(__VA_ARGS__))(T, __VA_ARGS__)                                                        \
        MMGR_STATIC_ASSERT(sizeof(T) == (size_t)MMGR_NARG(__VA_ARGS__) * MMGR_FP_SIZE,                                 \
                           #T " has a member that is not in its dispatch list, or is padded")

#define MMGR_NS_LAYOUT_OPEN(T, tail, ...)                                                                              \
    MMGR_CAT(MMGR_NS_L, MMGR_NARG(__VA_ARGS__))(T, __VA_ARGS__)                                                        \
        MMGR_STATIC_ASSERT(offsetof(T, tail) == (size_t)MMGR_NARG(__VA_ARGS__) * MMGR_FP_SIZE,                         \
                           #T "." #tail " does not begin where the dispatch run ends")

#define MMGR_NS static const


#if MMGR_HAS_ATTRIBUTE(packed)
#define MMGR_ENUM_PACKED __attribute__((packed))
#else
#define MMGR_ENUM_PACKED
#endif

#if MMGR_HAS_ATTRIBUTE(aligned)
#define MMGR_ALIGN(n) __attribute__((aligned(n)))
#else
#define MMGR_ALIGN(n)
#endif

#if MMGR_HAS_ATTRIBUTE(may_alias)
#define MMGR_ALIAS __attribute__((may_alias))
#else
#define MMGR_ALIAS
#endif

#if MMGR_HAS_ATTRIBUTE(unused)
#define MMGR_UNUSED __attribute__((unused))
#else
#define MMGR_UNUSED
#endif

#if MMGR_HAS_ATTRIBUTE(weak)
#define MMGR_WEAK __attribute__((weak))
#else
#define MMGR_WEAK
#endif

#ifndef MMGR_INLINE
#if MMGR_HAS_ATTRIBUTE(always_inline)
#define MMGR_INLINE static inline __attribute__((always_inline))
#else
#define MMGR_INLINE static inline
#endif
#endif

#if MMGR_CC_GNU
#define MMGR_TU_PRAGMA(directive) _Pragma(#directive)
#else
#define MMGR_TU_PRAGMA(directive)
#endif
#define MMGR_OPTIMIZE_O2 MMGR_TU_PRAGMA(GCC optimize("O2"))

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

#ifndef MMGR_HW_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MMGR_HW_BIG_ENDIAN 1
#else
#define MMGR_HW_BIG_ENDIAN 0
#endif
#endif

#endif
