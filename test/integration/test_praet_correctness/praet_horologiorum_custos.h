/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_horologiorum_custos.h
 * @brief Where the microseconds come from: the caller's clock, or one of ours pinned to a core they
 *        name, and the scaling between that clock's ticks and a microsecond.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note Every deadline in this module is microseconds, and a clock is what makes a microsecond mean
 *       anything. A build with no clock declared has deadlines it cannot honor, so it stops and asks
 *       for one.
 * @note What the clock is does not matter here. A counter that runs and can be read is the whole
 *       requirement - this scales its ticks against the frequency the caller states and never looks
 *       at where it came from.
 */
#ifndef MMGR_TEST_PRAET_HOROLOGIORUM_CUSTOS_H
#define MMGR_TEST_PRAET_HOROLOGIORUM_CUSTOS_H

// EMBED_STATIC_ASSERT and embed_word, for the scaling below. Reached the same way every other file
// here reaches them, so this header stands on its own rather than on what included it
#include "memoriam_praetereo/memoriam_praetereo.h"

#include "praet_praefinitum.h"

/**
 * @brief The clock is the caller's, read through the port.
 */
#define PRAET_CLOCK_CALLER 1

/**
 * @brief The clock is ours, pinned to the core PRAET_CLOCK_CORE names.
 */
#define PRAET_CLOCK_OWN 0

// Which architecture this is, and what that architecture gives a clock. The default source below is
// picked off that rather than assumed, since an architecture with no counter has nothing to pin
#include "praet_platform_detection.h"

/**
 * @brief Ticks the clock counts in one second.
 *
 * @note This is the PLL figure, and it is what everything else scales against. There is no way to
 *       read it off a part at compile time, so it is declared or the build stops.
 * @note The default is one megahertz, which reads a tick as a microsecond. Not a guess at the part's
 *       frequency - guessing one would make every deadline in the module quietly wrong by whatever
 *       the guess missed by. One megahertz is the floor this module supports, so what a build gets by
 *       taking it is timing that is honest about being unscaled.
 */
#ifndef PRAET_CLOCK_HZ
#define PRAET_CLOCK_HZ 1000000u
#define PRAET_UNSET_CLOCK_HZ 1
#warning "No clock is declared, so PRAET_CLOCK_HZ took the library default of 1000000 and a tick is read as one microsecond. Every deadline in this module is then wrong by whatever the real frequency is. Set it to the frequency of the clock this reads."
#endif

/**
 * @brief Whether the clock is the caller's or ours.
 *
 * @note PRAET_CLOCK_CALLER where the caller already runs a counter this can read, which is the usual
 *       answer on a part whose application owns the timers. PRAET_CLOCK_OWN where it does not, and
 *       this pins one to a core.
 * @note The default is ours, because that is the arm that needs nothing from the caller. A build that
 *       took this default and meant the other one finds out from the message, not from a timer that
 *       was never started.
 */
#ifndef PRAET_CLOCK_SOURCE
#define PRAET_UNSET_CLOCK_SOURCE 1
#if PRAET_PLATFORM_HAS_CYCLE_COUNTER
#define PRAET_CLOCK_SOURCE PRAET_CLOCK_OWN
#warning "PRAET_CLOCK_SOURCE was not set and was derived as PRAET_CLOCK_OWN, because this architecture defines a cycle counter a port can read. Say so, or set PRAET_CLOCK_CALLER to have this read a clock you already run."
#else
#define PRAET_CLOCK_SOURCE PRAET_CLOCK_CALLER
#warning "PRAET_CLOCK_SOURCE was not set and was derived as PRAET_CLOCK_CALLER, because this architecture defines no cycle counter a timer could be pinned to. Say so, and supply the clock."
#endif
#endif

// A source is one of the two named arms. A third value is somebody passing a frequency, a core number
// or a boolean here, and every test on it below would take the caller arm silently
#if (PRAET_CLOCK_SOURCE != PRAET_CLOCK_CALLER) && (PRAET_CLOCK_SOURCE != PRAET_CLOCK_OWN)
#error "PRAET_CLOCK_SOURCE must be PRAET_CLOCK_CALLER or PRAET_CLOCK_OWN."
#endif

// Asking this to pin a timer on an architecture that defines no counter to pin. The caller has to
// supply the clock there, and finding that out from a timer that never ticks is the whole reason the
// capability is derived rather than assumed
#if (PRAET_CLOCK_SOURCE == PRAET_CLOCK_OWN) && !PRAET_PLATFORM_HAS_CYCLE_COUNTER
#error "PRAET_CLOCK_SOURCE is PRAET_CLOCK_OWN, but this architecture defines no cycle counter for this to pin a timer to. Set PRAET_CLOCK_SOURCE to PRAET_CLOCK_CALLER and supply the clock. An ARMv6-M part and a host build both land here."
#endif

#if PRAET_CLOCK_SOURCE == PRAET_CLOCK_OWN

/**
 * @brief The core our timer is pinned to.
 *
 * @note A field of PRAET_CLOCK_SOURCE being PRAET_CLOCK_OWN, and required once it is. Which core is
 *       the caller's call - this only needs to know the timer is somewhere it will keep running.
 * @note What the number means is the platform's business. Zero, one, whatever a part calls its cores;
 *       nothing here reads it beyond handing it to the port.
 */
#ifndef PRAET_CLOCK_CORE
#define PRAET_CLOCK_CORE PRAET_PLATFORM_CLOCK_CORE
#define PRAET_UNSET_CLOCK_CORE 1
#warning "PRAET_CLOCK_CORE was not set and took PRAET_PLATFORM_CLOCK_CORE, which is core zero on every family this library targets. Set it to the core this should pin its timer to."
#endif

#else

// A field of the arm this build did not take. The core would sit in the build looking set while
// nothing pins a timer anywhere, and the caller would find that out from behavior
#ifdef PRAET_CLOCK_CORE
#error "PRAET_CLOCK_CORE is set but PRAET_CLOCK_SOURCE is PRAET_CLOCK_CALLER. Nothing pins a timer in this build, so there is no core to pin it to. Set PRAET_CLOCK_SOURCE to PRAET_CLOCK_OWN, or take PRAET_CLOCK_CORE out."
#endif

#endif

/**
 * @brief Ticks this clock counts in one microsecond.
 *
 * @note A compile-time constant, so the division below folds to a multiply and a shift rather than
 *       reaching a divide instruction on a part that may not have one.
 */
#define PRAET_TICKS_PER_MICRO (PRAET_CLOCK_HZ / 1000000u)

/**
 * @brief Converts a tick count from this clock into microseconds.
 *
 * @param[in] ticks_ Ticks read off the clock.
 * @return           Those ticks as whole microseconds, rounded down.
 * @note Rounded down on purpose. A deadline reached a fraction of a microsecond early is a window
 *       that was slightly short; one reached late is a stall reported after the fact.
 */
#define PRAET_CLOCK_MICROS(ticks_) ((ticks_) / (embed_word)PRAET_TICKS_PER_MICRO)

// A tick has to divide a microsecond evenly, or the conversion above throws away a different amount
// at every frequency and nothing here says by how much. Every part this library targets runs at a
// whole number of megahertz
EMBED_STATIC_ASSERT((PRAET_CLOCK_HZ % 1000000u) == 0u,
                    "PRAET_CLOCK_HZ is not a whole number of megahertz, and this module scales in whole ticks");

EMBED_STATIC_ASSERT(PRAET_TICKS_PER_MICRO >= 1u,
                    "PRAET_CLOCK_HZ is below one megahertz, which cannot resolve the microsecond deadlines this "
                    "module is written in");

#endif
