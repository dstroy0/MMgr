/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file mmgr_compiler_directives.h
 * @brief Preprocessor definitions, declaring no type and defining no function.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 */
#ifndef MMGR_COMPILER_DIRECTIVES_H
#define MMGR_COMPILER_DIRECTIVES_H

#include <stddef.h>

/**
 * @brief Set to 1 when __GNUC__ or __clang__ is defined, 0 otherwise.
 *
 * @note The fallback MMGR_HAS_ATTRIBUTE answers with where __has_attribute is missing. A compiler
 *       old enough to lack __has_attribute may still accept the GNU attributes, so answering 0 there
 *       would turn every attribute in this header off on a compiler that supports them.
 * @note Defined on both branches, so #ifdef MMGR_CC_GNU_ATTRS is always true. Test it with #if.
 */
#if defined(__GNUC__) || defined(__clang__)
#define MMGR_CC_GNU_ATTRS 1
#else

#define MMGR_CC_GNU_ATTRS 0
#endif

/**
 * @brief Expands to __has_attribute(attribute_) where __has_attribute is defined.
 *
 * @param[in] attribute_ Attribute name, as passed to __has_attribute.
 * @return               The value __has_attribute gives for attribute_.
 * @note Every attribute wrapper below is gated on this rather than on a compiler test, so a build
 *       is asked what it supports instead of being guessed at from its identity.
 * @warning Expands to MMGR_CC_GNU_ATTRS where __has_attribute is undefined, ignoring attribute_.
 *          That answer is the same for every attribute asked about, so a compiler without
 *          __has_attribute either gets all of them or none.
 */
#if defined(__has_attribute)
#define MMGR_HAS_ATTRIBUTE(attribute_) __has_attribute(attribute_)
#else

#define MMGR_HAS_ATTRIBUTE(attribute_) MMGR_CC_GNU_ATTRS
#endif

/**
 * @brief Expands to __has_builtin(builtin_) where __has_builtin is defined.
 *
 * @param[in] builtin_ Builtin name, as passed to __has_builtin.
 * @return             The value __has_builtin gives for builtin_.
 * @warning Expands to 0 where __has_builtin is undefined, ignoring builtin_. Unlike the attribute
 *          test there is no fallback worth guessing, since a builtin that is absent fails to
 *          compile rather than being ignored.
 */
#if defined(__has_builtin)
#define MMGR_HAS_BUILTIN(builtin_) __has_builtin(builtin_)
#else

#define MMGR_HAS_BUILTIN(builtin_) 0
#endif

/**
 * @brief Expands to a two-operand static assertion.
 *
 * @param[in] cond_ Constant expression passed through unchanged.
 * @param[in] msg_  Message operand passed through unchanged.
 * @note The library states what the build must prove rather than testing it at run time, so this is
 *       reached from every module. Spelling it once keeps the three-way spelling question here.
 * @note static_assert where __cplusplus is defined or __STDC_VERSION__ >= 202311L.
 * @note _Static_assert otherwise, which is the C11 spelling and the one this library is written to.
 */
#if defined(__cplusplus)
#define MMGR_STATIC_ASSERT(cond_, msg_) static_assert(cond_, msg_)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MMGR_STATIC_ASSERT(cond_, msg_) static_assert(cond_, msg_)
#else

#define MMGR_STATIC_ASSERT(cond_, msg_) _Static_assert(cond_, msg_)
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
 * @brief Expands to left_##right_.
 *
 * @param[in] left_  Left operand of ##.
 * @param[in] right_ Right operand of ##.
 * @return           The single token formed by joining left_ and right_.
 * @note The inner half of the two-step paste. ## suppresses expansion of its own operands, so a
 *       caller pasting a macro's value rather than its name has to go through MMGR_CAT.
 */
#define MMGR_CAT_(left_, right_) left_##right_

/**
 * @brief Expands to MMGR_CAT_(left_, right_).
 *
 * @param[in] left_  Left operand, forwarded to MMGR_CAT_.
 * @param[in] right_ Right operand, forwarded to MMGR_CAT_.
 * @return           The single token formed by joining left_ and right_.
 * @note The outer half. Its arguments expand before substitution, so MMGR_NARG's count arrives as a
 *       number and MMGR_CAT_ pastes that rather than the word MMGR_NARG.
 * @note Builds a macro name from a count, as in MMGR_NS_LAYOUT and MMGR_CARCER_WALK.
 */
