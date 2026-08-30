// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
/**
 * @file test_pow5_accuracy.c
 * @brief Derives all eighteen pow5 entries from exact integer arithmetic and compares each against
 *        the table, covering both significand halves, the binary exponent, and which way a dropped
 *        bit moved the value.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note Nothing here reads a table value to build an expectation. Every power of five is computed
 *       from 1 by repeated multiplication in a 1024-bit integer, so a defect in an entry cannot be
 *       masked by the same defect in what it is compared against.
 * @note The structural checks on entry count, normalization and exponent ordering live in test_pow5.
 */
#include "pow5/pow5.h"

#include "unity.h"

/**
 * @brief Expands to 32, the number of limbs a BigNumber holds.
 *
 * @note 32 limbs give 1024 bits. 5^256 is 595 bits, and the reciprocal test sets a bit 127 above
 *       that before dividing, so the widest value this suite builds reaches bit 722.
 */
#define MMGR_ACCURACY_LIMBS 32
/**
 * @brief Expands to 32, the width in bits of one BigNumber limb.
 *
 * @note Matches the mmgr_u32 the limb array holds. big_multiply_small accumulates a 32-by-32 product
 *       and its carry into an mmgr_u64, whose worst case is 2^64 minus 2^32.
 */
#define MMGR_ACCURACY_LIMB_BITS 32
/**
 * @brief Expands to 1024, the total width of a BigNumber in bits.
 *
 * @note big_get_bit returns 0 for a position at or above this rather than reading past the array.
 * @note big_divide walks from the top bit of this width rather than from the length of the numerator,
 *       so the bits it visits do not vary with the value it is handed.
 */
#define MMGR_ACCURACY_TOTAL_BITS (MMGR_ACCURACY_LIMBS * MMGR_ACCURACY_LIMB_BITS)
/**
 * @brief Expands to 6, the number of mmgr_pow5_up entries that fit 128 bits with nothing dropped.
 *
 * @note pow5.h documents index 0 through 5 as exact. 5^32 at index 5 is 75 bits and 5^64 at index 6
 *       is 149 bits, so the boundary falls here.
 * @note Counts a different thing from MMGR_POW5_SINGLE_HALF_STEPS in test_pow5, which is 5 because
 *       entry 5 still needs its low half.
 */
#define MMGR_ACCURACY_EXACT_STEPS 6

/**
 * @brief A 1024-bit unsigned integer, held as MMGR_ACCURACY_LIMBS limbs of MMGR_ACCURACY_LIMB_BITS
 *        bits each.
 *
 * @note Exists so this suite can compute a power of five exactly. 5^256 is 595 bits, which no scalar
 *       type on any environment this suite builds for can hold.
 */
typedef struct
{
    mmgr_u32 limb[MMGR_ACCURACY_LIMBS]; /**< Limbs in little-endian order, limb[0] least significant. */
} BigNumber;

/**
 * @brief Sets a BigNumber to a value that fits in one limb.
 *
 * @param[out] number Destination, zeroed across every limb before the value lands [BORROWS].
 * @param[in]  value  Value to place in limb[0].
 * @note A BigNumber with automatic storage holds indeterminate limbs until this runs, so every value
 *       this suite builds starts here.
 */
static void big_set_small(BigNumber *number, mmgr_u32 value)
{
    for (int index = 0; index < MMGR_ACCURACY_LIMBS; index++)
    {
        number->limb[index] = 0u;
    }
    number->limb[0] = value;
}

/**
 * @brief Multiplies a BigNumber in place by a value that fits in one limb.
 *
 * @param[in,out] number     Value to scale, overwritten with the product [BORROWS].
 * @param[in]     multiplier Multiplier, at most one limb wide.
 * @note A carry out of the top limb is dropped. The widest value this suite builds reaches bit 722,
 *       so that carry is zero every time this runs.
 */
static void big_multiply_small(BigNumber *number, mmgr_u32 multiplier)
{
    mmgr_u64 carry = 0u;

    for (int index = 0; index < MMGR_ACCURACY_LIMBS; index++)
    {
        // Explicit casts widen both factors to mmgr_u64 before multiplying. Two mmgr_u32 operands
        // would multiply in 32 bits and drop the high half, which is the carry this loop needs
        const mmgr_u64 product = ((mmgr_u64)number->limb[index] * (mmgr_u64)multiplier) + carry;

        // Explicit cast narrows to the low limb deliberately. The bits it drops are the ones the
        // next line keeps as the carry, so nothing is lost between the two statements
        number->limb[index] = (mmgr_u32)product;
        carry = product >> 32;
    }
}

