// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "spatium/spatium.h"

void test_spat_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("spatium.h compiled with no header before it");
}

/* ---------------------------------------------------------------------------------------------
 * construction
 *
 * The module is from and the two types. What a caller wants to know about a span is a field of
 * it, read where it is wanted, so there is nothing else here to test.
 * ------------------------------------------------------------------------------------------- */

void test_from_takes_a_buffer(void)
{
    uint8_t buf[8];
    const mmgr_spat s = spat.init(&(SpatCfg){buf, sizeof buf});

    TEST_ASSERT_EQUAL_PTR(buf, s.buf);
    TEST_ASSERT_EQUAL_size_t(8u, s.cap);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
}

