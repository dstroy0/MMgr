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
 * @note Every integer type below comes from stdint.h rather than from the library's width aliases.
 *       A wrong alias would resize the limbs this suite computes in, and each comparison against a
 *       table entry would then run at a width defined by the code being checked.
 * @note The structural checks on entry count, normalization and exponent ordering live in test_pow5.
 */
#include <stdint.h>

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
 * @note Matches the uint32_t the limb array holds. big_multiply_small accumulates a 32-by-32 product
 *       and its carry into a uint64_t, whose worst case is 2^64 minus 2^32.
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
 * @brief Expands to the number of entries mmgr_pow5_up actually holds.
 *
 * @note Sized from the array rather than from MMGR_POW5_STEPS, so a table that gained or lost an
 *       entry is walked in full without an edit here. Taking the count from the library would let
 *       the library decide how much of itself this suite checks.
 * @note The cast takes the sizeof quotient from size_t into the int a loop counter carries. The
 *       assertion below holds the count at 9, which is positive and far inside int.
 */
#define MMGR_ACCURACY_UP_ENTRIES ((int)(sizeof mmgr_pow5_up / sizeof mmgr_pow5_up[0]))

/**
 * @brief Expands to the number of entries mmgr_pow5_down actually holds.
 *
 * @note Sized from its own array rather than shared with MMGR_ACCURACY_UP_ENTRIES, so a change to
 *       one table cannot silently set the bound for walks over the other.
 */
#define MMGR_ACCURACY_DOWN_ENTRIES ((int)(sizeof mmgr_pow5_down / sizeof mmgr_pow5_down[0]))

/**
 * @brief Asserts mmgr_pow5_up still holds the nine entries this suite can represent.
 *
 * @note Entry 9 would be 5^512 at 1189 bits, and the reciprocal cases shift a further 127 above
 *       that. Both run past MMGR_ACCURACY_TOTAL_BITS, where big_multiply_small drops the carry off
 *       the top limb and every comparison against the table becomes meaningless.
 */
MMGR_STATIC_ASSERT(MMGR_ACCURACY_UP_ENTRIES == 9,
                   "mmgr_pow5_up changed length; a tenth power needs more bits than a BigNumber holds");

/**
 * @brief Asserts mmgr_pow5_down still holds nine entries, matching mmgr_pow5_up.
 *
 * @note The reciprocal of each up entry is what this table carries, so a length disagreeing with
 *       the up table would leave powers with no reciprocal or reciprocals with no power.
 */
MMGR_STATIC_ASSERT(MMGR_ACCURACY_DOWN_ENTRIES == MMGR_ACCURACY_UP_ENTRIES,
                   "mmgr_pow5_down does not hold one reciprocal per mmgr_pow5_up entry");

/**
 * @brief A 1024-bit unsigned integer, held as MMGR_ACCURACY_LIMBS limbs of MMGR_ACCURACY_LIMB_BITS
 *        bits each.
 *
 * @note Exists so this suite can compute a power of five exactly. 5^256 is 595 bits, which no scalar
 *       type on any environment this suite builds for can hold.
 */
typedef struct
{
    uint32_t limb[MMGR_ACCURACY_LIMBS]; /**< Limbs in little-endian order, limb[0] least significant. */
} BigNumber;

/**
 * @brief Sets a BigNumber to a value that fits in one limb.
 *
 * @param[out] number Destination, zeroed across every limb before the value lands [BORROWS].
 * @param[in]  value  Value to place in limb[0].
 * @note A BigNumber with automatic storage holds indeterminate limbs until this runs, so every value
 *       this suite builds starts here.
 */
static void big_set_small(BigNumber *number, uint32_t value)
{
    for (int index = 0; index < MMGR_ACCURACY_LIMBS; index++)
    {
        number->limb[index] = 0u;
    }
    number->limb[0] = value;
}

/**
 * @brief Loads a 128-bit significand into a BigNumber from its two 64-bit halves.
 *
 * @param[out] number Destination, zeroed across every limb before the halves land [BORROWS].
 * @param[in]  high   Bits 64 through 127 of the significand.
 * @param[in]  low    Bits 0 through 63 of the significand.
 * @note Exists so a table entry can be multiplied back out against the scale it divides. Every other
 *       value this suite handles is built here from 1, and an mmgr_pow5_down significand is the one
 *       input that arrives already assembled.
 * @warning Writes limb[0] through limb[3] by name, so it places 128 bits only while
 *          MMGR_ACCURACY_LIMB_BITS is 32. A narrower limb would leave the top of the significand in
 *          limbs this never touches, and the value loaded would be too small with no diagnostic.
 */