/**
 * @brief Reads one bit of a BigNumber, answering zero for any position outside the value.
 *
 * @param[in] number   Value to read [BORROWS].
 * @param[in] position Bit index, 0 being the least significant bit of limb[0].
 * @return             1 where the bit is set, 0 where it is clear or the position is out of range.
 * @note big_top_128 depends on the out-of-range answer. It walks 128 positions down from a value's
 *       length, which runs below bit 0 for anything narrower than 128 bits, and the zeros read there
 *       are what left-align the significand.
 */
static mmgr_u32 big_get_bit(const BigNumber *number, int position)
{
    // The two halves are combined because a position is readable only when both hold, and neither
    // half carries a side effect. Dropping either one would let the line below index past an end
    if (position < 0 || position >= MMGR_ACCURACY_TOTAL_BITS)
    {
        return 0u;
    }
    return (number->limb[position / MMGR_ACCURACY_LIMB_BITS] >> (position % MMGR_ACCURACY_LIMB_BITS)) & 1u;
}

static void big_set_bit(BigNumber *number, int position)
{
    number->limb[position / MMGR_ACCURACY_LIMB_BITS] |= (uint32_t)1u << (position % MMGR_ACCURACY_LIMB_BITS);
}

static int big_bit_length(const BigNumber *number)
{
    for (int index = MMGR_ACCURACY_LIMBS - 1; index >= 0; index--)
    {
        if (number->limb[index] != 0u)
        {
            uint32_t word = number->limb[index];
            int bits = 0;

            while (word != 0u)
            {
                bits++;
                word >>= 1;
            }
            return (index * MMGR_ACCURACY_LIMB_BITS) + bits;
        }
    }
    return 0;
}

static void big_shift_left_one(BigNumber *number)
{
    uint32_t carry = 0u;

    for (int index = 0; index < MMGR_ACCURACY_LIMBS; index++)
    {
        const uint32_t carry_out = number->limb[index] >> 31;

        number->limb[index] = (number->limb[index] << 1) | carry;
        carry = carry_out;
    }
}

static int big_compare(const BigNumber *left, const BigNumber *right)
{
    for (int index = MMGR_ACCURACY_LIMBS - 1; index >= 0; index--)
    {
        if (left->limb[index] != right->limb[index])
        {
            return left->limb[index] > right->limb[index] ? 1 : -1;
        }
    }
    return 0;
}

static void big_subtract(BigNumber *left, const BigNumber *right)
{
    uint64_t borrow = 0u;

    for (int index = 0; index < MMGR_ACCURACY_LIMBS; index++)
    {
        // A wrapped difference is the borrow: the true value lives in -2^32 through 2^32, so a
        // negative one lands above 2^63 and nothing else does.
        const uint64_t difference = (uint64_t)left->limb[index] - (uint64_t)right->limb[index] - borrow;

        left->limb[index] = (uint32_t)difference;
        borrow = (difference >> 63) & 1u;
    }
}

static void big_divide(const BigNumber *numerator, const BigNumber *denominator, BigNumber *quotient)
{
    BigNumber remainder;

    big_set_small(&remainder, 0u);
    big_set_small(quotient, 0u);

    for (int position = MMGR_ACCURACY_TOTAL_BITS - 1; position >= 0; position--)
    {
        big_shift_left_one(&remainder);
        remainder.limb[0] |= big_get_bit(numerator, position);

        if (big_compare(&remainder, denominator) >= 0)
        {
            big_subtract(&remainder, denominator);
            big_set_bit(quotient, position);
        }
    }
}

static void big_top_128(const BigNumber *number, int length, uint64_t *high, uint64_t *low)
{
    uint64_t high_part = 0u;
    uint64_t low_part = 0u;

    for (int offset = 0; offset < 64; offset++)
    {
        high_part = (high_part << 1) | (uint64_t)big_get_bit(number, length - 1 - offset);
    }
    for (int offset = 64; offset < 128; offset++)
    {
        low_part = (low_part << 1) | (uint64_t)big_get_bit(number, length - 1 - offset);
    }
    *high = high_part;
    *low = low_part;
}

static void big_power_of_five(BigNumber *number, int exponent)
{
    big_set_small(number, 1u);
    for (int applied = 0; applied < exponent; applied++)
    {
        big_multiply_small(number, 5u);
    }
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_the_exact_arithmetic_this_suite_relies_on_is_itself_right(void)
{
    BigNumber value;
    uint64_t high = 0u;
    uint64_t low = 0u;

    big_power_of_five(&value, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, big_bit_length(&value), "five to the zero is one");

    big_power_of_five(&value, 16);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(152587890625u % 4294967296u, value.limb[0], "five to the sixteen is wrong");
    TEST_ASSERT_EQUAL_INT_MESSAGE(38, big_bit_length(&value), "five to the sixteen is 38 bits");

    big_top_128(&value, big_bit_length(&value), &high, &low);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x8E1BC9BF04000000ULL, high, "the top 128 bits of an exact power are misaligned");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0u, low, "a 38 bit value has nothing in its low half");

    BigNumber numerator;
    BigNumber quotient;

    big_set_small(&numerator, 0u);
    big_set_bit(&numerator, 10);
    big_power_of_five(&value, 1);
    big_divide(&numerator, &value, &quotient);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(204u, quotient.limb[0], "1024 divided by 5 is 204 remainder 4");
}

