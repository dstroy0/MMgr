// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
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

void test_tenant_bound_is_a_real_size(void)
{
    TEST_ASSERT_GREATER_THAN_size_t(0u, MMGR_CONFIN_MAX);
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CONFIN_MAX >= MMGR_PLAINTEXT_CONFIN_SIZE ||
                                 MMGR_CONFIN_MAX >= MMGR_SECURE_CONFIN_SIZE,
                             "the bound must cover the largest tenant");
}

void test_ghost_slot_sits_past_the_workers(void)
{
    TEST_ASSERT_EQUAL_INT(MMGR_WORKER_COUNT, MMGR_GHOST_WORKER_SLOT);
}

void test_context_id_is_needed_exactly_when_it_is_used(void)
{
#if (MMGR_WORKER_COUNT != 1) || MMGR_DEBUG_CHECKS
    TEST_ASSERT_EQUAL_INT(1, MMGR_NEEDS_CONTEXT_ID);
#else
    TEST_ASSERT_EQUAL_INT(0, MMGR_NEEDS_CONTEXT_ID);
#endif
}
