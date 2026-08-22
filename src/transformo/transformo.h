// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_TRANSFORMO_H
#define MMGR_TRANSFORMO_H

#include "fractio/fractio.h"
#include "config/mmgr_config.h"
#include "pow5/pow5.h"

MMGR_INCIPE_DECLS

/**
 * @file transformo.h
 * @brief Turning a decimal mantissa and exponent into the double it names, exactly.
 *
 * A decimal value is mant times ten to the ex, and ten to the ex is five to the ex times two to the
 * ex. The twos never need arithmetic - a power of two is the exponent field, so applying one is an
 * add to that field and cannot round. Only the fives need carrying, and they are the same nine
 * numbers every time, which is what pow5 holds.
 *
 * The intermediate is 128 bits and never grows. An exact decimal expansion would need thousands -
 * a subnormal wants five to the 1074th, which is a 2494 bit integer - but nobody needs the
 * expansion. What is needed is enough bits to decide the rounding: fifty three that become the
 * mantissa, one below them, and one bit saying whether anything at all is set under that. A
 * hundred and twenty eight carries all three with seventy four to spare, because normalising after
 * each step holds the fraction in place and pushes the growth into an int.
 *
 * The rounding reads the same three places every time, whatever the value was. Round bit clear is
 * down. Set with something below is up. Set with nothing below is the tie, and the tie goes to
 * even. Nothing here looks at what the value was.
 *
 * This is the engine, not a policy. It was written inside the parser and lives here because the
 * render side has the same problem in the other direction and should not solve it twice. See
 * @ref qa_numeric.
 *
 * The table is the whole surface. There are no free functions to call.
 *
 * take and clz are in this header rather than the .c because both sit inside a digit loop, where a
 * call frame costs more than the body. The two that carry the intermediate are in the .c, because
 * always_inline put a whole copy of the engine into every module that converted a number once
 * there were two of them.
 */

/** @brief Largest mantissa that can still take another digit without wrapping. */
#define MMGR_MUTO_MANT_MAX ((mmgr_u64)((~(mmgr_u64)0 - 9u) / 10u))

/** @brief Beyond this the value is an infinity or a zero and the digits stop mattering. */
#define MMGR_MUTO_EXP_LIMIT 400

/**
 * @brief What a conversion is given.
 *
 * Public, and in the header, because the caller is what builds it. The mantissa is reached through
 * the whole way down: it is already in the caller's memory and this is its address, so there is
 * nothing to copy in and nothing to write back.
 *
 * Not the module's context. MutoCtx is what the bodies work with - it carries the hundred and
 * twenty eight bit intermediate and the table entry being applied - and it is file local in the .c.
 */
typedef struct
{
    mmgr_u64 *const mant; /**< The digits, as an integer. A take adds to it in place. */
    const char digit;     /**< The digit being taken. */
    const int e2;         /**< Binary exponent that goes with the mantissa. */
    const int ex;         /**< Decimal exponent to apply. */
    const int rest;       /**< Digits were dropped past what the mantissa could hold. */
    const unsigned above; /**< Parity of the rest of the number this is a field of. */
    const mmgr_bool neg;  /**< The value was negative. */
} TransformoCfg;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_bool (*take)(const TransformoCfg *c);
    double (*scale)(const TransformoCfg *c);
    mmgr_u64 (*scale_to_u64)(const TransformoCfg *c);
} TransformoNs;
MMGR_NS_LAYOUT(TransformoNs, take, scale, scale_to_u64);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
mmgr_bool mmgr_muto_take(const TransformoCfg *c);
double mmgr_muto_scale(const TransformoCfg *c);
mmgr_u64 mmgr_muto_scale_to_u64(const TransformoCfg *c);
/** @} */

/** @name The types a conversion takes, settled where the call is written.
 *  @{ */
#define MMGR_MUTO_IS_MANT(x_) ((void)_Generic((x_), mmgr_u64: 0))
#define MMGR_MUTO_IS_DIGIT(x_) ((void)_Generic((x_), char: 0))
#define MMGR_MUTO_IS_EXP(x_) ((void)_Generic((x_), int: 0))
#define MMGR_MUTO_IS_ABOVE(x_) ((void)_Generic((x_), unsigned: 0))
#define MMGR_MUTO_IS_NEG(x_) ((void)_Generic((x_), mmgr_bool: 0, int: 0))
/** @} */

/**
 * @name Calling through the table, without the struct at the call site.
 * @brief Each expands to a call through @c muto, because the table is the whole surface. What the
 *        macro adds is the config: positional in, types settled where the call is written, and the
 *        address of the mantissa taken here rather than by the caller.
 * @{ */

/** @brief Take one more digit into @p mant_, which the take adds to in place. */
#define mmgr_muto_take(mant_, digit_)                                                                                  \
    (MMGR_MUTO_IS_MANT(mant_), MMGR_MUTO_IS_DIGIT(digit_),                                                             \
     muto.take(&(TransformoCfg){.mant = &(mant_), .digit = (digit_)}))

/** @brief The double that @p mant_ times ten to the @p ex_ names. */
#define mmgr_muto_scale(mant_, ex_, rest_, neg_)                                                                       \
    (MMGR_MUTO_IS_MANT(mant_), MMGR_MUTO_IS_EXP(ex_), MMGR_MUTO_IS_EXP(rest_), MMGR_MUTO_IS_NEG(neg_),                 \
     muto.scale(&(TransformoCfg){.mant = &(mant_), .ex = (ex_), .rest = (rest_), .neg = (neg_)}))

/** @brief The same, landing in a u64 field rather than a double. */
#define mmgr_muto_scale_to_u64(mant_, e2_, ex_, above_)                                                                \
    (MMGR_MUTO_IS_MANT(mant_), MMGR_MUTO_IS_EXP(e2_), MMGR_MUTO_IS_EXP(ex_), MMGR_MUTO_IS_ABOVE(above_),               \
     muto.scale_to_u64(&(TransformoCfg){.mant = &(mant_), .e2 = (e2_), .ex = (ex_), .above = (above_)}))
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS TransformoNs muto MMGR_UNUSED = {
    .take = mmgr_muto_take,
    .scale = mmgr_muto_scale,
    .scale_to_u64 = mmgr_muto_scale_to_u64,
};

MMGR_FINIS_DECLS

#endif
