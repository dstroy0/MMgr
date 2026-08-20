// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "verbum_scrutor/verbum_scrutor.h"

void test_scrut_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("verbum_scrutor.h compiled with no header before it");
}

void test_scrut_namespace_is_wired(void)
{
    const VerbumScrutorNs *ns = &scrut;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(VerbumScrutorNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}
