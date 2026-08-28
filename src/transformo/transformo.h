/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Decimal to binary conversion: the limits, the arguments, and the muto dispatch table.
 *
 * @note take builds a mantissa one digit at a time; scale and scale_to_u64 turn one into a double or an integer.
 */
#ifndef MMGR_TRANSFORMO_H
#define MMGR_TRANSFORMO_H

#include "config/mmgr_config.h"
#include "fractio/fractio.h"
#include "pow5/pow5.h"

MMGR_INCIPE_DECLS

/**
 * @brief Expands to the largest mantissa mmgr_muto_take will extend, which is (~0 - 9) / 10.
 *
 * @note Chosen so multiplying by ten and adding a digit of nine both stay inside an mmgr_u64.
 * @note mmgr_muto_take returns MMGR_FALSE rather than appending once the mantissa passes this.
 */
#define MMGR_MUTO_MANT_MAX ((mmgr_u64)((~(mmgr_u64)0 - 9u) / 10u))

/**
 * @brief Expands to 400.
 *
 * @note transformo.c reads neither this nor any bound derived from it; muto_scale bounds args->ex against
 *       MMGR_POW5_MAX instead.
 */
#define MMGR_MUTO_EXP_LIMIT 400

/**
 * @brief Arguments for the three muto calls.
 *
 * @note take reads mant and digit; scale reads mant, ex, rest and neg; scale_to_u64 reads mant, e2, ex and above.
 * @warning mant is written through by take, so the caller's mantissa changes [BORROWS].
 */
typedef struct
{
    mmgr_u64 *const mant;  /**< The mantissa, extended by take and read by both scaling calls [BORROWS]. */
    const char digit;      /**< ASCII decimal digit take appends. */
    const mmgr_iword e2;   /**< Binary exponent the mantissa already carries, read by scale_to_u64. */
    const mmgr_iword ex;   /**< Decimal exponent to apply. */
    const mmgr_iword rest; /**< Non-zero when the caller already dropped bits, so rounding can see them. */
    const mmgr_word above; /**< Low bit steers which way scale_to_u64 breaks a tie. */
    const mmgr_bool neg;   /**< Sign scale gives its result. */
} TransformoCfg;

/**
 * @brief Type of the muto dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the three members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 */
typedef struct
{
    mmgr_bool (*take)(const TransformoCfg *args);        /**< Appends one decimal digit to a mantissa. */
    double (*scale)(const TransformoCfg *args);          /**< Turns a mantissa and decimal exponent into a double. */
    mmgr_u64 (*scale_to_u64)(const TransformoCfg *args); /**< Turns the same into a rounded 64-bit integer. */
} TransformoNs;
MMGR_NS_LAYOUT(TransformoNs, take, scale, scale_to_u64);

/**
 * @brief Appends args->digit to *args->mant as one more decimal digit.
 *
 * @param[in] args The mantissa to extend and the digit to append [BORROWS].
 * @return      MMGR_TRUE when the digit was appended, MMGR_FALSE when *args->mant already passed MMGR_MUTO_MANT_MAX.
 * @note On MMGR_FALSE the mantissa is left as it was, so a caller can count the digits it had to drop.
 * @warning args->digit must be an ASCII decimal digit, since the value added is args->digit minus '0'.
 * @warning Writes through args->mant [BORROWS].
 */
mmgr_bool mmgr_muto_take(const TransformoCfg *args);

/**
 * @brief Turns *args->mant times ten raised to args->ex into a double, signed by args->neg.
 *
 * @param[in] args The mantissa, the decimal exponent, the dropped bits and the sign [BORROWS].
 * @return      The nearest double, with ties going to even.
 * @note Set args->rest when digits were dropped from the mantissa, so the rounding still accounts for them.
 * @note Takes a plain double path when nothing was dropped, the mantissa is under 2^53 and args->ex is within 22.
 * @note Does not read args->e2, so the mantissa is taken as a plain integer.
 * @warning Returns a signed infinity for an args->ex above MMGR_POW5_MAX and a signed zero below its negative.
 */
double mmgr_muto_scale(const TransformoCfg *args);

/**
 * @brief Turns *args->mant times two raised to args->e2 times ten raised to args->ex into a rounded 64-bit integer.
 *
 * @param[in] args The mantissa, its binary exponent, the decimal exponent and the tie bias [BORROWS].
 * @return      The nearest integer with ties to even, 0 when the value rounds below one, or all ones when too large.
 * @note Reads args->e2, which mmgr_muto_scale leaves alone, so the mantissa may carry a binary exponent here.
 * @note The low bit of args->above is exclusive-ored into the tie test, so a caller can steer a halfway case.
 * @note Does not read args->neg, so the result is always unsigned.
 * @warning Does not bound args->ex against MMGR_POW5_MAX the way mmgr_muto_scale does.
 */
mmgr_u64 mmgr_muto_scale_to_u64(const TransformoCfg *args);

/**
 * @brief Dispatch table instance named muto; each member calls the matching mmgr_muto_ function.
 */
MMGR_NS TransformoNs muto MMGR_UNUSED = {
    .take = mmgr_muto_take,
    .scale = mmgr_muto_scale,
    .scale_to_u64 = mmgr_muto_scale_to_u64,
};

MMGR_FINIS_DECLS

#endif
