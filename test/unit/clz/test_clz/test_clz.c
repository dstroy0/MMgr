// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The leading zero count, at every position it can answer about.
//
// Sixty four cases in a loop rather than a handful of chosen values: the body is six steps that each
// halve what is left unknown, so the values that would break it are the ones sitting on a step's
// boundary, and choosing examples by hand is how those get missed.
#include "clz/clz.h"

#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_the_leading_zero_count_at_every_position(void)
{
    for (unsigned bit = 0; bit < 64u; bit++)
    {
        const mmgr_u64 x = (mmgr_u64)1 << bit;
        TEST_ASSERT_EQUAL_INT_MESSAGE(63 - (int)bit, mmgr_clz_lead(x), "wrong count for a single set bit");
        // And with noise below it, which must not change the answer.
        const mmgr_u64 noisy = x | (x - 1u);
        TEST_ASSERT_EQUAL_INT(63 - (int)bit, mmgr_clz_lead(noisy));
    }
}