static void big_set_128(BigNumber *number, uint64_t high, uint64_t low)
{
    big_set_small(number, 0u);

    // Explicit casts narrow each half to the limb that holds it. The shift takes the upper 32 bits
    // before the cast discards them, so the two limbs together keep every bit of the half
    number->limb[0] = (uint32_t)low;
    number->limb[1] = (uint32_t)(low >> MMGR_ACCURACY_LIMB_BITS);
    number->limb[2] = (uint32_t)high;
    number->limb[3] = (uint32_t)(high >> MMGR_ACCURACY_LIMB_BITS);
}

/**
 * @brief Multiplies a BigNumber in place by a value that fits in one limb.
 *
 * @param[in,out] number     Value to scale, overwritten with the product [BORROWS].
 * @param[in]     multiplier Multiplier, at most one limb wide.
 * @note A carry out of the top limb is dropped. The widest value this suite builds reaches bit 722,
 *       so that carry is zero every time this runs.
 */
static void big_multiply_small(BigNumber *number, uint32_t multiplier)
{
    uint64_t carry = 0u;

    for (int index = 0; index < MMGR_ACCURACY_LIMBS; index++)
    {
        // Explicit casts widen both factors to uint64_t before multiplying. Two uint32_t operands
        // would multiply in 32 bits and drop the high half, which is the carry this loop needs
        const uint64_t product = ((uint64_t)number->limb[index] * (uint64_t)multiplier) + carry;

        // Explicit cast narrows to the low limb deliberately. The bits it drops are the ones the
        // next line keeps as the carry, so nothing is lost between the two statements
        number->limb[index] = (uint32_t)product;
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
static uint32_t big_get_bit(const BigNumber *number, int position)
{
    // The two halves are combined because a position is readable only when both hold, and neither
    // half carries a side effect. Dropping either one would let the line below index past an end
    if (position < 0 || position >= MMGR_ACCURACY_TOTAL_BITS)
    {
        return 0u;
    }
    return (number->limb[position / MMGR_ACCURACY_LIMB_BITS] >> (position % MMGR_ACCURACY_LIMB_BITS)) & 1u;
}

/**
 * @brief Sets one bit of a BigNumber, leaving every other bit as it was.
 *
 * @param[in,out] number   Value to modify [BORROWS].
 * @param[in]     position Bit index, 0 being the least significant bit of limb[0].
 * @warning Takes no range check, where big_get_bit does. A position at or above
 *          MMGR_ACCURACY_TOTAL_BITS writes past the limb array. Every caller here stays inside it:
 *          big_divide passes 0 through 1023, and the reciprocal cases pass length + 127, which
 *          reaches 722 for 5^256.
 */
static void big_set_bit(BigNumber *number, int position)
{
    // Explicit cast pins the shifted literal to uint32_t so the mask is the width of the limb it is
    // applied to, whatever width unsigned int happens to be on the target
    number->limb[position / MMGR_ACCURACY_LIMB_BITS] |= (uint32_t)1u << (position % MMGR_ACCURACY_LIMB_BITS);
}

/**
 * @brief Counts the bits a BigNumber needs, from its highest set bit down to bit 0.
 *
 * @param[in] number Value to measure [BORROWS].
 * @return           Number of significant bits, 1 for a value of one and 0 for a value of zero.
 * @note The count is a width rather than an index. big_top_128 subtracts 1 from it to reach the
 *       highest set bit, and the reciprocal cases add 127 to it to place a numerator.
 */
static int big_bit_length(const BigNumber *number)
{
    for (int index = MMGR_ACCURACY_LIMBS - 1; index >= 0; index--)
    {
        if (number->limb[index] != 0u)
        {
            uint32_t highest_limb = number->limb[index];
            int bit_count = 0;

            while (highest_limb != 0u)
            {
                bit_count++;
                highest_limb >>= 1;
            }
            return (index * MMGR_ACCURACY_LIMB_BITS) + bit_count;
        }
    }
    return 0;
}

/**
 * @brief Shifts a BigNumber left by one bit, carrying between limbs.
 *
 * @param[in,out] number Value to shift, overwritten with the result [BORROWS].
 * @note The top-bit shift is derived from MMGR_ACCURACY_LIMB_BITS rather than written as 31, so a
 *       change to the limb width cannot leave this reading the wrong bit.
 * @warning A carry out of the top limb is dropped. big_divide is the only caller and it shifts a
 *          remainder held below its denominator, so the widest value reaching here is 5^256 at 595
 *          bits and the shift never reaches the top limb.
 */
static void big_shift_left_one(BigNumber *number)
{
    uint32_t carry = 0u;

    for (int index = 0; index < MMGR_ACCURACY_LIMBS; index++)
    {
        const uint32_t carry_out = number->limb[index] >> (MMGR_ACCURACY_LIMB_BITS - 1);

        number->limb[index] = (number->limb[index] << 1) | carry;
        carry = carry_out;
    }
}

/**
 * @brief Orders two BigNumber values.
 *
 * @param[in] left  Left operand [BORROWS].
 * @param[in] right Right operand [BORROWS].
 * @return          1 where left is greater, -1 where right is greater, 0 where the two are equal.
 * @note Walks from the top limb down and stops at the first pair that differ.
 * @note big_divide calls this once per bit position to decide whether the remainder has reached the
 *       denominator, which is what makes the quotient truncate rather than round.
 */
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

/**
 * @brief Subtracts one BigNumber from another in place.
 *
 * @param[in,out] left  Minuend, overwritten with the difference [BORROWS].
 * @param[in]     right Subtrahend [BORROWS].
 * @note big_divide is the only caller, and it subtracts only after big_compare reports the minuend
 *       has reached the subtrahend, so the difference is never negative.
 * @warning A borrow out of the top limb is dropped. A caller passing the smaller value as the
 *          minuend would get the difference modulo 2^1024 with no diagnostic.
 */
static void big_subtract(BigNumber *left, const BigNumber *right)
{
    uint64_t borrow = 0u;

    for (int index = 0; index < MMGR_ACCURACY_LIMBS; index++)
    {
        // Explicit casts widen both limbs to uint64_t before subtracting. The true difference lies
        // in -2^32 through 2^32 - 1, so a negative one wraps above 2^63 and a non-negative one
        // cannot reach it, which is what makes bit 63 the borrow
        const uint64_t difference = (uint64_t)left->limb[index] - (uint64_t)right->limb[index] - borrow;

        // Explicit cast narrows to the low limb deliberately. It drops bits 32 through 63, which are
        // all set on a negative difference and all clear otherwise, so the single bit the next line
        // reads out of position 63 carries everything the cast discarded
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

    for (int step = 0; step < MMGR_ACCURACY_UP_ENTRIES; step++)
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

    for (int step = MMGR_ACCURACY_EXACT_STEPS; step < MMGR_ACCURACY_UP_ENTRIES; step++)
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

    for (int step = 0; step < MMGR_ACCURACY_DOWN_ENTRIES; step++)
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

/**
 * @brief Multiplies each mmgr_pow5_down significand back by its power of five and checks the product
 *        never rises above the scale that produced it.
 *
 * @note Reads the stored significand rather than the quotient computed here, so an entry one unit
 *       too large fails this case even where big_divide is correct.
 * @note Bounds the product from above only. A significand that is too small passes here, and
 *       test_every_negative_power_of_five_is_the_exact_reciprocal_truncated_toward_zero pins the
 *       exact value from the other side.
 */
void test_no_reciprocal_was_rounded_up(void)
{
    BigNumber value;
    BigNumber numerator;
    BigNumber product;

    for (int step = 0; step < MMGR_ACCURACY_DOWN_ENTRIES; step++)
    {
        big_power_of_five(&value, 1 << step);

        const int length = big_bit_length(&value);

        big_set_small(&numerator, 0u);
        big_set_bit(&numerator, length + 127);

        // The stored significand times its power of five must land at or below the scale it divides.
        // A truncated reciprocal is the floor of that division, so a product above the scale is an
        // entry that was rounded up
        big_set_128(&product, mmgr_pow5_down[step].hi, mmgr_pow5_down[step].lo);
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
