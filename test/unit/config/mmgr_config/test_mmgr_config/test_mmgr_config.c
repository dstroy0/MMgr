/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "config/mmgr_config.h"

void test_config_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("mmgr_config.h compiled with no header before it");
}

void test_word_width_is_one_of_three(void)
{
    TEST_ASSERT_TRUE_MESSAGE(MMGR_WORD_BITS == 16 || MMGR_WORD_BITS == 32 || MMGR_WORD_BITS == 64,
                             "MMGR_WORD_BITS must be 16, 32 or 64");
}

void test_scan_width_is_the_word_width(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(MMGR_WORD_BITS, MMGR_SWAR_BITS,
                                  "the scan width is not a separate knob - it follows the machine");
}

void test_index_never_exceeds_the_word(void)
{
    TEST_ASSERT_LESS_OR_EQUAL_INT(MMGR_WORD_BITS, MMGR_INDEX_BITS);
}

void test_buffer_bound_is_a_real_size(void)
{
    TEST_ASSERT_GREATER_THAN_size_t(0u, MMGR_CARCER_MAX);
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CARCER_MAX >= MMGR_PLAINTEXT_CONFIN_SIZE ||
                                 MMGR_CARCER_MAX >= MMGR_SECURE_CONFIN_SIZE,
                             "the bound must cover the largest buffer");
}
