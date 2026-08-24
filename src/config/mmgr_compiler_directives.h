/**
 * @brief Preprocessor definitions; declares no type and defines no function.
 */
#ifndef MMGR_COMPILER_DIRECTIVES_H
#define MMGR_COMPILER_DIRECTIVES_H

#include <stddef.h>

/**
 * @brief Set to 1 when __clang__ is defined, 0 otherwise.
 *
 * @note Defined on both branches, so #ifdef MMGR_CC_CLANG is always true.
 */
#if defined(__clang__)
#define MMGR_CC_CLANG 1
#else

#define MMGR_CC_CLANG 0
#endif

/**
 * @brief Set to 1 when __GNUC__ is defined and __clang__ is not, 0 otherwise.
 */
#if defined(__GNUC__) && !defined(__clang__)
#define MMGR_CC_GNU 1
#else

#define MMGR_CC_GNU 0
#endif

/**
 * @brief Set to 1 when __GNUC__ or __clang__ is defined, 0 otherwise.
 *
 * @note Used as the fallback value of MMGR_HAS_ATTRIBUTE.
 */
#if defined(__GNUC__) || defined(__clang__)
#define MMGR_CC_GNU_ATTRS 1
#else

#define MMGR_CC_GNU_ATTRS 0
#endif

/**
 * @brief Expands to __has_attribute(x) where __has_attribute is defined.
 *
 * @param[in] x Attribute name, as passed to __has_attribute.
 * @return      The value __has_attribute gives for x.
 * @warning Expands to MMGR_CC_GNU_ATTRS where __has_attribute is undefined, ignoring x.
 */
#if defined(__has_attribute)
#define MMGR_HAS_ATTRIBUTE(x) __has_attribute(x)
#else

#define MMGR_HAS_ATTRIBUTE(x) MMGR_CC_GNU_ATTRS
#endif

/**
 * @brief Expands to __has_builtin(x) where __has_builtin is defined.
 *
 * @param[in] x Builtin name, as passed to __has_builtin.
 * @return      The value __has_builtin gives for x.
 * @warning Expands to 0 where __has_builtin is undefined, ignoring x.
 */
#if defined(__has_builtin)
#define MMGR_HAS_BUILTIN(x) __has_builtin(x)
#else

#define MMGR_HAS_BUILTIN(x) 0
#endif

/**
 * @brief Expands to a two-operand static assertion.
 *
 * @param[in] cond Constant expression passed through unchanged.
 * @param[in] msg  Message operand passed through unchanged.
 * @note static_assert where __cplusplus is defined or __STDC_VERSION__ >= 202311L.
 * @note _Static_assert otherwise.
 */
#if defined(__cplusplus)
#define MMGR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MMGR_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else

