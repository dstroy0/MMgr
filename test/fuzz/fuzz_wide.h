/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file fuzz_wide.h
 * @brief The exact unsigned integer the numeric harnesses compute their oracles in: WIDE_LIMBS limbs of 32 bits,
 *        multiplied and divided by a small factor, shifted, compared, subtracted and divided by another wide
 *        integer one bit at a time. Slow and plainly right, as an oracle must be.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#ifndef MMGR_FUZZ_WIDE_H
#define MMGR_FUZZ_WIDE_H

#include <stdint.h>
#include <string.h>

#define WIDE_LIMBS 48u

/** @brief An unsigned integer of WIDE_LIMBS 32-bit limbs, the low limb first. */
typedef struct
{
    uint32_t limb[WIDE_LIMBS];
} Wide;

/** @brief The wide integer holding @p value. */
static inline Wide wide_of(uint64_t value)
{
    Wide wide;
    memset(&wide, 0, sizeof wide);
    wide.limb[0] = (uint32_t)value;
    wide.limb[1] = (uint32_t)(value >> 32);
    return wide;
}

/** @brief The count of significant bits of @p wide, 0 for zero. */
static inline unsigned int wide_bits(const Wide *wide)
{
    for (unsigned int at = WIDE_LIMBS; at > 0u; at -= 1u)
    {
        const uint32_t limb = wide->limb[at - 1u];
        if (limb != 0u)
        {
            unsigned int bits = 32u;
            while (((limb >> (bits - 1u)) & 1u) == 0u)
            {
                bits -= 1u;
            }
            return (at - 1u) * 32u + bits;
        }
    }
    return 0u;
}

/** @brief Bit @p bit of @p wide. */
static inline unsigned int wide_bit(const Wide *wide, unsigned int bit)
{
    return (wide->limb[bit / 32u] >> (bit % 32u)) & 1u;
}

/** @brief @p wide times the small @p factor, in place. */
static inline void wide_times(Wide *wide, uint32_t factor)
{
    uint64_t carry = 0u;
    for (unsigned int at = 0u; at < WIDE_LIMBS; at += 1u)
    {
        const uint64_t product = (uint64_t)wide->limb[at] * factor + carry;
        wide->limb[at] = (uint32_t)product;
        carry = product >> 32;
    }
}

/** @brief @p wide floored by the small @p divisor, in place. Flooring twice is flooring by the product. */
static inline void wide_over(Wide *wide, uint32_t divisor)
{
    uint64_t remainder = 0u;
    for (unsigned int at = WIDE_LIMBS; at > 0u; at -= 1u)
    {
        const uint64_t part = (remainder << 32) | wide->limb[at - 1u];
        wide->limb[at - 1u] = (uint32_t)(part / divisor);
        remainder = part % divisor;
    }
}

/** @brief @p wide shifted up by @p shift bits, in place. */
static inline void wide_shift_up(Wide *wide, unsigned int shift)
{
    for (unsigned int step = 0u; step < shift; step += 1u)
    {
        wide_times(wide, 2u);
    }
}

/** @brief -1, 0 or 1 as @p left is below, equal to or above @p right. */
static inline int wide_order(const Wide *left, const Wide *right)
{
    for (unsigned int at = WIDE_LIMBS; at > 0u; at -= 1u)
    {
        if (left->limb[at - 1u] != right->limb[at - 1u])
        {
            return (left->limb[at - 1u] < right->limb[at - 1u]) ? -1 : 1;
        }
    }
    return 0;
}

/** @brief @p left minus @p right, in place, where @p left is not below @p right. */
static inline void wide_minus(Wide *left, const Wide *right)
{
    uint64_t borrow = 0u;
    for (unsigned int at = 0u; at < WIDE_LIMBS; at += 1u)
    {
        const uint64_t taken = (uint64_t)right->limb[at] + borrow;
        borrow = ((uint64_t)left->limb[at] < taken) ? 1u : 0u;
        left->limb[at] = (uint32_t)(((uint64_t)left->limb[at] + (borrow << 32)) - taken);
    }
}

/** @brief @p wide plus @p right, in place. */
static inline void wide_plus(Wide *wide, const Wide *right)
{
    uint64_t carry = 0u;
    for (unsigned int at = 0u; at < WIDE_LIMBS; at += 1u)
    {
        const uint64_t sum = (uint64_t)wide->limb[at] + right->limb[at] + carry;
        wide->limb[at] = (uint32_t)sum;
        carry = sum >> 32;
    }
}

/** @brief The floor of @p numerator over a nonzero @p denominator, by shift and subtract. */
static inline Wide wide_quotient(const Wide *numerator, const Wide *denominator)
{
    Wide quotient;
    Wide remainder;
    memset(&quotient, 0, sizeof quotient);
    memset(&remainder, 0, sizeof remainder);
    for (unsigned int bit = wide_bits(numerator); bit > 0u; bit -= 1u)
    {
        wide_times(&remainder, 2u);
        remainder.limb[0] |= wide_bit(numerator, bit - 1u);
        if (wide_order(&remainder, denominator) >= 0)
        {
            wide_minus(&remainder, denominator);
            quotient.limb[(bit - 1u) / 32u] |= (uint32_t)1u << ((bit - 1u) % 32u);
        }
    }
    return quotient;
}

#endif