#define MMGR_CAT(left_, right_) MMGR_CAT_(left_, right_)

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
 * @param[in] slot1_    Arguments one through twenty-four, discarded.
 * @param[in] selected_ The twenty-fifth argument, which is the one returned.
 * @param[in] ...       Arguments beyond the twenty-fifth, discarded.
 * @return              selected_.
 * @note How the count is taken. MMGR_NARG hands the caller's list followed by 24 down to 0, so the
 *       list shifts the constants along and whichever one lands in the twenty-fifth slot is the
 *       number of arguments the caller passed.
 */
#define MMGR_ARG_N(slot1_, slot2_, slot3_, slot4_, slot5_, slot6_, slot7_, slot8_, slot9_, slot10_, slot11_, slot12_,  \
                   slot13_, slot14_, slot15_, slot16_, slot17_, slot18_, slot19_, slot20_, slot21_, slot22_, slot23_,  \
                   slot24_, selected_, ...)                                                                            \
    selected_

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
 * @brief Asserts one member sits at the dispatch loculus its position claims.
 *
 * @param[in] Table_   Struct type passed to offsetof.
 * @param[in] member_  Member name passed to offsetof.
 * @param[in] loculus_ Index, cast to size_t and multiplied by MMGR_FP_SIZE.
 * @note The single check the whole MMGR_NS_L family is built out of. One member, one offset.
 * @note Table_, member_ and loculus_ are stringized into the assertion message, so a failure names
 *       the table, the member and the index it was expected at rather than only a file and line.
 */
