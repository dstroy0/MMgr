/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_config_verdict.h
 * @brief Read after every knob has reported, and the one place a configuration stops the build.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note Included last, after praet_defaults.h and praet_clock.h have both had their say. Each of them
 *       gives every unset knob a default, a warning naming it, and a flag. This reads the flags.
 * @note Why the stop is here and not at each knob: an #error halts its translation unit where it
 *       stands. A build missing four knobs would report the first, get it fixed, then report the
 *       second - four rounds to learn four facts that were all knowable on the first. Warning at each
 *       knob and stopping once at the end hands the whole list over together.
 * @note There is no list in the message below, and there cannot be. The preprocessor has no way to
 *       assemble one. The warnings above are the list, one line each, and this says to go read them.
 * @warning Skipped entirely where MMGR_ACCEPT_DEFAULTS is defined. The warnings still stand and are
 *          still the report; what the define changes is whether the build continues past it.
 */
#ifndef MMGR_TEST_PRAET_CONFIG_VERDICT_H
#define MMGR_TEST_PRAET_CONFIG_VERDICT_H

#include "praet_clock.h"
#include "praet_defaults.h"

#if !defined(MMGR_ACCEPT_DEFAULTS)

#if defined(PRAET_UNSET_CHANNELS) || defined(PRAET_UNSET_SETTLE_MICROS) || defined(PRAET_UNSET_KEEPALIVE_MICROS) ||    \
    defined(PRAET_UNSET_RECOVERY) || defined(PRAET_UNSET_CLOCK_HZ) || defined(PRAET_UNSET_CLOCK_SOURCE) ||             \
    defined(PRAET_UNSET_CLOCK_CORE)
#error "This build did not declare every knob this module reads. Each one that took a library default is named in a warning above this line, with the value it took and what to set it to. Set them, or define MMGR_ACCEPT_DEFAULTS to build on the defaults and keep the warnings as the record of which ones you took."
#endif

#endif

#endif
