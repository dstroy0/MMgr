/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file mmgr_compiler_directives.h
 * @brief Preprocessor definitions; declares no type and defines no function.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
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
 *
 * @note Defined on both branches, so #ifdef MMGR_CC_GNU is always true.
 */
#if defined(__GNUC__) && !defined(__clang__)
#define MMGR_CC_GNU 1
#else

#define MMGR_CC_GNU 0
#endif

/**
 * @brief Set to 1 when __GNUC__ or __clang__ is defined, 0 otherwise.
 *
 * @note Defined on both branches, so #ifdef MMGR_CC_GNU_ATTRS is always true.
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

/** @brief Expands to nothing where __cplusplus is undefined, so no brace is opened and none is owed. */
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
 * @note Builds a macro name from a count, as in MMGR_NS_LAYOUT and MMGR_CARCER_WALK.
 */
#define MMGR_CAT(a, b) MMGR_CAT_(a, b)

/**
 * @brief Expands to MMGR_NARG_ with __VA_ARGS__ followed by the constants 24 down to 0.
 *
 * @param[in] ... The list to count.
 * @return        The number of arguments, for one to twenty-four arguments.
 * @warning An empty list gives 1.
 * @warning Twenty-five or more arguments make MMGR_ARG_N select an argument instead of a constant.
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
 * @param[in] ...      Initializers for the compound literal.
 * @return             The value backend returns.
 * @warning backend receives the address of the literal [BORROWS].
 */
#define MMGR_CALL(backend, ArgsType, ...) backend(&(ArgsType){__VA_ARGS__})

/**
 * @brief Expands to sizeof(void (*)(void)).
 *
 * @note Multiplied by a loculus index in MMGR_NS_LOCULUS, and by the member count in the two layout macros.
 */
#define MMGR_FP_SIZE (sizeof(void (*)(void)))

/**
 * @brief Expands to MMGR_STATIC_ASSERT that offsetof(T, member) equals loculus * MMGR_FP_SIZE.
 *
 * @param[in] T       Struct type passed to offsetof.
 * @param[in] member  Member name passed to offsetof.
 * @param[in] loculus Index, cast to size_t and multiplied by MMGR_FP_SIZE.
 * @note T, member and loculus are stringized into the assertion message.
 */
