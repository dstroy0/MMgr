/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file transformo.h
 * @brief Decimal to binary conversion, the limits, the arguments, and the muto dispatch table.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note take builds a mantissa one digit at a time, and take_in does the same in a base from 2 to 36.
 * @note scale turns one into a double, and scale_to_u64 turns one into an integer.
 */
#ifndef MMGR_TRANSFORMO_H
#define MMGR_TRANSFORMO_H

/** @addtogroup mod_muto
 *  @{ */

#include "fractio/fractio.h"
#include "mmgr.h"
#include "pow5/pow5.h"

EMBED_BEGIN_DECLS

/**
 * @brief Expands to the largest mantissa mmgr_muto_take will extend, which is (~0 - 9) / 10.
 *
 * @note Chosen so a multiply by ten and an add of a digit of nine both stay inside an embed_u64.
 * @note mmgr_muto_take returns EMBED_FALSE and appends nothing once the mantissa passes this.
 * @note Explicit casts hold both the complement and the result at embed_u64, and the 9u and 10u
 *       match that width. So the subtraction and the divide run at the mantissa's own width
 *       and not at int's.
 */
#define MMGR_MUTO_MANT_MAX ((embed_u64)((~(embed_u64)0 - 9u) / 10u))

/**
 * @brief Expands to 400, the count at which a parsed decimal exponent stops its growth.
 *
 * @note cellul_expo tests its running exponent against this before each multiply and add. A long run
 *       of digits cannot run the count away with the embed_iword that holds it.
 * @note 400 is past the 308 a double reaches. Anything the clamp holds back has already gone to a
 *       signed infinity or a signed zero. An early stop of the count cannot change the result.
 * @note The literal is a plain number, as the embed_iword exponent it compares against.
 */
#define MMGR_MUTO_EXP_LIMIT 400

/**
 * @brief Arguments for the four muto calls.
 *
 * @note take reads mant and digit, and take_in reads mant, digit and base.
 * @note scale reads mant, ex, rest and neg.
 * @note scale_to_u64 reads mant, e2 and ex.
 * @warning take and take_in write through mant, and so the caller's mantissa changes [BORROWS].
 */
typedef struct
{
    embed_u64 *const mant;   /**< The mantissa, which take extends and both scale calls read [BORROWS]. */
    const char digit;        /**< ASCII digit take and take_in append. */
    const embed_iword e2;    /**< Binary exponent the mantissa already carries, read by scale_to_u64. */
    const embed_iword ex;    /**< Decimal exponent to apply. */
    const embed_iword rest;  /**< Non-zero when the caller already dropped bits, and so the round can see them. */
    const embed_bool neg;    /**< Sign scale gives its result. */
    const unsigned int base; /**< Base take_in reads the digit in, from 2 to 36. */
} TransformoCfg;

/**
 * @brief Type of the muto dispatch table.
 *
 * @note EMBED_TABLE_LAYOUT asserts the four members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets, with
 * nothing else.
 */
typedef struct
{
    embed_bool (*take)(const TransformoCfg *args);        /**< Appends one decimal digit to a mantissa. */
    double (*scale)(const TransformoCfg *args);           /**< Turns a mantissa and decimal exponent into a double. */
    embed_u64 (*scale_to_u64)(const TransformoCfg *args); /**< Turns the same plus e2 into a rounded 64-bit integer. */
    embed_bool (*take_in)(const TransformoCfg *args);     /**< Appends one digit of a base to a mantissa. */
} TransformoNs;
EMBED_TABLE_LAYOUT(TransformoNs, take, scale, scale_to_u64, take_in);

/**
 * @brief The value of an ASCII digit in a base up to 36.
 *
 * @param[in] character The byte.
 * @return              0 to 9 for '0' to '9', 10 to 35 for a letter of either case, and 36 for any other byte.
 * @note A value at or past the base says the byte is no digit of that base, and 36 is past every base.
 */
