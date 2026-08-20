// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "fractio/fractio.h"

void test_fract_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("fractio.h compiled with no header before it");
}

void test_fract_namespace_is_wired(void)
{
    const FractioNs *ns = &fract;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(FractioNs), sizeof(*ns), "the namespace instance is not its own type");
}
