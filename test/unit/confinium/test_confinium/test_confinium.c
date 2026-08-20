// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "confinium/confinium.h"

void test_confin_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("confinium.h compiled with no header before it");
}
