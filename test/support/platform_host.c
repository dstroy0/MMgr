// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The one symbol this library does not supply, supplied for the host.
//
// mmgr_platform_context_id() is the integrator's, because the library has no idea what a context is
// on the target - a FreeRTOS task handle, a pthread_t, a core id. Under MMGR_NEEDS_CONTEXT_ID
// (more than one worker, or debug checks on) something calls it, and a suite that links the library
// without it does not build. That is the contract working, not a gap: the `checks` environment
// failed to link with `undefined reference to mmgr_platform_context_id` until this file existed.
//
// A host suite is single-threaded and runs one case at a time, so a constant is a truthful answer:
// every borrow really does come from the same context. It has to be NON-ZERO, because the pools
// read 0 as "this slot has never been borrowed" and store the first caller's id there. A zero
// context id would make every slot look permanently unclaimed and the single-owner check would
// never compare anything, passing whatever it was given.
//
// A suite that wants to prove the check FIRES needs two distinct ids, which means driving this from
// the test rather than answering a constant. mmgr_test_set_context_id() is here for that, so such a
// suite can exist without a second copy of this file.

#include <stdint.h>

// Deliberately not 0. See above.
#define MMGR_TEST_DEFAULT_CONTEXT 1u

static uintptr_t s_context = MMGR_TEST_DEFAULT_CONTEXT;

void mmgr_test_set_context_id(uintptr_t id);

void mmgr_test_set_context_id(uintptr_t id)
{
    s_context = id;
}

uintptr_t mmgr_platform_context_id(void);

uintptr_t mmgr_platform_context_id(void)
{
    return s_context;
}
