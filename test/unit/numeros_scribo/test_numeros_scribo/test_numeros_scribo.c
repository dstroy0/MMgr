// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "numeros_scribo/numeros_scribo.h"

void test_numer_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("numeros_scribo.h compiled with no header before it");
}

void test_numer_namespace_is_wired(void)
{
    const NumerosScriboNs *ns = &numer;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(NumerosScriboNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}
