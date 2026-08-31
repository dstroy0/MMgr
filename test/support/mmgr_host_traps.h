/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 */
/**
 * @file mmgr_host_traps.h
 * @brief The reporting forms of MMGR_ASSERT and MMGR_FATAL, which need a host to report on.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note These lived in mmgr_config.h behind MMGR_DEBUG_CHECKS, which put stdio.h and stdlib.h inside
 *       src/ whenever the checks environment was built. A library for a target with no libc cannot
 *       reach for fprintf and abort, and the flag that pulled them in was a test environment's.
 * @note Both macros are #ifndef guarded in mmgr_config.h, so defining them here ahead of it is the
 *       seam rather than a change to it. src/ keeps the two libc-free forms: MMGR_ASSERT expands to a
 *       sizeof that type checks its condition and never evaluates it, and MMGR_FATAL spins.
 * @warning Reached only by a forced include on the checks environment's targets. A suite compiled
 *          without it gets the inert forms, which is what every other environment wants.
 */
#ifndef MMGR_HOST_TRAPS_H
#define MMGR_HOST_TRAPS_H

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Set to 1 to arm the reporting forms below.
 *
 * @note Lived in mmgr_config.h until nothing under src/ read it. Every remaining mention there is
 *       comment text describing what a checks build catches, so the knob itself is the suites' and
 *       belongs with them.
 * @note Takes 0 or 1 and nothing else. A switch read with #if treats any non-zero as set, so the
 *       check below is what keeps a mistyped value from quietly meaning on.
 * @note A suite built for another environment never reaches this header, and #if on an undefined
 *       name is 0, so a case testing it there reads false without needing a default.
 */
#ifndef MMGR_DEBUG_CHECKS
#define MMGR_DEBUG_CHECKS 0
#endif
#if (MMGR_DEBUG_CHECKS != 0) && (MMGR_DEBUG_CHECKS != 1)
#error "MMGR_DEBUG_CHECKS must be 0 or 1"
#endif

/**
 * @brief Reports a broken expectation and stops, naming the file and line that reached it.
 *
 * @param[in] cond_ Condition the caller expects to hold.
 * @param[in] msg_  String literal describing the expectation.
 * @note fflush(NULL) before the trap, or a harness that buffers its progress on stdout loses every
 *       line of it to the abort and reports the failure with nothing naming which case reached it.
 * @warning cond_ must carry no side effect. The form in src/ never evaluates it, so an assert
 *          holding the work would do nothing in the build that ships.
 */
#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond_, msg_)                                                                                       \
    ((cond_) ? (void)0                                                                                                 \
             : (void)(fprintf(stderr, "MMGR_ASSERT failed: %s\n  %s:%d\n", (msg_), __FILE__, __LINE__), fflush(NULL),  \
                      abort()))
#endif

/**
 * @brief Reports an illegal call and stops, naming the file and line that reached it.
 *
 * @param[in] msg_ String literal naming what was violated.
 * @note Aborts rather than spinning, which is what keeps a suite from hanging on a fault the way the
 *       shipping form would.
 * @note fflush(NULL) before the trap, for the reason MMGR_ASSERT flushes.
 */
#ifndef MMGR_FATAL
#define MMGR_FATAL(msg_)                                                                                               \
    ((void)(fprintf(stderr, "MMGR_FATAL: %s\n  %s:%d\n", (msg_), __FILE__, __LINE__), fflush(NULL), abort()))
#endif

#endif