void test_every_positive_power_of_five_is_the_top_128_bits_of_the_exact_value(void)
{
    BigNumber value;

    for (int step = 0; step < MMGR_POW5_STEPS; step++)
    {
        uint64_t high = 0u;
        uint64_t low = 0u;

        big_power_of_five(&value, 1 << step);

        const int length = big_bit_length(&value);

        big_top_128(&value, length, &high, &low);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(high, mmgr_pow5_up[step].hi, "high half is not the exact power of five");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(low, mmgr_pow5_up[step].lo, "low half is not the exact power of five");
        TEST_ASSERT_EQUAL_INT_MESSAGE(length - 128, (int)mmgr_pow5_up[step].e2,
                                      "the binary exponent does not place the significand at the exact value");
    }
}

void test_the_first_six_positive_powers_lose_no_bits_at_all(void)
{
    BigNumber value;

    for (int step = 0; step < MMGR_ACCURACY_EXACT_STEPS; step++)
    {
        big_power_of_five(&value, 1 << step);
        TEST_ASSERT_LESS_OR_EQUAL_INT_MESSAGE(128, big_bit_length(&value),
                                              "a power the header calls exact does not fit 128 bits");
    }
}

void test_the_last_three_positive_powers_drop_bits_and_drop_them_downward(void)
{
    BigNumber value;

    for (int step = MMGR_ACCURACY_EXACT_STEPS; step < MMGR_POW5_STEPS; step++)
    {
        big_power_of_five(&value, 1 << step);

        const int length = big_bit_length(&value);

        TEST_ASSERT_GREATER_THAN_INT_MESSAGE(128, length, "a power the header calls truncated fits 128 bits");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, big_get_bit(&value, 0),
                                         "a power of five is odd, so its lowest bit is always among the dropped ones");
    }
}

void test_every_negative_power_of_five_is_the_exact_reciprocal_truncated_toward_zero(void)
{
    BigNumber value;
    BigNumber numerator;
    BigNumber quotient;

    for (int step = 0; step < MMGR_POW5_STEPS; step++)
    {
        uint64_t high = 0u;
        uint64_t low = 0u;

        big_power_of_five(&value, 1 << step);

        const int length = big_bit_length(&value);

        big_set_small(&numerator, 0u);
        big_set_bit(&numerator, length + 127);
        big_divide(&numerator, &value, &quotient);

        const int quotient_length = big_bit_length(&quotient);

        TEST_ASSERT_EQUAL_INT_MESSAGE(128, quotient_length, "a normalized reciprocal significand is exactly 128 bits");

        big_top_128(&quotient, quotient_length, &high, &low);

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(high, mmgr_pow5_down[step].hi,
                                        "high half is not the truncated exact reciprocal");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(low, mmgr_pow5_down[step].lo, "low half is not the truncated exact reciprocal");
        TEST_ASSERT_EQUAL_INT_MESSAGE(-(length + 127), (int)mmgr_pow5_down[step].e2,
                                      "the binary exponent does not place the reciprocal at the exact value");
    }
}

void test_no_reciprocal_was_rounded_up(void)
{
    BigNumber value;
    BigNumber numerator;
    BigNumber quotient;
    BigNumber product;

    for (int step = 0; step < MMGR_POW5_STEPS; step++)
    {
        big_power_of_five(&value, 1 << step);

        const int length = big_bit_length(&value);

        big_set_small(&numerator, 0u);
        big_set_bit(&numerator, length + 127);
        big_divide(&numerator, &value, &quotient);

        // The table entry times its power of five must stay at or below the scale it divides, since
        // rounding up is the one direction a truncated reciprocal must never go.
        product = quotient;
        for (int applied = 0; applied < (1 << step); applied++)
        {
            big_multiply_small(&product, 5u);
        }
        TEST_ASSERT_LESS_OR_EQUAL_INT_MESSAGE(0, big_compare(&product, &numerator),
                                              "a reciprocal that multiplies back above its scale was rounded up");
    }
}

void test_the_reciprocal_of_five_repeats_rather_than_rounding(void)
{
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xCCCCCCCCCCCCCCCCULL, mmgr_pow5_down[0].hi,
                                    "five to the minus one truncates to a repeating C");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xCCCCCCCCCCCCCCCCULL, mmgr_pow5_down[0].lo,
                                    "a low half ending in D is the rounded value, which this table does not use");
}