EMBED_HOST_DEVICE EMBED_INLINE unsigned int mmgr_muto_digit_value(uint8_t character)
{
    const unsigned int decimal = (unsigned int)character - (unsigned int)'0';
    // A letter of either case folds to its lower case with the bit of 0x20 set
    const unsigned int letter = ((unsigned int)character | 0x20u) - (unsigned int)'a';
    return (decimal < 10u) ? decimal : ((letter < 26u) ? (letter + 10u) : 36u);
}

/**
 * @brief Appends args->digit to *args->mant as one more decimal digit.
 *
 * @param[in] args The mantissa to extend and the digit to append [BORROWS].
 * @return         EMBED_TRUE when the digit went on, EMBED_FALSE when *args->mant already passed
 *                 MMGR_MUTO_MANT_MAX.
 * @note On EMBED_FALSE the mantissa stays as it was. A caller can count the digits it had to drop.
 * @warning args->digit must be an ASCII decimal digit, since the value added is args->digit minus '0'.
 * @warning Writes through args->mant [BORROWS].
 */
EMBED_HOST_DEVICE embed_bool mmgr_muto_take(const TransformoCfg *args);

/**
 * @brief Turns *args->mant times ten raised to args->ex into a double, signed by args->neg.
 *
 * @param[in] args The mantissa, the decimal exponent, the dropped bits and the sign [BORROWS].
 * @return         The nearest double, with ties to even.
 * @note Set args->rest when digits fell off the mantissa, and so the round still accounts for them.
 * @note Takes a plain double path when nothing fell off, the mantissa is under 2^53 and args->ex is within 22.
 * @note Does not read args->e2, and so the mantissa counts as a plain integer.
 * @warning Returns a signed infinity for an args->ex above MMGR_POW5_MAX and a signed zero below its negative.
 */
EMBED_HOST_DEVICE double mmgr_muto_scale(const TransformoCfg *args);

/**
 * @brief Turns *args->mant times two raised to args->e2 times ten raised to args->ex into a rounded 64-bit integer.
 *
 * @param[in] args The mantissa, its binary exponent and the decimal exponent [BORROWS].
 * @return         The nearest integer with a half taken up, 0 when the value rounds below one, or all
 *                 ones when too large.
 * @note Reads args->e2, which mmgr_muto_scale leaves alone, and so the mantissa may carry a binary exponent here.
 * @note A half goes up whatever lies below it, and so the result is the magnitude rounded away from
 *       zero. mmgr_muto_scale rounds its own halves to even, as a double wants and what
 *       this deliberately does not do.
 * @note Reads neither args->rest, args->above nor args->neg, and so the result is always unsigned.
 * @warning Does not bound args->ex against MMGR_POW5_MAX the way mmgr_muto_scale does. Only
 *          MMGR_POW5_STEPS bits of the magnitude take part in the walk. A larger exponent loses its high
 *          bits and the value scaled is not the one asked for.
 */
EMBED_HOST_DEVICE embed_u64 mmgr_muto_scale_to_u64(const TransformoCfg *args);

/**
 * @brief Appends args->digit to *args->mant as one more digit in base args->base.
 *
 * @param[in] args The mantissa to extend, the digit to append and its base [BORROWS].
 * @return         EMBED_TRUE when the digit went on. EMBED_FALSE when the base is outside 2 to 36, when the byte is
 *                 no digit of the base, or when the digit would carry the mantissa past all ones.
 * @note On EMBED_FALSE the mantissa stays as it was. A caller tells an overflow from a byte that is no digit with
 *       mmgr_muto_digit_value.
 * @note A digit is '0' to '9', then a letter of either case for 10 to 35.
 * @warning Writes through args->mant [BORROWS].
 */
EMBED_HOST_DEVICE embed_bool mmgr_muto_take_in(const TransformoCfg *args);

/**
 * @brief Dispatch table instance named muto, with each member set to its mmgr_muto_ function.
 */
EMBED_TABLE_STORAGE TransformoNs muto EMBED_UNUSED = {
    .take = mmgr_muto_take,
    .scale = mmgr_muto_scale,
    .scale_to_u64 = mmgr_muto_scale_to_u64,
    .take_in = mmgr_muto_take_in,
};

EMBED_END_DECLS

/** @} */

#endif
