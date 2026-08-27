/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
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
        TEST_ASSERT_EQUAL_INT_MESSAGE(63 - (int)bit, MMGR_CALL(clz.lead, ClzCfg, .val = x), "wrong count for a single set bit");
                const mmgr_u64 noisy = x | (x - 1u);
        TEST_ASSERT_EQUAL_INT(63 - (int)bit, MMGR_CALL(clz.lead, ClzCfg, .val = noisy));
    }
}
