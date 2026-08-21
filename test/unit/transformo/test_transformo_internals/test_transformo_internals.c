// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The parts of the engine that cannot be reached from outside it.
//
// Most of the engine is tested through the two entries it exports, which is the right way round:
// those are the contract and the insides are free to change. Three things in it are not reachable
// that way and are worth pinning anyway.
//
// A carry out of the middle column of the 128 by 128 multiply happens about once in 2^63 multiplies.
// Forty thousand random and strobed values did not produce one and never will; the operands below
// were solved for rather than found. A carry that is written and never executed is a carry nobody
// knows works.
//
// A tie that is exactly a tie - the round bit set and nothing at all below it - is what the round
// to even rule exists for, and reaching one from a decimal string means finding a decimal that
// lands exactly halfway between two doubles. Handing the rounding a fraction directly is the same
// test without the search.
//
// The normalise has a path for a fraction whose high word is empty, which the conversion never
// produces because it guards the mantissa first. It is one shift and it is correct; this says so.
//
// The translation unit is compiled in rather than linked, which is what makes the file-local
// entries visible. Its namespace is renamed on the way in so it does not collide with the copy in
// the library this suite also links.
#include "transformo/transformo.c"

#include "unity.h"

#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

/* muto_round reads the sign off the context like everything else does. These cases were written
   when it was a parameter, and what they are pinning is the rounding, not where the sign lives. */
static double muto_round_probe(MutoCtx *c, mmgr_bool neg)
{
    c->neg = neg;
    return muto_round(c);
}

/* ---------------------------------------------------------------------------------------------
 * the multiply
 * ------------------------------------------------------------------------------------------- */

void test_the_middle_column_carries_into_the_top(void)
{
    // Solved for: the middle column lands two short of wrapping with two waiting below it, so the
    // add of the low carry takes it over and the top word has to take one.
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

    // The full 256 bit product of these two is 0x6000000000000002 in its top word and nothing in
    // the one below, which is the carry arriving: without it the top word would read ...0000. The
    // normalise then brings the top bit up, one place, so what comes out is that doubled.
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xC000000000000004ULL, f.hi, "the top word did not take the carry");
    TEST_ASSERT_EQUAL_HEX64(0u, f.lo);
    TEST_ASSERT_EQUAL_INT_MESSAGE(127, f.fe2, "the exponent should carry the 128 and the shift back");
    TEST_ASSERT_TRUE_MESSAGE(f.rest != 0, "the dropped half was not empty and should have been remembered");
}

void test_the_multiply_agrees_with_halves_done_by_hand(void)
{
    // A handful of ordinary pairs, checked against the product assembled from 32 bit pieces in the
    // test rather than by the same routine under test.
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

            // The same product, one 32 bit column at a time.
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

/* ---------------------------------------------------------------------------------------------
 * the normalise
 * ------------------------------------------------------------------------------------------- */

void test_normalising_a_fraction_whose_high_word_is_empty(void)
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

void test_normalising_nothing_leaves_it_alone(void)
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

void test_the_leading_zero_count_at_every_position(void)
{
    for (unsigned bit = 0; bit < 64u; bit++)
    {
        const mmgr_u64 x = (mmgr_u64)1 << bit;
        TEST_ASSERT_EQUAL_INT_MESSAGE(63 - (int)bit, mmgr_muto_clz(x), "wrong count for a single set bit");
        // And with noise below it, which must not change the answer.
        TEST_ASSERT_EQUAL_INT(63 - (int)bit, mmgr_muto_clz(x | (x - 1u)));
    }
}

/* ---------------------------------------------------------------------------------------------
 * the rounding
 * ------------------------------------------------------------------------------------------- */

/** @brief A fraction that will round to a mantissa with the given low bit, at a chosen tie. */
static double round_of(mmgr_u64 mant53, unsigned half, unsigned rest, int e2)
{
    MutoCtx f;

    // A 128 bit fraction whose high word is mant53 shifted up eleven stands for mant53 times two
    // to the eleven plus sixty four plus e2, so e2 of minus seventy five makes the value mant53.
    f.hi = (mant53 << 11) | ((mmgr_u64)half << 10);
    f.lo = 0u;
    f.fe2 = e2;
    f.rest = (int)rest;
    return muto_round_probe(&f, MMGR_FALSE);
}

void test_an_exact_tie_goes_to_even(void)
{
    // The round bit set, nothing at all below it, so the only thing left to decide by is whether
    // the mantissa is already even. This is the case a decimal string can reach but only by
    // landing exactly halfway between two doubles.
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
    TEST_ASSERT_TRUE_MESSAGE(mmgr_fract_sign(muto_round_probe(&f, MMGR_TRUE)) != 0u, "and it keeps a sign it was given");
}
