// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "bitio/bitio.h"

void test_bitio_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("bitio.h compiled with no header before it");
}

void test_bitio_namespace_is_wired(void)
{
    const BitioNs *ns = &bitio;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(BitioNs), sizeof(*ns), "the namespace instance is not its own type");
}