#define MMGR_NS_LOCULUS(Table_, member_, loculus_)                                                                     \
    MMGR_STATIC_ASSERT(offsetof(Table_, member_) == (size_t)(loculus_) * MMGR_FP_SIZE,                                 \
                       #Table_ "." #member_ " is not at dispatch loculus " #loculus_)

/**
 * @brief Asserts one member sits at dispatch loculus 0.
 *
 * @param[in] Table_   Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_ The single member's name, at loculus 0.
 * @note The base of the family. Every longer line ends in this one, so it is the only line here that
 *       names no other.
 */
#define MMGR_NS_L1(Table_, member1_) MMGR_NS_LOCULUS(Table_, member1_, 0);

/**
 * @brief Asserts two members sit at consecutive loculi from 0.
 *
 * @param[in] Table_   Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_ Member at loculus 0, which MMGR_NS_L1 asserts.
 * @param[in] member2_ Member at loculus 1, asserted after it.
 * @note The step every longer line repeats. Expand the line one shorter, then assert the next index.
 *       The preprocessor cannot walk a list, so each member count needs a line of its own.
 */
#define MMGR_NS_L2(Table_, member1_, member2_) MMGR_NS_L1(Table_, member1_) MMGR_NS_LOCULUS(Table_, member2_, 1);

/**
 * @brief Asserts three members sit at consecutive loculi from 0.
 *
 * @param[in] Table_   Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_ Member at loculus 0.
 * @param[in] member2_ Member at loculus 1.
 * @param[in] member3_ Member at loculus 2, asserted after MMGR_NS_L2 covers the first two.
 * @note MMGR_NS_LAYOUT selects this line for a table with three members.
 */
#define MMGR_NS_L3(Table_, member1_, member2_, member3_)                                                               \
    MMGR_NS_L2(Table_, member1_, member2_) MMGR_NS_LOCULUS(Table_, member3_, 2);

/**
 * @brief Asserts four members sit at consecutive loculi from 0.
 *
 * @param[in] Table_   Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_ Member at loculus 0.
 * @param[in] member2_ Member at loculus 1.
 * @param[in] member3_ Member at loculus 2.
 * @param[in] member4_ Member at loculus 3, asserted after MMGR_NS_L3 covers the first three.
 * @note MMGR_NS_LAYOUT selects this line for a table with four members.
 */
#define MMGR_NS_L4(Table_, member1_, member2_, member3_, member4_)                                                     \
    MMGR_NS_L3(Table_, member1_, member2_, member3_) MMGR_NS_LOCULUS(Table_, member4_, 3);

/**
 * @brief Asserts five members sit at consecutive loculi from 0.
 *
 * @param[in] Table_   Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_ Member at loculus 0.
 * @param[in] member2_ Member at loculus 1.
 * @param[in] member3_ Member at loculus 2.
 * @param[in] member4_ Member at loculus 3.
 * @param[in] member5_ Member at loculus 4, asserted after MMGR_NS_L4 covers the first four.
 * @note MMGR_NS_LAYOUT selects this line for a table with five members.
 */
#define MMGR_NS_L5(Table_, member1_, member2_, member3_, member4_, member5_)                                           \
    MMGR_NS_L4(Table_, member1_, member2_, member3_, member4_) MMGR_NS_LOCULUS(Table_, member5_, 4);

/**
 * @brief Asserts six members sit at consecutive loculi from 0.
 *
 * @param[in] Table_   Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_ Member at loculus 0.
 * @param[in] member2_ Member at loculus 1.
 * @param[in] member3_ Member at loculus 2.
 * @param[in] member4_ Member at loculus 3.
 * @param[in] member5_ Member at loculus 4.
 * @param[in] member6_ Member at loculus 5, asserted after MMGR_NS_L5 covers the first five.
 * @note MMGR_NS_LAYOUT selects this line for a table with six members.
 */
#define MMGR_NS_L6(Table_, member1_, member2_, member3_, member4_, member5_, member6_)                                 \
    MMGR_NS_L5(Table_, member1_, member2_, member3_, member4_, member5_) MMGR_NS_LOCULUS(Table_, member6_, 5);

/**
 * @brief Asserts seven members sit at consecutive loculi from 0.
 *
 * @param[in] Table_   Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_ Member at loculus 0.
 * @param[in] member2_ Member at loculus 1.
 * @param[in] member3_ Member at loculus 2.
 * @param[in] member4_ Member at loculus 3.
 * @param[in] member5_ Member at loculus 4.
 * @param[in] member6_ Member at loculus 5.
 * @param[in] member7_ Member at loculus 6, asserted after MMGR_NS_L6 covers the first six.
 * @note MMGR_NS_LAYOUT selects this line for a table with seven members.
 */
#define MMGR_NS_L7(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_)                       \
    MMGR_NS_L6(Table_, member1_, member2_, member3_, member4_, member5_, member6_)                                     \
    MMGR_NS_LOCULUS(Table_, member7_, 6);

/**
 * @brief Asserts eight members sit at consecutive loculi from 0.
 *
 * @param[in] Table_   Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_ Member at loculus 0.
 * @param[in] member2_ Member at loculus 1.
 * @param[in] member3_ Member at loculus 2.
 * @param[in] member4_ Member at loculus 3.
 * @param[in] member5_ Member at loculus 4.
 * @param[in] member6_ Member at loculus 5.
 * @param[in] member7_ Member at loculus 6.
 * @param[in] member8_ Member at loculus 7, asserted after MMGR_NS_L7 covers the first seven.
 * @note MMGR_NS_LAYOUT selects this line for a table with eight members.
 */
#define MMGR_NS_L8(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_)             \
    MMGR_NS_L7(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_)                           \
    MMGR_NS_LOCULUS(Table_, member8_, 7);

/**
 * @brief Asserts nine members sit at consecutive loculi from 0.
 *
 * @param[in] Table_   Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_ Member at loculus 0.
 * @param[in] member2_ Member at loculus 1.
 * @param[in] member3_ Member at loculus 2.
 * @param[in] member4_ Member at loculus 3.
 * @param[in] member5_ Member at loculus 4.
 * @param[in] member6_ Member at loculus 5.
 * @param[in] member7_ Member at loculus 6.
 * @param[in] member8_ Member at loculus 7.
 * @param[in] member9_ Member at loculus 8, asserted after MMGR_NS_L8 covers the first eight.
 * @note MMGR_NS_LAYOUT selects this line for a table with nine members.
 */
#define MMGR_NS_L9(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_)   \
    MMGR_NS_L8(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_)                 \
    MMGR_NS_LOCULUS(Table_, member9_, 8);

/**
 * @brief Asserts ten members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9, asserted after MMGR_NS_L9 covers the first nine.
 * @note MMGR_NS_LAYOUT selects this line for a table with ten members.
 */
#define MMGR_NS_L10(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_)                                                                                         \
    MMGR_NS_L9(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_)       \
    MMGR_NS_LOCULUS(Table_, member10_, 9);

/**
 * @brief Asserts eleven members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10, asserted after MMGR_NS_L10 covers the first ten.
 * @note MMGR_NS_LAYOUT selects this line for a table with eleven members.
 */
#define MMGR_NS_L11(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_)                                                                              \
    MMGR_NS_L10(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_)                                                                                             \
    MMGR_NS_LOCULUS(Table_, member11_, 10);

/**
 * @brief Asserts twelve members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11, asserted after MMGR_NS_L11 covers the first eleven.
 * @note MMGR_NS_LAYOUT selects this line for a table with twelve members.
 */
#define MMGR_NS_L12(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_)                                                                   \
    MMGR_NS_L11(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_)                                                                                  \
    MMGR_NS_LOCULUS(Table_, member12_, 11);

/**
 * @brief Asserts thirteen members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12, asserted after MMGR_NS_L12 covers the first twelve.
 * @note MMGR_NS_LAYOUT selects this line for a table with thirteen members.
 */
#define MMGR_NS_L13(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_)                                                        \
    MMGR_NS_L12(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_)                                                                       \
    MMGR_NS_LOCULUS(Table_, member13_, 12);

/**
 * @brief Asserts fourteen members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13, asserted after MMGR_NS_L13 covers the first thirteen.
 * @note MMGR_NS_LAYOUT selects this line for a table with fourteen members.
 */
#define MMGR_NS_L14(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_)                                             \
    MMGR_NS_L13(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_)                                                            \
    MMGR_NS_LOCULUS(Table_, member14_, 13);

/**
 * @brief Asserts fifteen members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14, asserted after MMGR_NS_L14 covers the first fourteen.
 * @note MMGR_NS_LAYOUT selects this line for a table with fifteen members.
 */
#define MMGR_NS_L15(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_)                                  \
    MMGR_NS_L14(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_)                                                 \
    MMGR_NS_LOCULUS(Table_, member15_, 14);

/**
 * @brief Asserts sixteen members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14.
 * @param[in] member16_ Member at loculus 15, asserted after MMGR_NS_L15 covers the first fifteen.
 * @note MMGR_NS_LAYOUT selects this line for a table with sixteen members.
 */
#define MMGR_NS_L16(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_, member16_)                       \
    MMGR_NS_L15(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_, member15_)                                      \
    MMGR_NS_LOCULUS(Table_, member16_, 15);

/**
 * @brief Asserts seventeen members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14.
 * @param[in] member16_ Member at loculus 15.
 * @param[in] member17_ Member at loculus 16, asserted after MMGR_NS_L16 covers the first sixteen.
 * @note MMGR_NS_LAYOUT selects this line for a table with seventeen members.
 */
#define MMGR_NS_L17(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_)            \
    MMGR_NS_L16(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_, member15_, member16_)                           \
    MMGR_NS_LOCULUS(Table_, member17_, 16);

/**
 * @brief Asserts eighteen members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14.
 * @param[in] member16_ Member at loculus 15.
 * @param[in] member17_ Member at loculus 16.
 * @param[in] member18_ Member at loculus 17, asserted after MMGR_NS_L17 covers the first seventeen.
 * @note MMGR_NS_LAYOUT selects this line for a table with eighteen members.
 */
#define MMGR_NS_L18(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_) \
    MMGR_NS_L17(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_)                \
    MMGR_NS_LOCULUS(Table_, member18_, 17);

/**
 * @brief Asserts nineteen members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14.
 * @param[in] member16_ Member at loculus 15.
 * @param[in] member17_ Member at loculus 16.
 * @param[in] member18_ Member at loculus 17.
 * @param[in] member19_ Member at loculus 18, asserted after MMGR_NS_L18 covers the first eighteen.
 * @note MMGR_NS_LAYOUT selects this line for a table with nineteen members.
 */
#define MMGR_NS_L19(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_, \
                    member19_)                                                                                         \
    MMGR_NS_L18(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_)     \
    MMGR_NS_LOCULUS(Table_, member19_, 18);

