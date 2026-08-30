/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "impensa_ancorae_acus/impensa_ancorae_acus.h"

void test_anchor_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("impensa_ancorae_acus.h compiled with no header before it");
}

void test_anchor_table_covers_every_byte(void)
{
    const uint8_t first = MMGR_CALL(ancorae.impensa, AncoraeCfg, .byte = (uint8_t)0u);
    int varies = 0;

    for (unsigned c = 0; c < 256u; c++)
    {
        if (MMGR_CALL(ancorae.impensa, AncoraeCfg, .byte = (uint8_t)c) != first)
        {
            varies = 1;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(varies, "a profile that costs every byte the same ranks nothing");
}

void test_anchor_never_picks_the_terminator(void)
{
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(255u, MMGR_CALL(ancorae.impensa, AncoraeCfg, .byte = (uint8_t)0u),
                                    "NUL ends a scan, so it must never be the cheapest anchor");
}

void test_impensa_ancorae_acus_is_never_zero(void)
{
    for (unsigned c = 0; c < 256u; c++)
    {
        TEST_ASSERT_GREATER_THAN_UINT8_MESSAGE(0u, MMGR_CALL(ancorae.impensa, AncoraeCfg, .byte = (uint8_t)c),
                                               "zero would tie with a byte that cannot occur");
    }
}

void test_anchor_prefers_rare_bytes_to_common_ones(void)
{
    TEST_ASSERT_LESS_THAN_UINT8_MESSAGE(MMGR_CALL(ancorae.impensa, AncoraeCfg, .byte = (unsigned char)' '),
                                        MMGR_CALL(ancorae.impensa, AncoraeCfg, .byte = (unsigned char)'q'),
                                        "space is the most common byte in text and must cost more than q");
    TEST_ASSERT_LESS_THAN_UINT8_MESSAGE(MMGR_CALL(ancorae.impensa, AncoraeCfg, .byte = (unsigned char)'e'),
                                        MMGR_CALL(ancorae.impensa, AncoraeCfg, .byte = (unsigned char)'z'),
                                        "z is rarer than e");
}
