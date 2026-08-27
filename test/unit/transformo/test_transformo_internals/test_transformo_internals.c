/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "transformo/transformo.c"

#include "unity.h"

#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

static double muto_round_probe(MutoCtx *c, mmgr_bool neg)
{
    c->neg = neg;
    return muto_round(c);
}


void test_the_middle_column_carries_into_the_top(void)
{
            MutoCtx f;
    MmgrPow5 g;

    f.hi = 0xC000000000000000ULL;
    f.lo = 0xFFFFFFFFFFFFFFFEULL;
    f.fe2 = 0;
    f.rest = 0;

    g.hi = 0x8000000000000001ULL;
    g.lo = 0xFFFFFFFFFFFFFFFFULL;
    g.e2 = 0;

    f.pow = &g;
    muto_mul_pow5(&f);

                TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xC000000000000004ULL, f.hi, "the top word did not take the carry");
    TEST_ASSERT_EQUAL_HEX64(0u, f.lo);
    TEST_ASSERT_EQUAL_INT_MESSAGE(127, f.fe2, "the exponent should carry the 128 and the shift back");
    TEST_ASSERT_TRUE_MESSAGE(f.rest != 0, "the dropped half was not empty and should have been remembered");
}

void test_the_multiply_agrees_with_halves_done_by_hand(void)
{
            static const mmgr_u64 vals[] = {0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, 0x9E3779B97F4A7C15ULL,
                                    0xA000000000000000ULL, 0xCCCCCCCCCCCCCCCDULL};
    MutoCtx f;

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        for (unsigned j = 0; j < sizeof vals / sizeof vals[0]; j++)
        {
            mmgr_u64 hi = 0;
            mmgr_u64 lo = 0;
            f.a = vals[i];
            f.b = vals[j];
            muto_mul(&f);
            hi = f.phi;
            lo = f.plo;

                        const mmgr_u64 m = 0xFFFFFFFFULL;
            const mmgr_u64 a0 = vals[i] & m;
            const mmgr_u64 a1 = vals[i] >> 32;
            const mmgr_u64 b0 = vals[j] & m;
            const mmgr_u64 b1 = vals[j] >> 32;
            const mmgr_u64 t0 = a0 * b0;
            const mmgr_u64 t1 = a1 * b0 + (t0 >> 32);
            const mmgr_u64 t2 = a0 * b1 + (t1 & m);
            const mmgr_u64 want_lo = (t2 << 32) | (t0 & m);
            const mmgr_u64 want_hi = a1 * b1 + (t1 >> 32) + (t2 >> 32);

            TEST_ASSERT_EQUAL_HEX64(want_hi, hi);
            TEST_ASSERT_EQUAL_HEX64(want_lo, lo);
        }
    }
}


void test_normalizing_a_fraction_whose_high_word_is_empty(void)
{
    MutoCtx f;

    f.hi = 0u;
    f.lo = 0x0000000000000001ULL;
    f.fe2 = 0;
    f.rest = 0;

    muto_norm(&f);

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x8000000000000000ULL, f.hi, "the low word should have come up and been shifted");
    TEST_ASSERT_EQUAL_HEX64(0u, f.lo);
    TEST_ASSERT_EQUAL_INT_MESSAGE(-64 - 63, f.fe2, "the exponent should carry both the move and the shift");
}

void test_normalizing_nothing_leaves_it_alone(void)
{
    MutoCtx f;

    f.hi = 0u;
    f.lo = 0u;
    f.fe2 = 7;
    f.rest = 0;

    muto_norm(&f);

    TEST_ASSERT_EQUAL_HEX64(0u, f.hi);
    TEST_ASSERT_EQUAL_HEX64(0u, f.lo);
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, f.fe2, "there was nothing to shift, so nothing should have moved");
}


