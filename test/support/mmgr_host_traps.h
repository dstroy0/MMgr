/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 */
/**
 * @file mmgr_host_traps.h
 * @brief The report forms of MMGR_ASSERT and MMGR_FATAL, which need a host to report on.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note These live here and not in mmgr_config.h, because there they would put stdio.h and stdlib.h
 *       inside src/ for the checks environment. A library for a target with no libc cannot
 *       reach for fprintf and abort, and the flag that pulls them in is a test environment's.
 * @note Both macros sit under #ifndef in mmgr_config.h, and so a definition here ahead of it is the
 *       seam and not a change to it. The src/ tree keeps the two libc-free forms. MMGR_ASSERT expands
 *       to a sizeof that type checks its condition and never evaluates it, and MMGR_FATAL spins.
 * @warning Reached only by a forced include on the checks environment's targets. A suite compiled
 *          without it gets the inert forms, as every other environment wants.
 */
#ifndef MMGR_HOST_TRAPS_H
#define MMGR_HOST_TRAPS_H

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Set to 1 to arm the report forms below.
 *
 * @note Nothing under src/ reads it. Every mention there is comment text about what a checks build
 *       catches, and so the knob itself is the suites' and belongs with them.
 * @note Takes 0 or 1, alone. A switch read with #if treats any non-zero as set, and so the
 *       check below keeps a mistyped value from a quiet read as on.
 * @note A suite built for another environment never reaches this header, and #if on an undefined
 *       name is 0. A case that tests it there reads false with no need of a default.
 */
#ifndef MMGR_DEBUG_CHECKS
#define MMGR_DEBUG_CHECKS 0
#endif
#if (MMGR_DEBUG_CHECKS != 0) && (MMGR_DEBUG_CHECKS != 1)
#error "MMGR_DEBUG_CHECKS must be 0 or 1"
#endif

/**
 * @brief Reports a broken expectation and stops, with the file and line that reached it.
 *
 * @param[in] cond_ Condition the caller expects to hold.
 * @param[in] msg_  String literal that describes the expectation.
 * @note fflush(NULL) before the trap. Otherwise a harness that buffers its progress on stdout loses
 *       every line of it to the abort, and the failure report names no case.
 * @warning cond_ must carry no side effect. The form in src/ never evaluates it. An assert
 *          with the work inside would do nothing in the build for a part.
 */
#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond_, msg_)                                                                                       \
    ((cond_) ? (void)0                                                                                                 \
             : (void)(fprintf(stderr, "MMGR_ASSERT failed: %s\n  %s:%d\n", (msg_), __FILE__, __LINE__), fflush(NULL),  \
                      abort()))
#endif

/**
 * @brief Reports an illegal call and stops, with the file and line that reached it.
 *
 * @param[in] msg_ String literal that names the broken rule.
 * @note Aborts and does not spin, which keeps a suite from a hang on a fault the way the
 *       form for a part would.
 * @note fflush(NULL) before the trap, for the reason MMGR_ASSERT flushes.
 */
#ifndef MMGR_FATAL
#define MMGR_FATAL(msg_)                                                                                               \
    ((void)(fprintf(stderr, "MMGR_FATAL: %s\n  %s:%d\n", (msg_), __FILE__, __LINE__), fflush(NULL), abort()))
#endif

#endif