#define MMGR_NS_LOCULUS(T, member, loculus)                                                                            \
    MMGR_STATIC_ASSERT(offsetof(T, member) == (size_t)(loculus) * MMGR_FP_SIZE,                                        \
                       #T "." #member " is not at dispatch loculus " #loculus)

/**
 * @brief Asserts one member sits at dispatch loculus 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a The single member's name.
 * @note The base of the family. Every longer line ends in this one, so it is the only line here that
 *       names no other.
 */
#define MMGR_NS_L1(T, a) MMGR_NS_LOCULUS(T, a, 0);

/**
 * @brief Asserts two members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0, which MMGR_NS_L1 asserts.
 * @param[in] b Member at loculus 1, asserted after it.
 * @note The step every longer line repeats: expand the line one shorter, then assert the next index.
 *       The preprocessor cannot walk a list, so each member count needs a line of its own.
 */
#define MMGR_NS_L2(T, a, b) MMGR_NS_L1(T, a) MMGR_NS_LOCULUS(T, b, 1);

/**
 * @brief Asserts three members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2, asserted after MMGR_NS_L2 covers the first two.
 * @note MMGR_NS_LAYOUT selects this line for a type with three members.
 */
#define MMGR_NS_L3(T, a, b, c) MMGR_NS_L2(T, a, b) MMGR_NS_LOCULUS(T, c, 2);

/**
 * @brief Asserts four members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3, asserted after MMGR_NS_L3 covers the first three.
 * @note MMGR_NS_LAYOUT selects this line for a type with four members.
 */
#define MMGR_NS_L4(T, a, b, c, d) MMGR_NS_L3(T, a, b, c) MMGR_NS_LOCULUS(T, d, 3);

/**
 * @brief Asserts five members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4, asserted after MMGR_NS_L4 covers the first four.
 * @note MMGR_NS_LAYOUT selects this line for a type with five members.
 */
#define MMGR_NS_L5(T, a, b, c, d, e) MMGR_NS_L4(T, a, b, c, d) MMGR_NS_LOCULUS(T, e, 4);

/**
 * @brief Asserts six members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5, asserted after MMGR_NS_L5 covers the first five.
 * @note MMGR_NS_LAYOUT selects this line for a type with six members.
 */
#define MMGR_NS_L6(T, a, b, c, d, e, f) MMGR_NS_L5(T, a, b, c, d, e) MMGR_NS_LOCULUS(T, f, 5);

/**
 * @brief Asserts seven members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6, asserted after MMGR_NS_L6 covers the first six.
 * @note MMGR_NS_LAYOUT selects this line for a type with seven members.
 */
#define MMGR_NS_L7(T, a, b, c, d, e, f, g) MMGR_NS_L6(T, a, b, c, d, e, f) MMGR_NS_LOCULUS(T, g, 6);

/**
 * @brief Asserts eight members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7, asserted after MMGR_NS_L7 covers the first seven.
 * @note MMGR_NS_LAYOUT selects this line for a type with eight members.
 */
#define MMGR_NS_L8(T, a, b, c, d, e, f, g, h) MMGR_NS_L7(T, a, b, c, d, e, f, g) MMGR_NS_LOCULUS(T, h, 7);

/**
 * @brief Asserts nine members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8, asserted after MMGR_NS_L8 covers the first eight.
 * @note MMGR_NS_LAYOUT selects this line for a type with nine members.
 */
#define MMGR_NS_L9(T, a, b, c, d, e, f, g, h, i) MMGR_NS_L8(T, a, b, c, d, e, f, g, h) MMGR_NS_LOCULUS(T, i, 8);

/**
 * @brief Asserts ten members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9, asserted after MMGR_NS_L9 covers the first nine.
 * @note MMGR_NS_LAYOUT selects this line for a type with ten members.
 */
#define MMGR_NS_L10(T, a, b, c, d, e, f, g, h, i, j) MMGR_NS_L9(T, a, b, c, d, e, f, g, h, i) MMGR_NS_LOCULUS(T, j, 9);

/**
 * @brief Asserts eleven members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10, asserted after MMGR_NS_L10 covers the first ten.
 * @note MMGR_NS_LAYOUT selects this line for a type with eleven members.
 */
#define MMGR_NS_L11(T, a, b, c, d, e, f, g, h, i, j, k)                                                                \
    MMGR_NS_L10(T, a, b, c, d, e, f, g, h, i, j) MMGR_NS_LOCULUS(T, k, 10);

/**
 * @brief Asserts twelve members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11, asserted after MMGR_NS_L11 covers the first eleven.
 * @note MMGR_NS_LAYOUT selects this line for a type with twelve members.
 */
#define MMGR_NS_L12(T, a, b, c, d, e, f, g, h, i, j, k, l)                                                             \
    MMGR_NS_L11(T, a, b, c, d, e, f, g, h, i, j, k) MMGR_NS_LOCULUS(T, l, 11);

/**
 * @brief Asserts thirteen members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12, asserted after MMGR_NS_L12 covers the first twelve.
 * @note MMGR_NS_LAYOUT selects this line for a type with thirteen members.
 */
#define MMGR_NS_L13(T, a, b, c, d, e, f, g, h, i, j, k, l, m)                                                          \
    MMGR_NS_L12(T, a, b, c, d, e, f, g, h, i, j, k, l) MMGR_NS_LOCULUS(T, m, 12);

/**
 * @brief Asserts fourteen members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13, asserted after MMGR_NS_L13 covers the first thirteen.
 * @note MMGR_NS_LAYOUT selects this line for a type with fourteen members.
 */
#define MMGR_NS_L14(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n)                                                       \
    MMGR_NS_L13(T, a, b, c, d, e, f, g, h, i, j, k, l, m) MMGR_NS_LOCULUS(T, n, 13);

/**
 * @brief Asserts fifteen members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14, asserted after MMGR_NS_L14 covers the first fourteen.
 * @note MMGR_NS_LAYOUT selects this line for a type with fifteen members.
 */
#define MMGR_NS_L15(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)                                                    \
    MMGR_NS_L14(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n) MMGR_NS_LOCULUS(T, o, 14);

/**
 * @brief Asserts sixteen members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14.
 * @param[in] p Member at loculus 15, asserted after MMGR_NS_L15 covers the first fifteen.
 * @note MMGR_NS_LAYOUT selects this line for a type with sixteen members.
 */
#define MMGR_NS_L16(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)                                                 \
    MMGR_NS_L15(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) MMGR_NS_LOCULUS(T, p, 15);

/**
 * @brief Asserts seventeen members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14.
 * @param[in] p Member at loculus 15.
 * @param[in] q Member at loculus 16, asserted after MMGR_NS_L16 covers the first sixteen.
 * @note MMGR_NS_LAYOUT selects this line for a type with seventeen members.
 */
#define MMGR_NS_L17(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q)                                              \
    MMGR_NS_L16(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) MMGR_NS_LOCULUS(T, q, 16);

/**
 * @brief Asserts eighteen members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14.
 * @param[in] p Member at loculus 15.
 * @param[in] q Member at loculus 16.
 * @param[in] r Member at loculus 17, asserted after MMGR_NS_L17 covers the first seventeen.
 * @note MMGR_NS_LAYOUT selects this line for a type with eighteen members.
 */
#define MMGR_NS_L18(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r)                                           \
    MMGR_NS_L17(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) MMGR_NS_LOCULUS(T, r, 17);

/**
 * @brief Asserts nineteen members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14.
 * @param[in] p Member at loculus 15.
 * @param[in] q Member at loculus 16.
 * @param[in] r Member at loculus 17.
 * @param[in] s Member at loculus 18, asserted after MMGR_NS_L18 covers the first eighteen.
 * @note MMGR_NS_LAYOUT selects this line for a type with nineteen members.
 */
#define MMGR_NS_L19(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s)                                        \
    MMGR_NS_L18(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) MMGR_NS_LOCULUS(T, s, 18);

/**
 * @brief Asserts twenty members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14.
 * @param[in] p Member at loculus 15.
 * @param[in] q Member at loculus 16.
 * @param[in] r Member at loculus 17.
 * @param[in] s Member at loculus 18.
 * @param[in] t Member at loculus 19, asserted after MMGR_NS_L19 covers the first nineteen.
 * @note MMGR_NS_LAYOUT selects this line for a type with twenty members.
 */
#define MMGR_NS_L20(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t)                                     \
    MMGR_NS_L19(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) MMGR_NS_LOCULUS(T, t, 19);

/**
 * @brief Asserts twenty-one members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14.
 * @param[in] p Member at loculus 15.
 * @param[in] q Member at loculus 16.
 * @param[in] r Member at loculus 17.
 * @param[in] s Member at loculus 18.
 * @param[in] t Member at loculus 19.
 * @param[in] u Member at loculus 20, asserted after MMGR_NS_L20 covers the first twenty.
 * @note MMGR_NS_LAYOUT selects this line for a type with twenty-one members.
 */
#define MMGR_NS_L21(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u)                                  \
    MMGR_NS_L20(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) MMGR_NS_LOCULUS(T, u, 20);

/**
 * @brief Asserts twenty-two members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14.
 * @param[in] p Member at loculus 15.
 * @param[in] q Member at loculus 16.
 * @param[in] r Member at loculus 17.
 * @param[in] s Member at loculus 18.
 * @param[in] t Member at loculus 19.
 * @param[in] u Member at loculus 20.
 * @param[in] v Member at loculus 21, asserted after MMGR_NS_L21 covers the first twenty-one.
 * @note MMGR_NS_LAYOUT selects this line for a type with twenty-two members.
 */
#define MMGR_NS_L22(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v)                               \
    MMGR_NS_L21(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) MMGR_NS_LOCULUS(T, v, 21);

/**
 * @brief Asserts twenty-three members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14.
 * @param[in] p Member at loculus 15.
 * @param[in] q Member at loculus 16.
 * @param[in] r Member at loculus 17.
 * @param[in] s Member at loculus 18.
 * @param[in] t Member at loculus 19.
 * @param[in] u Member at loculus 20.
 * @param[in] v Member at loculus 21.
 * @param[in] w Member at loculus 22, asserted after MMGR_NS_L22 covers the first twenty-two.
 * @note MMGR_NS_LAYOUT selects this line for a type with twenty-three members.
 */
#define MMGR_NS_L23(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w)                            \
    MMGR_NS_L22(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) MMGR_NS_LOCULUS(T, w, 22);

/**
 * @brief Asserts twenty-four members sit at consecutive loculi from 0.
 *
 * @param[in] T Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] a Member at loculus 0.
 * @param[in] b Member at loculus 1.
 * @param[in] c Member at loculus 2.
 * @param[in] d Member at loculus 3.
 * @param[in] e Member at loculus 4.
 * @param[in] f Member at loculus 5.
 * @param[in] g Member at loculus 6.
 * @param[in] h Member at loculus 7.
 * @param[in] i Member at loculus 8.
 * @param[in] j Member at loculus 9.
 * @param[in] k Member at loculus 10.
 * @param[in] l Member at loculus 11.
 * @param[in] m Member at loculus 12.
 * @param[in] n Member at loculus 13.
 * @param[in] o Member at loculus 14.
 * @param[in] p Member at loculus 15.
 * @param[in] q Member at loculus 16.
 * @param[in] r Member at loculus 17.
 * @param[in] s Member at loculus 18.
 * @param[in] t Member at loculus 19.
 * @param[in] u Member at loculus 20.
 * @param[in] v Member at loculus 21.
 * @param[in] w Member at loculus 22.
 * @param[in] x Member at loculus 23, asserted after MMGR_NS_L23 covers the first twenty-three.
 * @note MMGR_NS_LAYOUT selects this line for a type with twenty-four members.
 * @warning The ceiling of the family. A twenty-fifth member has no MMGR_NS_L25 to reach, and
 *          MMGR_NARG cannot count that far either, so raising the ceiling means adding a line here
 *          and a constant to both MMGR_NARG and MMGR_ARG_N.
 */
#define MMGR_NS_L24(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x)                         \
    MMGR_NS_L23(T, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) MMGR_NS_LOCULUS(T, x, 23);

/**
 * @brief Expands the matching MMGR_NS_L, then asserts sizeof(T) equals the member count times MMGR_FP_SIZE.
 *
 * @param[in] T   Struct type forwarded to MMGR_NS_L and to sizeof.
 * @param[in] ... Member names in loculus order, one to twenty-four.
 * @note Two mistakes are what this is for, and neither is visible at a use site. A member added to
 *       the struct but left out of the list moves every entry below it to a loculus that is no
 *       longer its own, and padding between members does the same without anyone editing the list
 *       at all. The per-member assertions catch the first; the size assertion catches the second,
 *       since a padded struct is larger than the count times MMGR_FP_SIZE.
 * @note MMGR_NARG(__VA_ARGS__) is cast to size_t before the multiply.
 * @warning Any size other than the member count times MMGR_FP_SIZE fails the assertion.
 */
#define MMGR_NS_LAYOUT(T, ...)                                                                                         \
    MMGR_CAT(MMGR_NS_L, MMGR_NARG(__VA_ARGS__))(T, __VA_ARGS__)                                                        \
        MMGR_STATIC_ASSERT(sizeof(T) == (size_t)MMGR_NARG(__VA_ARGS__) * MMGR_FP_SIZE,                                 \
                           #T " has a member that is not in its dispatch list, or is padded")

/**
 * @brief Expands to static const.
 *
 * @note Declares the dispatch tables, such as ascii, spat and clz. They stand in for a namespace,
 *       which C does not have.
 * @note Internal linkage is what makes a table definable in a header at all: each translation unit
 *       that includes it gets its own, rather than every one of them defining the same symbol. const
 *       is what pays for the indirection, since a table no other translation unit can reach and no
 *       code assigns to leaves the compiler free to resolve an entry call to the backend directly.
 * @note Each table carries MMGR_UNUSED as well, for the translation unit that includes the header
 *       and calls nothing through it.
 */
#define MMGR_NS static const

/**
 * @brief Expands to __attribute__((flatten)) where MMGR_HAS_ATTRIBUTE(flatten) is non-zero.
 *
 * @note For a caller, not for the library. It asks the compiler to inline everything the function it
 *       marks calls, which reaches an entry body that the inliner would otherwise leave out of line
 *       on size. The entries are large enough that it does leave them: measured on an ESP32-S3, a
 *       cellul.len over eight bytes costs 112 cycles called and 80 inlined, so the call is 32 of
 *       them - a third of the work at that length, and it is paid on every entry.
 * @note Worth it where an extent is short and settled before the build, which is what this library
 *       is for. A long scan amortizes the call and will not notice: the same measurement at 64 bytes
 *       is 253 against 240.
 * @note Costs the walk's code at every site that takes it, so it belongs on the one hot function a
 *       caller cares about rather than on a translation unit.
 * @warning Needs the entry body visible, so a build without link-time optimization gets nothing from
 *          it. That is the MMGR_LTO build option, which defaults to ON.
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(flatten) is 0, which costs speed and never
 *          correctness.
 */
#if MMGR_HAS_ATTRIBUTE(flatten)
#define MMGR_FLATTEN __attribute__((flatten))
#else

#define MMGR_FLATTEN
#endif

/**
 * @brief Expands to __attribute__((packed)) where MMGR_HAS_ATTRIBUTE(packed) is non-zero.
 *
 * @note An enum carrying this is declared at the width its range needs rather than at int width. A
 *       struct with such a member takes its offsets from that width, so the attribute belongs to the
 *       layout and is not a size optimization.
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(packed) is 0, and a compiler may also accept
 *          the attribute and then ignore it. Neither case is visible from here, which is why
 *          mmgr_types.h declares MmgrEnumProbe and asserts sizeof(MmgrEnumProbe) == 1 rather than
 *          trusting this #if.
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
 * @note Used in both directions. It raises alignment on storage, as locus_carcerum.h does on a
 *       cellblock's bytes and confinium_exclusivum_infinitas.h does on its ring state. It also
 *       lowers alignment to 1, which is one half of MMGR_RAW in proximus_operor.h and lets a word
 *       type be read from any address.
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(aligned) is 0, ignoring n. A raise that
 *          vanishes leaves an object less aligned than its use expects. A lower to 1 that vanishes
 *          leaves the type at its natural alignment while the code still reads it from any address.
 */
#if MMGR_HAS_ATTRIBUTE(aligned)
#define MMGR_ALIGN(n) __attribute__((aligned(n)))
#else

#define MMGR_ALIGN(n)
#endif

/**
 * @brief Expands to __attribute__((may_alias)) where MMGR_HAS_ATTRIBUTE(may_alias) is non-zero.
 *
 * @note The library reads a byte array through a word type. A character type may alias any object.
 *       A word lvalue reading the bytes of a uint8_t array is the direction the aliasing rules
 *       forbid, and this attribute is what permits it. MMGR_RAW in proximus_operor.h carries it, and
 *       proximus_operor.c also uses it on its own for a type that keeps uint64_t's alignment.
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(may_alias) is 0. Nothing diagnoses that. The
 *          code still compiles and the compiler is free to assume the two accesses never meet.
 */
#if MMGR_HAS_ATTRIBUTE(may_alias)
#define MMGR_ALIAS __attribute__((may_alias))
#else

#define MMGR_ALIAS
#endif

/**
 * @brief Expands to __attribute__((unused)) where MMGR_HAS_ATTRIBUTE(unused) is non-zero.
 *
 * @note Suppresses the unused-variable diagnostic on a definition that is deliberately left
 *       unreferenced. A dispatch table is defined in a header with internal linkage, so every
 *       translation unit including that header gets a copy of it. One that calls nothing through
 *       its copy would warn about a definition it never asked for.
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(unused) is 0. That costs a diagnostic and
 *          never correctness.
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
 * @param[in] directive Pragma text, stringized by #.
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
 * @note MMGR_DIAG_STR stringizes the whole pragma text, including w.
 */
#define MMGR_DIAG_IGNORE(w) _Pragma(MMGR_DIAG_STR(clang diagnostic ignored w))

/**
 * @brief Expands to #x.
 *
 * @param[in] x Token sequence to stringize.
 * @return      x as a string literal.
 * @note Called by MMGR_DIAG_IGNORE.
 * @warning Not defined where neither __clang__ nor __GNUC__ is. Nothing needs it there, since
 *          MMGR_DIAG_IGNORE expands to nothing on that arm.
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
 * @param[in] x Token sequence to stringize.
 * @return      x as a string literal.
 * @note Called by MMGR_DIAG_IGNORE.
 * @warning Not defined where neither __clang__ nor __GNUC__ is. Nothing needs it there, since
 *          MMGR_DIAG_IGNORE expands to nothing on that arm.
 */
#define MMGR_DIAG_STR(x) #x

/**
 * @brief Expands to _Pragma(MMGR_DIAG_STR(GCC diagnostic ignored w)).
 *
 * @param[in] w Warning name as a string literal, such as "-Wpadded".
 * @note Selected where __GNUC__ is defined and __clang__ is not.
 * @note MMGR_DIAG_STR stringizes the whole pragma text, including w.
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

/**
 * @brief Set to 1 when the target loads a word from any address in one instruction, 0 otherwise.
 *
 * @note Not whether an unaligned load compiles - every target here accepts one through
 *       mmgr_proxim_word_t, which carries MMGR_ALIGN(1). It is whether the hardware does it, or the
 *       compiler assembles it out of byte loads and shifts. Measured on one such load: ARMv7-M emits
 *       a single ldr, Xtensa twelve instructions and RISC-V eleven.
 * @note The difference decides which of two shapes is faster in a scan that needs the word at an
 *       offset of one. Where a load is a single instruction, take it; where it is a dozen, derive
 *       the word from the one already in hand and a byte.
 * @note __ARM_FEATURE_UNALIGNED is the compiler's own answer for ARM, and is switched off by
 *       -mno-unaligned-access. x86 is stated directly, having no equivalent macro.
 * @warning Neither definition is made when MMGR_HW_FAST_UNALIGNED is already defined.
 */
#ifndef MMGR_HW_FAST_UNALIGNED
#if defined(__ARM_FEATURE_UNALIGNED) || defined(__x86_64__) || defined(__i386__)
#define MMGR_HW_FAST_UNALIGNED 1
#else

#define MMGR_HW_FAST_UNALIGNED 0
#endif
#endif

#endif
