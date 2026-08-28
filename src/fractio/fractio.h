/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief The binary64 field layout, the assertions that pin it, and the fract table.
 */
#ifndef MMGR_FRACTIO_H
#define MMGR_FRACTIO_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief The three field positions of a binary64 double, as masks, shifts and widths.
 *
 * @note The assertions below check the masks tile the word without gap or overlap.
 * @note Every constant carries a ull suffix, matching the mmgr_u64 the fields are read from.
 */
#define MMGR_DBL_SIGN_MASK 0x8000000000000000ull /**< The sign bit. */
#define MMGR_DBL_EXP_MASK 0x7FF0000000000000ull  /**< The eleven exponent bits. */
#define MMGR_DBL_MANT_MASK 0x000FFFFFFFFFFFFFull /**< The fifty-two stored mantissa bits. */
#define MMGR_DBL_SIGN_SHIFT 63u                  /**< Bit position of the sign. */
#define MMGR_DBL_MANT_BITS 52u                   /**< Stored mantissa width, and the exponent's shift. */
#define MMGR_DBL_EXP_BITS 11u                    /**< Exponent width. */
#define MMGR_DBL_SIGN_ONE 0x1ull                 /**< A sign of one, before shifting. */
#define MMGR_DBL_EXP_ALL 0x7FFull                /**< An exponent field of all ones. */
#define MMGR_DBL_BIAS 1023                       /**< Amount added to the true exponent when stored. */

/**
 * @brief The width of a double in bits, bytes and mmgr_word units.
 *
 * @note MMGR_DBL_WORDS rounds up, and the assertion below requires it to come out exact.
 */
#define MMGR_DBL_BITS 64u                                                         /**< Bits in a double. */
#define MMGR_DBL_BYTES (MMGR_DBL_BITS / 8u)                                       /**< Bytes in a double. */
#define MMGR_DBL_WORDS ((MMGR_DBL_BITS + (MMGR_WORD_BITS - 1u)) / MMGR_WORD_BITS) /**< mmgr_word units per double. */

/**
 * @brief Pins the target's double to the width and storage this file assumes.
 *
 * @note Checks the bit width, that a double is a whole number of mmgr_word units, and that mmgr_u64 holds it.
 */
MMGR_STATIC_ASSERT(sizeof(double) * 8u == MMGR_DBL_BITS,
                   "double is not 64 bits on this target, so every field position in this file is wrong "
                   "- a build where double means float cannot use it");
MMGR_STATIC_ASSERT(MMGR_DBL_WORDS *MMGR_WORD_BITS == MMGR_DBL_BITS,
                   "a double is not a whole number of words on this target");
MMGR_STATIC_ASSERT(sizeof(mmgr_u64) == sizeof(double), "the bit pattern of a double does not fit mmgr_u64");

/**
 * @brief Pins the field constants above against each other.
 *
 * @note Checks the widths sum to 64, the masks tile the word, and no two masks overlap.
 * @note Also checks each mask agrees with the shift and width that describe the same field.
 */
MMGR_STATIC_ASSERT(1u + MMGR_DBL_EXP_BITS + MMGR_DBL_MANT_BITS == MMGR_DBL_BITS,
                   "the three fields do not add up to the width of the value");
MMGR_STATIC_ASSERT((MMGR_DBL_SIGN_MASK | MMGR_DBL_EXP_MASK | MMGR_DBL_MANT_MASK) == 0xFFFFFFFFFFFFFFFFull,
                   "the three field masks leave a gap");
MMGR_STATIC_ASSERT((MMGR_DBL_SIGN_MASK & MMGR_DBL_EXP_MASK) == 0u && (MMGR_DBL_EXP_MASK & MMGR_DBL_MANT_MASK) == 0u &&
                       (MMGR_DBL_SIGN_MASK & MMGR_DBL_MANT_MASK) == 0u,
                   "the three field masks overlap");
MMGR_STATIC_ASSERT(MMGR_DBL_SIGN_MASK == (MMGR_DBL_SIGN_ONE << MMGR_DBL_SIGN_SHIFT),
                   "the sign mask and the sign shift disagree about where the sign is");
MMGR_STATIC_ASSERT(MMGR_DBL_EXP_MASK == (MMGR_DBL_EXP_ALL << MMGR_DBL_MANT_BITS),
                   "the exponent mask and the exponent width disagree");
MMGR_STATIC_ASSERT(MMGR_DBL_EXP_ALL == ((1u << MMGR_DBL_EXP_BITS) - 1u), "the exponent does not fill its field");
MMGR_STATIC_ASSERT(MMGR_DBL_BIAS == ((1 << (MMGR_DBL_EXP_BITS - 1u)) - 1), "the bias is not the one binary64 uses");

