// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "proximus_operor/proximus_operor.h"

void test_proxim_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("proximus_operor.h compiled with no header before it");
}

void test_proxim_namespace_is_wired(void)
{
    const ProximusOperorNs *ns = &proxim;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(ProximusOperorNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}
