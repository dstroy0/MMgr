/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "confinium_externum/confinium_externum.h"

void test_exter_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("confinium_externum.h compiled with no header before it");
}
