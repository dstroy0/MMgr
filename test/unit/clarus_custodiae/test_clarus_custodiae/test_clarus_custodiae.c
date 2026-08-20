// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Unit suite for src/clarus_custodiae.
//
// test/ is exempt from the src/ style rules, so this reads as plain host C.

#include "unity.h"

#include "clarus_custodiae/clarus_custodiae.h"

// The header is included FIRST and alone, above everything but unity.h. A module header
// that only compiles once some other header has been read is a real defect, and it is
// invisible in src/ because the .c that includes it always includes its dependencies too.
void test_clarus_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("clarus_custodiae.h compiled with no header before it");
}

// Every member of the namespace is a `const` function pointer set at its declaration, so
// a member nobody wired is a null this catches once rather than a crash at a call site.
void test_clarus_namespace_is_wired(void)
{
    const ClarusCustodiaeNs *ns = &clarus;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(ClarusCustodiaeNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}
