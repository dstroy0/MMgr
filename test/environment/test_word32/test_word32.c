/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "config/mmgr_config.h"

void test_word32_widths_are_what_was_asked_for(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(32, MMGR_WORD_BITS, "MMGR_WORD_BITS did not reach the translation unit");

    TEST_ASSERT_TRUE_MESSAGE(MMGR_WORD_BITS == 16 || MMGR_WORD_BITS == 32 || MMGR_WORD_BITS == 64,
                             "MMGR_WORD_BITS is not a supported width");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_INDEX_BITS == 16 || MMGR_INDEX_BITS == 32,
                             "MMGR_INDEX_BITS is not a supported width");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_SWAR_BITS <= MMGR_WORD_BITS,
                             "a scan lane wider than the register reads the wrong bytes");
}

void test_word32_types_match_the_widths(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_WORD_BITS / 8u, sizeof(mmgr_word));
    TEST_ASSERT_EQUAL_size_t(MMGR_INDEX_BITS / 8u, sizeof(mmgr_idx));
    TEST_ASSERT_EQUAL_size_t(1u, sizeof(MmgrEnumProbe));
}
