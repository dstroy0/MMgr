// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "byteio/byteio.h"

void test_byteio_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("byteio.h compiled with no header before it");
}

void test_byteio_namespace_is_wired(void)
{
    const ByteioNs *ns = &byteio;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(ByteioNs), sizeof(*ns), "the namespace instance is not its own type");
}