/**
 * @brief The range of powers of two a double can carry once the mantissa is treated as an integer.
 *
 * @note MMGR_DBL_SCALE_MAX takes the largest finite exponent, removes the bias, and drops the mantissa width.
 * @note MMGR_DBL_SCALE_MIN starts from an exponent field of 1, which is the smallest normal.
 * @note Explicit casts hold both expressions in mmgr_iword, since each result is negative or near zero.
 */
#define MMGR_DBL_SCALE_MAX ((mmgr_iword)(MMGR_DBL_EXP_ALL - 1u) - MMGR_DBL_BIAS - (mmgr_iword)MMGR_DBL_MANT_BITS)
#define MMGR_DBL_SCALE_MIN (1 - MMGR_DBL_BIAS - (mmgr_iword)MMGR_DBL_MANT_BITS)

/**
 * @brief Pins the two scale bounds to the values binary64 gives.
 */
MMGR_STATIC_ASSERT(MMGR_DBL_SCALE_MAX == 971, "the largest scale a finite double can carry is not what it was");
MMGR_STATIC_ASSERT(MMGR_DBL_SCALE_MIN == -1074, "the smallest scale a subnormal can carry is not what it was");

/**
 * @brief Arguments for the fract calls; each reads only what it needs.
 *
 * @note val and bits share storage, so writing one and reading the other reinterprets the same bytes.
 * @note sign, exp and mant are read by merge alone; the other five calls read the union.
 */
typedef struct
{
    union {
        const double val;    /**< The value, when the caller supplies a double. */
        const mmgr_u64 bits; /**< The same storage read as a bit pattern. */
    };
    const mmgr_u64 sign; /**< Sign for merge, 0 or 1. */
    const mmgr_u64 exp;  /**< Biased exponent for merge. */
    const mmgr_u64 mant; /**< Stored mantissa for merge. */
} FractioCfg;

/**
 * @brief Type of the fract dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the six members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 */
typedef struct
{
    mmgr_u64 (*sign)(const FractioCfg *args);    /**< Sign bit of bits, as 0 or 1. */
    mmgr_u64 (*exp)(const FractioCfg *args);     /**< Biased exponent field of bits. */
    mmgr_u64 (*mant)(const FractioCfg *args);    /**< Stored mantissa field of bits. */
    mmgr_u64 (*merge)(const FractioCfg *args);   /**< Packs sign, exp and mant into one pattern. */
    double (*from_bits)(const FractioCfg *args); /**< Reads the union as a double. */
    mmgr_u64 (*to_bits)(const FractioCfg *args); /**< Reads the union as a bit pattern. */
} FractioNs;
MMGR_NS_LAYOUT(FractioNs, sign, exp, mant, merge, from_bits, to_bits);

/**
 * @brief Returns the sign bit of args->bits.
 *
 * @param[in] args Bit pattern in the union [BORROWS].
 * @return      0 for a positive sign, 1 for a negative one.
 */
mmgr_u64 mmgr_fract_sign(const FractioCfg *args);

/**
 * @brief Returns the exponent field of args->bits.
 *
 * @param[in] args Bit pattern in the union [BORROWS].
 * @return      The stored exponent, still carrying MMGR_DBL_BIAS.
 * @note 0 marks a zero or subnormal; MMGR_DBL_EXP_ALL marks an infinity or NaN.
 */
mmgr_u64 mmgr_fract_exp(const FractioCfg *args);

/**
 * @brief Returns the mantissa field of args->bits.
 *
 * @param[in] args Bit pattern in the union [BORROWS].
 * @return      The fifty-two stored bits, without the implicit leading one.
 */
mmgr_u64 mmgr_fract_mant(const FractioCfg *args);

/**
 * @brief Packs args->sign, args->exp and args->mant into one bit pattern.
 *
 * @param[in] args The three fields [BORROWS].
 * @return      The assembled pattern.
 * @note Each field is masked to its own width, so a wide input cannot reach a neighboring field.
 */
mmgr_u64 mmgr_fract_merge(const FractioCfg *args);

/**
 * @brief Reads the union as a double.
 *
 * @param[in] args Union with its bits member filled [BORROWS].
 * @return      The same storage interpreted as a double.
 */
double mmgr_fract_from_bits(const FractioCfg *args);

/**
 * @brief Reads the union as a bit pattern.
 *
 * @param[in] args Union with its val member filled [BORROWS].
 * @return      The same storage interpreted as mmgr_u64.
 */
mmgr_u64 mmgr_fract_to_bits(const FractioCfg *args);

/**
 * @brief Dispatch table instance named fract; each member calls the matching mmgr_fract_ function.
 */
MMGR_NS FractioNs fract MMGR_UNUSED = {
    .sign = mmgr_fract_sign,
    .exp = mmgr_fract_exp,
    .mant = mmgr_fract_mant,
    .merge = mmgr_fract_merge,
    .from_bits = mmgr_fract_from_bits,
    .to_bits = mmgr_fract_to_bits,
};

MMGR_FINIS_DECLS

#endif
