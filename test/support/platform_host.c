/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <stdint.h>

#define MMGR_TEST_DEFAULT_CONTEXT 1u

static uintptr_t s_context = MMGR_TEST_DEFAULT_CONTEXT;

void mmgr_test_set_context_id(uintptr_t id);

void mmgr_test_set_context_id(uintptr_t id)
{
    s_context = id;
}


