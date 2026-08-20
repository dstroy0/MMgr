// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "spatium/spatium.h"

void test_spat_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("spatium.h compiled with no header before it");
}

void test_spat_namespace_is_wired(void)
{
    const SpatiumNs *ns = &spat;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(SpatiumNs), sizeof(*ns), "the namespace instance is not its own type");
}
