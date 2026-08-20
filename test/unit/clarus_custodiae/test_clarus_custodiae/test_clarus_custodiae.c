// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "clarus_custodiae/clarus_custodiae.h"

void test_clarus_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("clarus_custodiae.h compiled with no header before it");
}

void test_clarus_namespace_is_wired(void)
{
    const ClarusCustodiaeNs *ns = &clarus;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(ClarusCustodiaeNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}