/**
 * @brief Asserts twenty members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14.
 * @param[in] member16_ Member at loculus 15.
 * @param[in] member17_ Member at loculus 16.
 * @param[in] member18_ Member at loculus 17.
 * @param[in] member19_ Member at loculus 18.
 * @param[in] member20_ Member at loculus 19, asserted after MMGR_NS_L19 covers the first nineteen.
 * @note MMGR_NS_LAYOUT selects this line for a table with twenty members.
 */
#define MMGR_NS_L20(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_, \
                    member19_, member20_)                                                                              \
    MMGR_NS_L19(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_,     \
                member19_)                                                                                             \
    MMGR_NS_LOCULUS(Table_, member20_, 19);

/**
 * @brief Asserts twenty-one members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14.
 * @param[in] member16_ Member at loculus 15.
 * @param[in] member17_ Member at loculus 16.
 * @param[in] member18_ Member at loculus 17.
 * @param[in] member19_ Member at loculus 18.
 * @param[in] member20_ Member at loculus 19.
 * @param[in] member21_ Member at loculus 20, asserted after MMGR_NS_L20 covers the first twenty.
 * @note MMGR_NS_LAYOUT selects this line for a table with twenty-one members.
 */
#define MMGR_NS_L21(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_, \
                    member19_, member20_, member21_)                                                                   \
    MMGR_NS_L20(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_,     \
                member19_, member20_)                                                                                  \
    MMGR_NS_LOCULUS(Table_, member21_, 20);

