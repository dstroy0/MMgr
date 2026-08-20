// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "occultum_custodiae/occultum_custodiae.h"

void test_occult_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("occultum_custodiae.h compiled with no header before it");
}

void test_occult_namespace_is_wired(void)
{
    const OccultumCustodiaeNs *ns = &occult;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(OccultumCustodiaeNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}
