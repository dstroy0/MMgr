// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "config/mmgr_config.h"

void test_types_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("mmgr_config.h brings mmgr_types.h in with the widths already set");
}

void test_fixed_width_types_are_their_widths(void)
{
    TEST_ASSERT_EQUAL_size_t(1u, sizeof(mmgr_u8));
    TEST_ASSERT_EQUAL_size_t(2u, sizeof(mmgr_u16));
    TEST_ASSERT_EQUAL_size_t(4u, sizeof(mmgr_u32));
    TEST_ASSERT_EQUAL_size_t(8u, sizeof(mmgr_u64));
    TEST_ASSERT_EQUAL_size_t(1u, sizeof(mmgr_i8));
    TEST_ASSERT_EQUAL_size_t(2u, sizeof(mmgr_i16));
    TEST_ASSERT_EQUAL_size_t(4u, sizeof(mmgr_i32));
    TEST_ASSERT_EQUAL_size_t(8u, sizeof(mmgr_i64));
}

void test_word_matches_the_configured_width(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_WORD_BITS / 8u, sizeof(mmgr_word));
}

void test_index_fits_the_register_that_carries_it(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_INDEX_BITS / 8u, sizeof(mmgr_idx));
    TEST_ASSERT_LESS_OR_EQUAL_size_t_MESSAGE(sizeof(mmgr_word), sizeof(mmgr_idx),
                                             "an index wider than the word is spilled arithmetic on every use");
}

void test_packed_enum_keeps_its_declared_width(void)
{
    // losing this moves every field after an enum in every struct, and borrows are addressed by
    // offset
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, sizeof(MmgrEnumProbe), "MMGR_ENUM_PACKED was not honored");
}

void test_bool_constants(void)
{
    TEST_ASSERT_TRUE(MMGR_TRUE);
    TEST_ASSERT_FALSE(MMGR_FALSE);
    TEST_ASSERT_NOT_EQUAL(MMGR_TRUE, MMGR_FALSE);
}