/**
 * @brief Asserts twenty-two members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14.
 * @param[in] member16_ Member at loculus 15.
 * @param[in] member17_ Member at loculus 16.
 * @param[in] member18_ Member at loculus 17.
 * @param[in] member19_ Member at loculus 18.
 * @param[in] member20_ Member at loculus 19.
 * @param[in] member21_ Member at loculus 20.
 * @param[in] member22_ Member at loculus 21, asserted after MMGR_NS_L21 covers the first twenty-one.
 * @note MMGR_NS_LAYOUT selects this line for a table with twenty-two members.
 */
#define MMGR_NS_L22(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_, \
                    member19_, member20_, member21_, member22_)                                                        \
    MMGR_NS_L21(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_,     \
                member19_, member20_, member21_)                                                                       \
    MMGR_NS_LOCULUS(Table_, member22_, 21);

/**
 * @brief Asserts twenty-three members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14.
 * @param[in] member16_ Member at loculus 15.
 * @param[in] member17_ Member at loculus 16.
 * @param[in] member18_ Member at loculus 17.
 * @param[in] member19_ Member at loculus 18.
 * @param[in] member20_ Member at loculus 19.
 * @param[in] member21_ Member at loculus 20.
 * @param[in] member22_ Member at loculus 21.
 * @param[in] member23_ Member at loculus 22, asserted after MMGR_NS_L22 covers the first twenty-two.
 * @note MMGR_NS_LAYOUT selects this line for a table with twenty-three members.
 */
#define MMGR_NS_L23(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_, \
                    member19_, member20_, member21_, member22_, member23_)                                             \
    MMGR_NS_L22(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_,     \
                member19_, member20_, member21_, member22_)                                                            \
    MMGR_NS_LOCULUS(Table_, member23_, 22);

/**
 * @brief Asserts twenty-four members sit at consecutive loculi from 0.
 *
 * @param[in] Table_    Struct type forwarded to MMGR_NS_LOCULUS.
 * @param[in] member1_  Member at loculus 0.
 * @param[in] member2_  Member at loculus 1.
 * @param[in] member3_  Member at loculus 2.
 * @param[in] member4_  Member at loculus 3.
 * @param[in] member5_  Member at loculus 4.
 * @param[in] member6_  Member at loculus 5.
 * @param[in] member7_  Member at loculus 6.
 * @param[in] member8_  Member at loculus 7.
 * @param[in] member9_  Member at loculus 8.
 * @param[in] member10_ Member at loculus 9.
 * @param[in] member11_ Member at loculus 10.
 * @param[in] member12_ Member at loculus 11.
 * @param[in] member13_ Member at loculus 12.
 * @param[in] member14_ Member at loculus 13.
 * @param[in] member15_ Member at loculus 14.
 * @param[in] member16_ Member at loculus 15.
 * @param[in] member17_ Member at loculus 16.
 * @param[in] member18_ Member at loculus 17.
 * @param[in] member19_ Member at loculus 18.
 * @param[in] member20_ Member at loculus 19.
 * @param[in] member21_ Member at loculus 20.
 * @param[in] member22_ Member at loculus 21.
 * @param[in] member23_ Member at loculus 22.
 * @param[in] member24_ Member at loculus 23, asserted after MMGR_NS_L23 covers the first
 *                      twenty-three.
 * @note MMGR_NS_LAYOUT selects this line for a table with twenty-four members.
 * @warning The ceiling of the family. A twenty-fifth member has no MMGR_NS_L25 to reach, and
 *          MMGR_NARG cannot count that far either, so raising the ceiling means adding a line here
 *          and a constant to both MMGR_NARG and MMGR_ARG_N.
 */