static double round_of(mmgr_u64 mant53, unsigned half, unsigned rest, int e2)
{
    MutoCtx f;

            f.hi = (mant53 << 11) | ((mmgr_u64)half << 10);
    f.lo = 0u;
    f.fe2 = e2;
    f.rest = (int)rest;
    return muto_round_probe(&f, MMGR_FALSE);
}

void test_an_exact_tie_goes_to_even(void)
{
                const mmgr_u64 even = ((mmgr_u64)1 << 52) | 0u;
    const mmgr_u64 odd = ((mmgr_u64)1 << 52) | 1u;

    const double stays = round_of(even, 1u, 0u, -75);
    const double climbs = round_of(odd, 1u, 0u, -75);

    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE((double)even, stays, "an even mantissa at an exact tie stays put");
    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE((double)(odd + 1u), climbs, "an odd one at an exact tie goes up to even");
}

void test_a_tie_with_anything_under_it_goes_up(void)
{
    const mmgr_u64 even = ((mmgr_u64)1 << 52) | 0u;
    const double v = round_of(even, 1u, 1u, -75);

    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE((double)(even + 1u), v, "not a tie once something is set below it");
}

void test_below_the_tie_goes_down(void)
{
    const mmgr_u64 odd = ((mmgr_u64)1 << 52) | 1u;
    const double v = round_of(odd, 0u, 1u, -75);

    TEST_ASSERT_EQUAL_DOUBLE_MESSAGE((double)odd, v, "the round bit clear means down whatever is under it");
}

void test_rounding_a_fraction_of_nothing(void)
{
    MutoCtx f;

    f.hi = 0u;
    f.lo = 0u;
    f.fe2 = 0;
    f.rest = 0;

    TEST_ASSERT_EQUAL_DOUBLE(0.0, muto_round_probe(&f, MMGR_FALSE));
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(fract.sign, FractioCfg, .bits = MMGR_CALL(fract.to_bits, FractioCfg, .val = muto_round_probe(&f, MMGR_TRUE))) != 0u,
                             "and it keeps a sign it was given");
}


static mmgr_u64 to_u64_of(mmgr_u64 hi, mmgr_u64 lo, mmgr_iword fe2, mmgr_iword rest, mmgr_word above)
{
    MutoCtx f;

    memset(&f, 0, sizeof f);
    f.hi = hi;
    f.lo = lo;
    f.fe2 = fe2;
    f.rest = rest;
    f.above = above;
    return muto_to_u64(&f);
}

void test_to_u64_of_an_empty_fraction_is_zero(void)
{
            TEST_ASSERT_EQUAL_UINT64(0u, to_u64_of(0u, 0u, -100, 0, 0u));
}

void test_to_u64_of_a_number_wider_than_the_word_saturates(void)
{
            TEST_ASSERT_EQUAL_UINT64(~(mmgr_u64)0, to_u64_of(1u, 0u, -32, 0, 0u));
}

void test_to_u64_with_the_point_on_the_word_boundary(void)
{
            TEST_ASSERT_EQUAL_UINT64(7u, to_u64_of(7u, 0u, -64, 0, 0u));
}

void test_to_u64_on_the_boundary_rounds_a_tie_to_even(void)
{
            TEST_ASSERT_EQUAL_UINT64_MESSAGE(4u, to_u64_of(4u, (mmgr_u64)1 << 63, 0 - 64, 0, 0u), "even stays");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(6u, to_u64_of(5u, (mmgr_u64)1 << 63, 0 - 64, 0, 0u), "odd goes up");
}

void test_to_u64_on_the_boundary_sees_what_is_under_the_round_bit(void)
{
        TEST_ASSERT_EQUAL_UINT64(5u, to_u64_of(4u, ((mmgr_u64)1 << 63) | ((mmgr_u64)1 << 62), -64, 0, 0u));
}

void test_to_u64_on_the_boundary_takes_the_parity_of_the_whole_number(void)
{
            TEST_ASSERT_EQUAL_UINT64(5u, to_u64_of(4u, (mmgr_u64)1 << 63, -64, 0, 1u));
}
