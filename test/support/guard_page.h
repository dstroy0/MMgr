/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#ifndef MMGR_TEST_GUARD_PAGE_H
#define MMGR_TEST_GUARD_PAGE_H

#include <stddef.h>

#if defined(_WIN32)
#define MMGR_GUARD_SUPPORTED 1
#elif defined(__unix__) || defined(__APPLE__)
#define MMGR_GUARD_SUPPORTED 1
#else
#define MMGR_GUARD_SUPPORTED 0
#endif

int mmgr_guard_available(void);

size_t mmgr_guard_page_size(void);

unsigned char *mmgr_guard_run(void);

int mmgr_guard_traps_on(const unsigned char *p);

int mmgr_guard_run_thunk(void (*fn)(void *), void *ctx);

#endif
