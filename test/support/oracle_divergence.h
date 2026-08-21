// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_TEST_ORACLE_DIVERGENCE_H
#define MMGR_TEST_ORACLE_DIVERGENCE_H

/**
 * @file oracle_divergence.h
 * @brief Skip one case on the oracle side of the A/B, and say why on the way out.
 *
 * The suite is run twice: once against this library, and once with MMGR_ORACLE_LIBC on, which
 * points the namespaces at libc. Both runs have to be green, because a red B side stops being a
 * comparison and starts being noise nobody reads.
 *
 * Almost every case holds on both sides, which is the whole point of building the oracle. A few do
 * not, and they are the ones pinning somewhere this library deliberately does not agree with libc:
 * a rounding rule C leaves to the implementation, a bound libc does not have, a spelling of nan
 * that is the platform's rather than anyone's specification. Those cases are the reason the
 * behavior is written down at all, so they are not weakened to something both sides can pass -
 * they are pinned against this library and stood down on the other run.
 *
 * Put it as the first statement of the case, with a reason that says what diverges:
 *
 *     void test_a_tie_rounds_to_even(void)
 *     {
 *         MMGR_SKIP_ON_ORACLE("libc breaks the tie away from zero, this library breaks it to even");
 *         ...
 *     }
 *
 * A skipped case reports as Ignored, which Unity counts and ctest does not fail on, so the B run
 * still says out loud how many claims it stood down and which.
 */

#if defined(MMGR_ORACLE_LIBC) && MMGR_ORACLE_LIBC
#define MMGR_SKIP_ON_ORACLE(why) TEST_IGNORE_MESSAGE("oracle build: " why)
#else
#define MMGR_SKIP_ON_ORACLE(why)                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (0)
#endif

#endif