#define MMGR_NS_L24(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,  \
                    member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_, \
                    member19_, member20_, member21_, member22_, member23_, member24_)                                  \
    MMGR_NS_L23(Table_, member1_, member2_, member3_, member4_, member5_, member6_, member7_, member8_, member9_,      \
                member10_, member11_, member12_, member13_, member14_, member15_, member16_, member17_, member18_,     \
                member19_, member20_, member21_, member22_, member23_)                                                 \
    MMGR_NS_LOCULUS(Table_, member24_, 23);

/**
 * @brief Asserts a dispatch table's members sit at consecutive loculi and that nothing else is in it.
 *
 * @param[in] Table_ Struct type forwarded to MMGR_NS_L and to sizeof.
 * @param[in] ...    Member names in loculus order, one to twenty-four.
 * @note Two mistakes are what this is for, and neither is visible at a use site. A member added to
 *       the struct but left out of the list moves every entry below it to a loculus that is no
 *       longer its own, and padding between members does the same without anyone editing the list
 *       at all. The per-member assertions catch the first. The size assertion catches the second,
 *       since a padded struct is larger than the count times MMGR_FP_SIZE.
 * @note MMGR_CAT builds the MMGR_NS_L line's name from MMGR_NARG's count of the member list, so the
 *       caller states the members once and the arity follows from them.
 * @note MMGR_NARG(__VA_ARGS__) is cast to size_t before the multiply.
 * @warning Any size other than the member count times MMGR_FP_SIZE fails the assertion.
 */
