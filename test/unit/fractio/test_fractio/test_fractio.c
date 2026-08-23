#include "unity.h"

#include "fractio/fractio.h"

void test_fractio_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("fractio.h compiled with no header before it");
}

void test_sign_of_positive_and_negative(void)
{
    TEST_ASSERT_EQUAL_UINT64(0u, mmgr_fract_sign(mmgr_fract_to_bits(1.0)));
    TEST_ASSERT_EQUAL_UINT64(1u, mmgr_fract_sign(mmgr_fract_to_bits(-1.0)));
    TEST_ASSERT_EQUAL_UINT64(0u, mmgr_fract_sign(mmgr_fract_to_bits(0.0)));
}

void test_negative_zero_keeps_its_sign(void)
{
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(1u, mmgr_fract_sign(mmgr_fract_to_bits(-0.0)),
                                     "negative zero is negative in the bits");
    TEST_ASSERT_TRUE(-0.0 == 0.0);
}

void test_exponent_of_one_is_the_bias(void)
{
    TEST_ASSERT_EQUAL_UINT64((uint64_t)MMGR_DBL_BIAS, mmgr_fract_exp(mmgr_fract_to_bits(1.0)));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)MMGR_DBL_BIAS + 1u, mmgr_fract_exp(mmgr_fract_to_bits(2.0)));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)MMGR_DBL_BIAS - 1u, mmgr_fract_exp(mmgr_fract_to_bits(0.5)));
}

void test_zero_has_no_exponent_and_no_mantissa(void)
{
    TEST_ASSERT_EQUAL_UINT64(0u, mmgr_fract_exp(mmgr_fract_to_bits(0.0)));
    TEST_ASSERT_EQUAL_UINT64(0u, mmgr_fract_mant(mmgr_fract_to_bits(0.0)));
}

void test_mantissa_drops_the_implicit_bit(void)
{
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, mmgr_fract_mant(mmgr_fract_to_bits(1.0)),
                                     "1.0 is 1.000... so the stored mantissa is zero");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, mmgr_fract_mant(mmgr_fract_to_bits(2.0)), "so is 2.0");
    TEST_ASSERT_EQUAL_UINT64(1ull << (MMGR_DBL_MANT_BITS - 1u), mmgr_fract_mant(mmgr_fract_to_bits(1.5)));
}

void test_infinities(void)
{
    const double inf = mmgr_fract_from_bits(mmgr_fract_merge((mmgr_u64)0u, MMGR_DBL_EXP_ALL, (mmgr_u64)0u));
    const double ninf = mmgr_fract_from_bits(mmgr_fract_merge((mmgr_u64)1u, MMGR_DBL_EXP_ALL, (mmgr_u64)0u));

    TEST_ASSERT_EQUAL_UINT64(MMGR_DBL_EXP_ALL, mmgr_fract_exp(mmgr_fract_to_bits(inf)));
    TEST_ASSERT_EQUAL_UINT64(0u, mmgr_fract_mant(mmgr_fract_to_bits(inf)));
    TEST_ASSERT_EQUAL_UINT64(0u, mmgr_fract_sign(mmgr_fract_to_bits(inf)));
    TEST_ASSERT_EQUAL_UINT64(1u, mmgr_fract_sign(mmgr_fract_to_bits(ninf)));
    TEST_ASSERT_TRUE(inf > 1e308);
    TEST_ASSERT_TRUE(ninf < -1e308);
}

void test_a_nan_has_a_full_exponent_and_a_mantissa(void)
{
    const double nan = mmgr_fract_from_bits(mmgr_fract_merge((mmgr_u64)0u, MMGR_DBL_EXP_ALL, (mmgr_u64)1u));
    TEST_ASSERT_EQUAL_UINT64(MMGR_DBL_EXP_ALL, mmgr_fract_exp(mmgr_fract_to_bits(nan)));
    TEST_ASSERT_NOT_EQUAL(0u, mmgr_fract_mant(mmgr_fract_to_bits(nan)));
    TEST_ASSERT_FALSE_MESSAGE(nan == nan, "a NaN is not equal to itself");
}

void test_merge_and_from_bits_reverse_the_accessors(void)
{
    static const double vals[] = {0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 3.14159265358979, 1e-300, 1e300, -2.5};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        const mmgr_u64 was = mmgr_fract_to_bits(vals[i]);
        const mmgr_u64 bits = mmgr_fract_merge(mmgr_fract_sign(was), mmgr_fract_exp(was), mmgr_fract_mant(was));
        const mmgr_u64 back = mmgr_fract_to_bits(mmgr_fract_from_bits(bits));

                TEST_ASSERT_EQUAL_UINT64(mmgr_fract_sign(was), mmgr_fract_sign(back));
        TEST_ASSERT_EQUAL_UINT64(mmgr_fract_exp(was), mmgr_fract_exp(back));
        TEST_ASSERT_EQUAL_UINT64(mmgr_fract_mant(was), mmgr_fract_mant(back));
    }
}

void test_a_subnormal_keeps_its_mantissa(void)
{
    const double tiny = mmgr_fract_from_bits(mmgr_fract_merge((mmgr_u64)0u, (mmgr_u64)0u, (mmgr_u64)1u));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, mmgr_fract_exp(mmgr_fract_to_bits(tiny)),
                                     "a subnormal has a zero exponent field");
    TEST_ASSERT_EQUAL_UINT64(1u, mmgr_fract_mant(mmgr_fract_to_bits(tiny)));
    TEST_ASSERT_TRUE_MESSAGE(tiny > 0.0, "and is still greater than zero");
}

void test_merge_masks_each_field(void)
{
        const mmgr_u64 bits = mmgr_fract_merge((mmgr_u64)1u, MMGR_DBL_EXP_ALL, MMGR_DBL_MANT_MASK);
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
        const double inf = 1e308 * 10.0;
    static const double vals[6] = {0.0, -0.0, 1.0, -2.5, 4.9406564584124654e-324, 1.7976931348623157e308};

    for (unsigned i = 0; i < 6u; i++)
    {
        const mmgr_u64 bits = mmgr_fract_to_bits(vals[i]);
        TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(vals[i], mmgr_fract_from_bits(bits),
                                         "the pattern did not come back as the value");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(bits, mmgr_fract_to_bits(mmgr_fract_from_bits(bits)), "and back again");
    }

        const mmgr_u64 b = mmgr_fract_to_bits(-2.5);
    TEST_ASSERT_EQUAL_HEX64(b, mmgr_fract_merge(mmgr_fract_sign(b), mmgr_fract_exp(b), mmgr_fract_mant(b)));

        TEST_ASSERT_EQUAL_HEX64(MMGR_DBL_EXP_MASK, mmgr_fract_to_bits(inf));
    TEST_ASSERT_TRUE_MESSAGE((mmgr_fract_to_bits(inf - inf) & MMGR_DBL_EXP_MASK) == MMGR_DBL_EXP_MASK,
                             "a nan has the all ones exponent like an infinity does");
}