#define MMGR_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/**
 * @brief Expands to extern "C" { where __cplusplus is defined.
 *
 * @warning The expansion contains an unmatched {; MMGR_FINIS_DECLS supplies the }.
 * @note Expands to nothing where __cplusplus is undefined.
 */
#ifdef __cplusplus
#define MMGR_INCIPE_DECLS                                                                                              \
    extern "C"                                                                                                         \
    {

/** @brief Expands to } where __cplusplus is defined, closing MMGR_INCIPE_DECLS. */
#define MMGR_FINIS_DECLS }
#else

#define MMGR_INCIPE_DECLS

/** @brief Expands to nothing where __cplusplus is undefined. */
#define MMGR_FINIS_DECLS
#endif

/**
 * @brief Expands to a##b.
 *
 * @param[in] a Left operand of ##.
 * @param[in] b Right operand of ##.
 * @return      The single token formed by joining a and b.
 * @note Called by MMGR_CAT.
 */
#define MMGR_CAT_(a, b) a##b

/**
 * @brief Expands to MMGR_CAT_(a, b).
 *
 * @param[in] a Left operand, forwarded to MMGR_CAT_.
 * @param[in] b Right operand, forwarded to MMGR_CAT_.
 * @return      The single token formed by joining a and b.
 * @note Used by MMGR_NS_LAYOUT and MMGR_NS_LAYOUT_OPEN to build a macro name.
 */
#define MMGR_CAT(a, b) MMGR_CAT_(a, b)

/**
 * @brief Expands to MMGR_NARG_ with __VA_ARGS__ followed by the constants 24 down to 0.
 *
 * @param[in] ... The list to count.
 * @return        The number of arguments, for one to twenty-four arguments.
 * @warning An empty list gives 1.
 * @warning Twenty-five or more arguments give a count that is too low.
 */
#define MMGR_NARG(...)                                                                                                 \
    MMGR_NARG_(__VA_ARGS__, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
/**
 * @brief Expands to MMGR_ARG_N(__VA_ARGS__).
 *
 * @param[in] ... The list from MMGR_NARG, followed by the constants 24 down to 0.
 * @return        The value MMGR_ARG_N selects.
 * @note Called by MMGR_NARG.
 */
#define MMGR_NARG_(...) MMGR_ARG_N(__VA_ARGS__)

/**
 * @brief Expands to its twenty-fifth argument.
 *
 * @param[in] _1  Arguments one through twenty-four, discarded.
 * @param[in] N   The twenty-fifth argument.
 * @param[in] ... Arguments beyond the twenty-fifth, discarded.
 * @return        N.
 */
#define MMGR_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21,     \
                   _22, _23, _24, N, ...)                                                                              \
    N

/**
 * @brief Expands to backend(&(ArgsType){__VA_ARGS__}).
 *
 * @param[in] backend  Function called with the address of the literal.
 * @param[in] ArgsType Type of the compound literal.
 * @param[in] ...      Initialisers for the compound literal.
 * @return             The value backend returns.
 * @warning backend receives the address of the literal [BORROWS].
 */
#define MMGR_CALL(backend, ArgsType, ...) backend(&(ArgsType){__VA_ARGS__})

/**
 * @brief Expands to sizeof(void (*)(void)).
 *
 * @note Multiplied by the loculus index in MMGR_NS_LOCULUS, MMGR_NS_LAYOUT and MMGR_NS_LAYOUT_OPEN.
 */
#define MMGR_FP_SIZE (sizeof(void (*)(void)))

/**
 * @brief Expands to MMGR_STATIC_ASSERT that offsetof(T, member) equals loculus * MMGR_FP_SIZE.
 *
 * @param[in] T       Struct type passed to offsetof.
 * @param[in] member  Member name passed to offsetof.
 * @param[in] loculus Index, cast to size_t and multiplied by MMGR_FP_SIZE.
 * @note T, member and loculus are stringised into the assertion message.
 */
#define MMGR_NS_LOCULUS(T, member, loculus)                                                                            \
    MMGR_STATIC_ASSERT(offsetof(T, member) == (size_t)(loculus) * MMGR_FP_SIZE,                                        \
                       #T "." #member " is not at dispatch loculus " #loculus)

/**
 * @brief MMGR_NS_L1 to MMGR_NS_L24 expand to one MMGR_NS_LOCULUS per member, at indices 0 upward.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member names in order; a takes index 0, b takes index 1.
 * @note MMGR_NS_L<n> expands MMGR_NS_L<n-1> before its own MMGR_NS_LOCULUS.
 * @warning Selected by MMGR_NS_LAYOUT through MMGR_CAT on the argument count.
 */
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

/**
 * @brief Expands the matching MMGR_NS_L, then asserts sizeof(T) equals the member count times MMGR_FP_SIZE.
 *
 * @param[in] T   Struct type forwarded to MMGR_NS_L and to sizeof.
 * @param[in] ... Member names in loculus order, one to twenty-four.
 * @note MMGR_NARG(__VA_ARGS__) is cast to size_t before the multiply.
 * @warning Any size other than the member count times MMGR_FP_SIZE fails the assertion.
 */
#define MMGR_NS_LAYOUT(T, ...)                                                                                         \
    MMGR_CAT(MMGR_NS_L, MMGR_NARG(__VA_ARGS__))(T, __VA_ARGS__)                                                        \
        MMGR_STATIC_ASSERT(sizeof(T) == (size_t)MMGR_NARG(__VA_ARGS__) * MMGR_FP_SIZE,                                 \
                           #T " has a member that is not in its dispatch list, or is padded")

/**
 * @brief Expands the matching MMGR_NS_L, then asserts offsetof(T, tail) equals the count times MMGR_FP_SIZE.
 *
 * @param[in] T    Struct type forwarded to MMGR_NS_L and to offsetof.
 * @param[in] tail Member name passed to offsetof.
 * @param[in] ...  Member names in loculus order, one to twenty-four.
 * @note MMGR_NARG(__VA_ARGS__) is cast to size_t before the multiply.
 * @note Tests tail's offset, where MMGR_NS_LAYOUT tests sizeof(T).
 */
#define MMGR_NS_LAYOUT_OPEN(T, tail, ...)                                                                              \
    MMGR_CAT(MMGR_NS_L, MMGR_NARG(__VA_ARGS__))(T, __VA_ARGS__)                                                        \
        MMGR_STATIC_ASSERT(offsetof(T, tail) == (size_t)MMGR_NARG(__VA_ARGS__) * MMGR_FP_SIZE,                         \
                           #T "." #tail " does not begin where the dispatch run ends")

/**
 * @brief Expands to static const.
 *
 * @note Used to declare the namespace tables, such as ascii, spat and clz.
 */
#define MMGR_NS static const

/**
 * @brief Expands to __attribute__((packed)) where MMGR_HAS_ATTRIBUTE(packed) is non-zero.
 *
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(packed) is 0.
 * @note mmgr_types.h asserts sizeof(MmgrEnumProbe) == 1 to catch that case.
 */
#if MMGR_HAS_ATTRIBUTE(packed)
#define MMGR_ENUM_PACKED __attribute__((packed))
#else

#define MMGR_ENUM_PACKED
#endif

/**
 * @brief Expands to __attribute__((aligned(n))) where MMGR_HAS_ATTRIBUTE(aligned) is non-zero.
 *
 * @param[in] n Alignment operand passed to the attribute.
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(aligned) is 0, ignoring n.
 */
#if MMGR_HAS_ATTRIBUTE(aligned)
#define MMGR_ALIGN(n) __attribute__((aligned(n)))
#else

#define MMGR_ALIGN(n)
#endif

/**
 * @brief Expands to __attribute__((may_alias)) where MMGR_HAS_ATTRIBUTE(may_alias) is non-zero.
 *
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(may_alias) is 0.
 */
#if MMGR_HAS_ATTRIBUTE(may_alias)
#define MMGR_ALIAS __attribute__((may_alias))
#else

#define MMGR_ALIAS
#endif

/**
 * @brief Expands to __attribute__((unused)) where MMGR_HAS_ATTRIBUTE(unused) is non-zero.
 *
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(unused) is 0.
 */
#if MMGR_HAS_ATTRIBUTE(unused)
#define MMGR_UNUSED __attribute__((unused))
#else

#define MMGR_UNUSED
#endif

/**
 * @brief Expands to __attribute__((weak)) where MMGR_HAS_ATTRIBUTE(weak) is non-zero.
 *
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(weak) is 0.
 */
#if MMGR_HAS_ATTRIBUTE(weak)
#define MMGR_WEAK __attribute__((weak))
#else

#define MMGR_WEAK
#endif

/**
 * @brief Expands to static inline __attribute__((always_inline)).
 *
 * @note Expands to static inline where MMGR_HAS_ATTRIBUTE(always_inline) is 0.
 * @warning Neither definition is made when MMGR_INLINE is already defined.
 */
#ifndef MMGR_INLINE
#if MMGR_HAS_ATTRIBUTE(always_inline)
#define MMGR_INLINE static inline __attribute__((always_inline))
#else
#define MMGR_INLINE static inline
#endif
#endif

/**
 * @brief Expands to _Pragma(#directive) where MMGR_CC_GNU is non-zero.
 *
 * @param[in] directive Pragma text, stringised by #.
 * @warning Expands to nothing where MMGR_CC_GNU is 0, ignoring directive.
 */
#if MMGR_CC_GNU
#define MMGR_TU_PRAGMA(directive) _Pragma(#directive)
#else

#define MMGR_TU_PRAGMA(directive)
#endif

/**
 * @brief Expands to MMGR_TU_PRAGMA(GCC optimize("O2")).
 *
 * @warning Expands to nothing where MMGR_CC_GNU is 0, through MMGR_TU_PRAGMA.
 */
#define MMGR_OPTIMIZE_O2 MMGR_TU_PRAGMA(GCC optimize("O2"))

#if defined(__clang__)
/** @brief Expands to _Pragma("clang diagnostic push") where __clang__ is defined. */
#define MMGR_DIAG_PUSH _Pragma("clang diagnostic push")

/** @brief Expands to _Pragma("clang diagnostic pop") where __clang__ is defined. */
#define MMGR_DIAG_POP _Pragma("clang diagnostic pop")

/**
 * @brief Expands to _Pragma(MMGR_DIAG_STR(clang diagnostic ignored w)) where __clang__ is defined.
 *
 * @param[in] w Warning name as a string literal, such as "-Wpadded".
 * @note MMGR_DIAG_STR stringises the whole pragma text, including w.
 */
#define MMGR_DIAG_IGNORE(w) _Pragma(MMGR_DIAG_STR(clang diagnostic ignored w))

/**
 * @brief Expands to #x.
 *
 * @param[in] x Token sequence to stringise.
 * @return      x as a string literal.
 * @note Called by MMGR_DIAG_IGNORE.
 */
#define MMGR_DIAG_STR(x) #x
#elif defined(__GNUC__)
/** @brief Expands to _Pragma("GCC diagnostic push") where __GNUC__ is defined and __clang__ is not. */
#define MMGR_DIAG_PUSH _Pragma("GCC diagnostic push")

/** @brief Expands to _Pragma("GCC diagnostic pop") where __GNUC__ is defined and __clang__ is not. */
#define MMGR_DIAG_POP _Pragma("GCC diagnostic pop")

/**
 * @brief Expands to #x.
 *
 * @param[in] x Token sequence to stringise.
 * @return      x as a string literal.
 * @note Called by MMGR_DIAG_IGNORE.
 */
#define MMGR_DIAG_STR(x) #x

/**
 * @brief Expands to _Pragma(MMGR_DIAG_STR(GCC diagnostic ignored w)).
 *
 * @param[in] w Warning name as a string literal, such as "-Wpadded".
 * @note Selected where __GNUC__ is defined and __clang__ is not.
 * @note MMGR_DIAG_STR stringises the whole pragma text, including w.
 */
#define MMGR_DIAG_IGNORE(w) _Pragma(MMGR_DIAG_STR(GCC diagnostic ignored w))
#else

/** @brief Expands to nothing when __clang__ and __GNUC__ are both undefined. */
#define MMGR_DIAG_PUSH

/** @brief Expands to nothing when __clang__ and __GNUC__ are both undefined. */
#define MMGR_DIAG_POP

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @param[in] w Warning name as a string literal, discarded.
 */
#define MMGR_DIAG_IGNORE(w)
#endif

/**
 * @brief Set to 1 when __BYTE_ORDER__ and __ORDER_BIG_ENDIAN__ are both defined and equal, 0 otherwise.
 *
 * @warning Neither definition is made when MMGR_HW_BIG_ENDIAN is already defined.
 */
#ifndef MMGR_HW_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MMGR_HW_BIG_ENDIAN 1
#else

#define MMGR_HW_BIG_ENDIAN 0
#endif
#endif

#endif
