// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Unit suite for src/dma.
//
// test/ is exempt from the src/ style rules, so this reads as plain host C.

#include "unity.h"

#include "dma/dma.h"

// The header is included FIRST and alone, above everything but unity.h. A module header
// that only compiles once some other header has been read is a real defect, and it is
// invisible in src/ because the .c that includes it always includes its dependencies too.
void test_dma_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("dma.h compiled with no header before it");
}
