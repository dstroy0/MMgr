// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_TEST_GUARD_PAGE_H
#define MMGR_TEST_GUARD_PAGE_H

/**
 * @file guard_page.h
 * @brief A buffer with unreadable pages either side of it, and a way to survive touching them.
 *
 * A poison pattern catches a write past the end, because a write changes what is there. It cannot
 * catch a read, because a read leaves no trace. The only instrument that catches a read is memory
 * that is not there: a page marked no-access next to the buffer, so a load one byte too far traps
 * rather than quietly succeeding.
 *
 * The trap is caught and turned into a return value, so a suite can ask every entry the question
 * and report all the answers instead of dying on the first one.
 *
 * Where the platform has no page protection to ask for, mmgr_guard_available() is false and the
 * suite says so rather than passing on an instrument that was never armed.
 */

#include <stddef.h>

#if defined(_WIN32)
#define MMGR_GUARD_SUPPORTED 1
#elif defined(__unix__) || defined(__APPLE__)
#define MMGR_GUARD_SUPPORTED 1
#else
#define MMGR_GUARD_SUPPORTED 0
#endif

/** @brief Is there an armed guard on this platform. */
int mmgr_guard_available(void);

/** @brief Bytes in a page, or 0 when there is no guard. */
size_t mmgr_guard_page_size(void);

/**
 * @brief A writable run with a no-access page on each side.
 * @return The first byte of the writable run, or NULL when there is no guard.
 *
 * The run is exactly one page. Place a buffer of n bytes at mmgr_guard_run() + page - n and its
 * last byte is the last readable byte there is.
 */
unsigned char *mmgr_guard_run(void);

/**
 * @brief Read one byte through the guard, to prove the guard is armed.
 * @param p Address to touch.
 * @return 1 if touching it trapped.
 */
int mmgr_guard_traps_on(const unsigned char *p);

/**
 * @brief Call @p fn, reporting whether it touched a guarded page.
 * @param fn Thunk to run.
 * @param ctx Passed through.
 * @return 1 if it trapped, 0 if it returned normally.
 */
int mmgr_guard_run_thunk(void (*fn)(void *), void *ctx);

#endif
