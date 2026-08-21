// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "fractio/fractio.h"

void test_fractio_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("fractio.h compiled with no header before it");
}

void test_sign_of_positive_and_negative(void)
{
    TEST_ASSERT_EQUAL_UINT64(0u, fract.sign(1.0));
    TEST_ASSERT_EQUAL_UINT64(1u, fract.sign(-1.0));
    TEST_ASSERT_EQUAL_UINT64(0u, fract.sign(0.0));
}

void test_negative_zero_keeps_its_sign(void)
{
    // the whole reason to read the bits rather than compare: -0.0 == 0.0 is true
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1u, fract.sign(-0.0), "negative zero is negative in the bits");
    TEST_ASSERT_TRUE(-0.0 == 0.0);
}

void test_exponent_of_one_is_the_bias(void)
{
    TEST_ASSERT_EQUAL_UINT64((uint64_t)MMGR_DBL_BIAS, fract.exp(1.0));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)MMGR_DBL_BIAS + 1u, fract.exp(2.0));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)MMGR_DBL_BIAS - 1u, fract.exp(0.5));
}

void test_zero_has_no_exponent_and_no_mantissa(void)
{
    TEST_ASSERT_EQUAL_UINT64(0u, fract.exp(0.0));
    TEST_ASSERT_EQUAL_UINT64(0u, fract.mant(0.0));
}

void test_mantissa_drops_the_implicit_bit(void)
{
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, fract.mant(1.0), "1.0 is 1.000... so the stored mantissa is zero");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, fract.mant(2.0), "so is 2.0");
    TEST_ASSERT_EQUAL_UINT64(1ull << (MMGR_DBL_MANT_BITS - 1u), fract.mant(1.5));
}

void test_infinities(void)
{
    const double inf = fract.from_bits(fract.merge(0u, MMGR_DBL_EXP_ALL, 0u));
    const double ninf = fract.from_bits(fract.merge(1u, MMGR_DBL_EXP_ALL, 0u));

    TEST_ASSERT_EQUAL_UINT64(MMGR_DBL_EXP_ALL, fract.exp(inf));
    TEST_ASSERT_EQUAL_UINT64(0u, fract.mant(inf));
    TEST_ASSERT_EQUAL_UINT64(0u, fract.sign(inf));
    TEST_ASSERT_EQUAL_UINT64(1u, fract.sign(ninf));
    TEST_ASSERT_TRUE(inf > 1e308);
    TEST_ASSERT_TRUE(ninf < -1e308);
}

void test_a_nan_has_a_full_exponent_and_a_mantissa(void)
{
    const double nan = fract.from_bits(fract.merge(0u, MMGR_DBL_EXP_ALL, 1u));
    TEST_ASSERT_EQUAL_UINT64(MMGR_DBL_EXP_ALL, fract.exp(nan));
    TEST_ASSERT_NOT_EQUAL(0u, fract.mant(nan));
    TEST_ASSERT_FALSE_MESSAGE(nan == nan, "a NaN is not equal to itself");
}

void test_merge_and_from_bits_reverse_the_accessors(void)
{
    static const double vals[] = {0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 3.14159265358979, 1e-300, 1e300, -2.5};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        const mmgr_u64 bits = fract.merge(fract.sign(vals[i]), fract.exp(vals[i]), fract.mant(vals[i]));
        const double back = fract.from_bits(bits);

        // compare the bits, not the values, so negative zero and NaN are handled too
        TEST_ASSERT_EQUAL_UINT64(fract.sign(vals[i]), fract.sign(back));
        TEST_ASSERT_EQUAL_UINT64(fract.exp(vals[i]), fract.exp(back));
        TEST_ASSERT_EQUAL_UINT64(fract.mant(vals[i]), fract.mant(back));
    }
}

void test_a_subnormal_keeps_its_mantissa(void)
{
    const double tiny = fract.from_bits(fract.merge(0u, 0u, 1u));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, fract.exp(tiny), "a subnormal has a zero exponent field");
    TEST_ASSERT_EQUAL_UINT64(1u, fract.mant(tiny));
    TEST_ASSERT_TRUE_MESSAGE(tiny > 0.0, "and is still greater than zero");
}

void test_merge_masks_each_field(void)
{
    // a field wider than its slot must not spill into the next
    const mmgr_u64 bits = fract.merge(1u, MMGR_DBL_EXP_ALL, MMGR_DBL_MANT_MASK);
    TEST_ASSERT_EQUAL_UINT64(1u, (bits & MMGR_DBL_SIGN_MASK) >> MMGR_DBL_SIGN_SHIFT);
    TEST_ASSERT_EQUAL_UINT64(MMGR_DBL_EXP_ALL, (bits & MMGR_DBL_EXP_MASK) >> MMGR_DBL_MANT_BITS);
    TEST_ASSERT_EQUAL_UINT64(MMGR_DBL_MANT_MASK, bits & MMGR_DBL_MANT_MASK);
}

void test_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_fract_sign, fract.sign);
    TEST_ASSERT_EQUAL_PTR(mmgr_fract_from_bits, fract.from_bits);
}

void test_to_bits_is_from_bits_the_other_way(void)
{
    // Every class, because the pattern is the point and a nan or a subnormal has one too.
    const double inf = 1e308 * 10.0;
    static const double vals[6] = {0.0, -0.0, 1.0, -2.5, 4.9406564584124654e-324, 1.7976931348623157e308};

    for (unsigned i = 0; i < 6u; i++)
    {
        const mmgr_u64 bits = fract.to_bits(vals[i]);
        TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(vals[i], fract.from_bits(bits), "the pattern did not come back as the value");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(bits, fract.to_bits(fract.from_bits(bits)), "and back again");
    }

    // The fields agree with the pattern they were taken from.
    const mmgr_u64 b = fract.to_bits(-2.5);
    TEST_ASSERT_EQUAL_HEX64(b, fract.merge(fract.sign(-2.5), fract.exp(-2.5), fract.mant(-2.5)));

    // An infinity and a nan have patterns too, and to_bits does not look at what it is holding.
    TEST_ASSERT_EQUAL_HEX64(MMGR_DBL_EXP_MASK, fract.to_bits(inf));
    TEST_ASSERT_TRUE_MESSAGE((fract.to_bits(inf - inf) & MMGR_DBL_EXP_MASK) == MMGR_DBL_EXP_MASK,
                             "a nan has the all ones exponent like an infinity does");
}
