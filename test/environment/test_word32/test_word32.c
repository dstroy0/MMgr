// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Environment suite: word32.
//
// a 32-bit word on whatever host this is
//
// A build says what widths it selected; this asserts the code actually GOT them. Those are
// different claims: -DMMGR_SWAR_BITS=16 that never reaches a translation unit leaves a
// suite passing against the host's 64-bit lane while the report says the 16-bit one ran.

#include "unity.h"

#include "mmgr_config.h"

void test_word32_widths_are_what_was_asked_for(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(32, MMGR_WORD_BITS, "MMGR_WORD_BITS did not reach the translation unit");

    // True in every environment, override or not.
    TEST_ASSERT_TRUE_MESSAGE(MMGR_WORD_BITS == 16 || MMGR_WORD_BITS == 32 || MMGR_WORD_BITS == 64,
                             "MMGR_WORD_BITS is not a supported width");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_INDEX_BITS == 16 || MMGR_INDEX_BITS == 32,
                             "MMGR_INDEX_BITS is not a supported width");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_SWAR_BITS <= MMGR_WORD_BITS,
                             "a scan lane wider than the register reads the wrong bytes");
}

// The types have to match the numbers. A width macro the typedefs did not follow is worse
// than a wrong macro: the asserts above pass and every offset computed from the type is out.
void test_word32_types_match_the_widths(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_WORD_BITS / 8u, sizeof(mmgr_word));
    TEST_ASSERT_EQUAL_size_t(MMGR_INDEX_BITS / 8u, sizeof(mmgr_idx));
    TEST_ASSERT_EQUAL_size_t(1u, sizeof(MmgrEnumProbe));
}