#define MMGR_NS_LAYOUT(Table_, ...)                                                                                    \
    MMGR_CAT(MMGR_NS_L, MMGR_NARG(__VA_ARGS__))(Table_, __VA_ARGS__)                                                   \
        MMGR_STATIC_ASSERT(sizeof(Table_) == (size_t)MMGR_NARG(__VA_ARGS__) * MMGR_FP_SIZE,                            \
                           #Table_ " has a member that is not in its dispatch list, or is padded")

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
 * @param[in] bytes_ Alignment operand passed to the attribute.
 * @note Used in both directions. It raises alignment on storage, as locus_carcerum.h does on a
 *       cellblock's bytes and memoria_anularis.h does on its ring state. It also
 *       lowers alignment to 1, which is one half of MMGR_RAW in proximus_operor.h and lets a word
 *       type be read from any address.
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(aligned) is 0, ignoring bytes_. A raise that
 *          vanishes leaves an object less aligned than its use expects. A lower to 1 that vanishes
 *          leaves the type at its natural alignment while the code still reads it from any address.
 */
#if MMGR_HAS_ATTRIBUTE(aligned)
#define MMGR_ALIGN(bytes_) __attribute__((aligned(bytes_)))
#else

#define MMGR_ALIGN(bytes_)
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
 * @note Marks the port layer's default hardware hooks, which memoriam_praetereo.c defines to refuse
 *       or to do nothing. An unported build links those and fails visibly at the call rather than
 *       failing to link, and a port replaces them by defining the same symbols.
 * @warning Expands to nothing where MMGR_HAS_ATTRIBUTE(weak) is 0. The defaults then have external
 *          linkage like any other definition, so a port that defines the same symbol collides with
 *          one instead of replacing it, and the link fails on a duplicate.
 */
#if MMGR_HAS_ATTRIBUTE(weak)
#define MMGR_WEAK __attribute__((weak))
#else

#define MMGR_WEAK
#endif

/**
 * @brief Expands to static inline __attribute__((always_inline)).
 *
 * @note Declares the file-scope helpers throughout the library. Each one names a step rather than
 *       factoring out a cost, so a helper the compiler leaves out of line puts a call on a path the
 *       design gives none. Stating the attribute here once keeps it off all of them.
 * @note Expands to static inline where MMGR_HAS_ATTRIBUTE(always_inline) is 0. Plain inline asks
 *       rather than requires, which costs speed and never correctness.
 * @warning Neither definition is made when MMGR_INLINE is already defined. A build wanting a
 *          different inlining policy defines it ahead of this header rather than editing this.
 */
#ifndef MMGR_INLINE
#if MMGR_HAS_ATTRIBUTE(always_inline)
#define MMGR_INLINE static inline __attribute__((always_inline))
#else
#define MMGR_INLINE static inline
#endif
#endif

/**
 * @brief Expands to #text_.
 *
 * @param[in] text_ Token sequence to stringize.
 * @return          text_ as a string literal.
 * @note Called by MMGR_DIAG_IGNORE, which builds a whole pragma line and stringizes it in one step
 *       because _Pragma takes a string literal and the warning name arrives as one already.
 * @note Defined once above the compiler arms rather than inside each. The definition does not vary
 *       by compiler, and two identical copies drift the moment one is edited.
 */
#define MMGR_DIAG_STR(text_) #text_

#if defined(__clang__)
/**
 * @brief Expands to _Pragma("clang diagnostic push") where __clang__ is defined.
 *
 * @note Saves the diagnostic state so a suppression can be bounded. An ignored pragma with nothing
 *       saved ahead of it runs to the end of the translation unit and silences code it was never
 *       meant to cover, so every MMGR_DIAG_IGNORE sits between this and MMGR_DIAG_POP.
 */
#define MMGR_DIAG_PUSH _Pragma("clang diagnostic push")

/**
 * @brief Expands to _Pragma("clang diagnostic pop") where __clang__ is defined.
 *
 * @note Restores the state MMGR_DIAG_PUSH saved, which is what ends a suppression. Omitting it
 *       leaves the ignore in force for the rest of the translation unit, and nothing diagnoses that
 *       because the diagnostic it would have raised is the one being suppressed.
 */
#define MMGR_DIAG_POP _Pragma("clang diagnostic pop")

/**
 * @brief Expands to _Pragma(MMGR_DIAG_STR(clang diagnostic ignored warning_)) under __clang__.
 *
 * @param[in] warning_ Warning name as a string literal, such as "-Wpadded".
 * @note Suppresses one named diagnostic, and belongs between a MMGR_DIAG_PUSH and a MMGR_DIAG_POP
 *       so it ends where the code that needs it ends.
 * @note Nothing under src invokes it. test/support/mmgr_oracle_libc.h is the only caller in the
 *       tree, silencing -Wfloat-conversion over the shims that point entries at libc, where the
 *       conversion is the oracle's own doing rather than the library's.
 * @note MMGR_DIAG_STR stringizes the whole pragma text, including warning_.
 */
#define MMGR_DIAG_IGNORE(warning_) _Pragma(MMGR_DIAG_STR(clang diagnostic ignored warning_))
#elif defined(__GNUC__)
/** @brief Expands to _Pragma("GCC diagnostic push") where __GNUC__ is defined and __clang__ is not. */
#define MMGR_DIAG_PUSH _Pragma("GCC diagnostic push")

/** @brief Expands to _Pragma("GCC diagnostic pop") where __GNUC__ is defined and __clang__ is not. */
#define MMGR_DIAG_POP _Pragma("GCC diagnostic pop")

/**
 * @brief Expands to _Pragma(MMGR_DIAG_STR(GCC diagnostic ignored warning_)).
 *
 * @param[in] warning_ Warning name as a string literal, such as "-Wpadded".
 * @note Selected where __GNUC__ is defined and __clang__ is not.
 * @note Suppresses one named diagnostic, and belongs between a MMGR_DIAG_PUSH and a MMGR_DIAG_POP
 *       so it ends where the code that needs it ends.
 * @note MMGR_DIAG_STR stringizes the whole pragma text, including warning_.
 */
#define MMGR_DIAG_IGNORE(warning_) _Pragma(MMGR_DIAG_STR(GCC diagnostic ignored warning_))
#else

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @note A compiler with no diagnostic pragma has nothing to save, so the bracket a suppression sits
 *       in costs nothing and the calling code needs no arm of its own.
 */
#define MMGR_DIAG_PUSH

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @note Pairs with MMGR_DIAG_PUSH, which also expands to nothing here.
 */
#define MMGR_DIAG_POP

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @param[in] warning_ Warning name as a string literal, discarded.
 * @warning The diagnostic is not suppressed on this arm. A compiler here that raises it anyway
 *          reports it, which is the safe direction, and MMGR_DIAG_STR is still defined above so
 *          nothing fails to compile.
 */
#define MMGR_DIAG_IGNORE(warning_)
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
 *       offset of one. Where a load is a single instruction, take it. Where it is a dozen, derive
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
