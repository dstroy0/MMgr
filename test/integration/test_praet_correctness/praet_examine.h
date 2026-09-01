/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_examine.h
 * @brief Which states and transitions a run actually reached.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note The examination arm. The correctness arm says every case passed, and that says nothing about
 *       which of the machine's states and transitions any case walked through. A four state core with
 *       eight statuses has more paths than a suite this size can cover by accident, so the ones it
 *       misses are worth knowing before any of it reaches a part.
 * @note Reports and never gates. A transition nothing reached is a hole in the cases, and whether it
 *       is worth filling is a reading of the report and not a build failure. The optimization arm is
 *       this same instrument carrying timings.
 * @note Off unless PRAET_EXAMINE is 1, and every entry is then a macro expanding to nothing, so a
 *       build that did not ask for it carries no counter and no call.
 * @warning Not a knob praet_config_verdict.h asks about. Every knob there describes the image a
 *          caller ships; this describes a run of the suite, which is the harness's business and not
 *          the caller's.
 */
#ifndef MMGR_TEST_PRAET_EXAMINE_H
#define MMGR_TEST_PRAET_EXAMINE_H

#include "praet_flag_map.h"

/**
 * @brief Whether a run records the states and transitions it reached.
 */
#ifndef PRAET_EXAMINE
#define PRAET_EXAMINE 0
#endif

#if (PRAET_EXAMINE != 0) && (PRAET_EXAMINE != 1)
#error "PRAET_EXAMINE must be 0 or 1."
#endif

#if PRAET_EXAMINE

EMBED_BEGIN_DECLS

/**
 * @brief Records one channel's flag word changing.
 *
 * @param[in] channel Channel whose word changed.
 * @param[in] was     The word before.
 * @param[in] now     The word after.
 * @note Called from the one function that writes a flag word, which is what makes the record complete
 *       rather than a sample of the sites somebody remembered to instrument.
 * @note Records the core transition as a pair, and every status bit that came on or went off. A word
 *       that changed only its region descriptor records neither, and nothing here reads the region.
 */
void praet_examine_transition(embed_word channel, uint32_t was, uint32_t now);

/**
 * @brief Prints what the run reached, as a table.
 *
 * @note Written to stdout, because this is a report a person reads and not a result anything asserts
 *       on. A suite that fails still prints it, which is when it is most worth having.
 */
void praet_examine_report(void);

EMBED_END_DECLS

#else

/**
 * @brief Expands to nothing where PRAET_EXAMINE is 0.
 *
 * @param channel_ Channel whose word changed, discarded.
 * @param was_     The word before, discarded.
 * @param now_     The word after, discarded.
 * @note Casts each argument to void so a build without the instrument does not warn about the values
 *       it was handed.
 */
#define praet_examine_transition(channel_, was_, now_) ((void)(channel_), (void)(was_), (void)(now_))

/**
 * @brief Expands to nothing where PRAET_EXAMINE is 0.
 */
#define praet_examine_report() ((void)0)

#endif

#endif
