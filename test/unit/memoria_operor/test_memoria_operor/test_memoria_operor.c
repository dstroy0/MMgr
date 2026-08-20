// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "memoria_operor/memoria_operor.h"

void test_memor_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("memoria_operor.h compiled with no header before it");
}

void test_memor_namespace_is_wired(void)
{
    const MemoriaOperorNs *ns = &memor;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(MemoriaOperorNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}
