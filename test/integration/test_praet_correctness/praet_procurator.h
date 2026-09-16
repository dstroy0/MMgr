/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_procurator.h
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
 * @note Off unless PRAET_PROCURATOR is 1, and every entry is then a macro expanding to nothing, so a
 *       build that did not ask for it carries no counter and no call.
 * @warning Not a knob praet_iudex.h asks about. Every knob there describes the image a
 *          caller ships; this describes a run of the suite, which is the harness's business and not
 *          the caller's.
 */
#ifndef MMGR_TEST_PRAET_PROCURATOR_H
#define MMGR_TEST_PRAET_PROCURATOR_H

#include "praet_tabula_vexillorum.h"

/**
 * @brief Whether a run records the work it did, on top of the states it reached.
 *
 * @note The optimization arm, and it is the examination arm carrying more counters. What it records
 *       is work: entries called, channels walked, times the poll short circuited, times the port was
 *       asked anything, and the bytes of state a context costs.
 * @note No times, and none on purpose. A count of work is the same on every run of one build, so it
 *       is a fact a host can state. What that work costs is a property of a part, and a number taken
 *       here would be a number about this machine dressed as a number about the library.
 * @note Reports and never gates, the same as the examination arm. A count that grew is worth reading
 *       and is not a build failure, because whether it matters is measured somewhere else.
 */
#ifndef PRAET_OPTIMIZE
#define PRAET_OPTIMIZE 0
#endif

#if (PRAET_OPTIMIZE != 0) && (PRAET_OPTIMIZE != 1)
#error "PRAET_OPTIMIZE must be 0 or 1."
#endif

/**
 * @brief Whether a run records the states and transitions it reached.
 *
 * @note Turned on by PRAET_OPTIMIZE as well, since the work counts are read beside the states they
 *       were spent reaching and a report with one half missing says less than either half alone.
 */
#ifndef PRAET_PROCURATOR
#if PRAET_OPTIMIZE
#define PRAET_PROCURATOR 1
#else
#define PRAET_PROCURATOR 0
#endif
#endif

#if (PRAET_PROCURATOR != 0) && (PRAET_PROCURATOR != 1)
#error "PRAET_PROCURATOR must be 0 or 1."
#endif

#if PRAET_OPTIMIZE && !PRAET_PROCURATOR
#error "PRAET_OPTIMIZE needs PRAET_PROCURATOR, which it turns on by itself unless something set it to 0."
#endif

#if PRAET_PROCURATOR

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
void praet_procurator_transitus(embed_word channel, uint32_t was, uint32_t now);

/**
 * @brief Prints what the run reached, as a table.
 *
 * @note Written to stdout, because this is a report a person reads and not a result anything asserts
 *       on. A suite that fails still prints it, which is when it is most worth having.
 * @note Prints the work the run did as well, where PRAET_OPTIMIZE is on.
 */
void praet_procurator_report(void);

EMBED_END_DECLS

#else

/**
 * @brief Expands to nothing where PRAET_PROCURATOR is 0.
 *
 * @param channel_ Channel whose word changed, discarded.
 * @param was_     The word before, discarded.
 * @param now_     The word after, discarded.
 * @note Casts each argument to void so a build without the instrument does not warn about the values
 *       it was handed.
 */
#define praet_procurator_transitus(channel_, was_, now_) ((void)(channel_), (void)(was_), (void)(now_))

/**
 * @brief Expands to nothing where PRAET_PROCURATOR is 0.
 */
#define praet_procurator_report() ((void)0)

#endif

#if PRAET_OPTIMIZE

EMBED_BEGIN_DECLS

/**
 * @brief The pieces of work an optimization run counts.
 *
 * @note One id per entry a caller can reach, plus the two things the poll does that are worth telling
 *       apart. Dense from zero, because the counters are an array indexed by these.
 */
typedef enum
{
    PRAET_OPUS_ADNECTERE = 0,    /**< praet_ordo_adnectere ran. */
    PRAET_OPUS_SEPARARE = 1,    /**< praet_ordo_separare ran. */
    PRAET_OPUS_RELATIO = 2,    /**< praet_ordo_relatio ran. */
    PRAET_OPUS_EFFICERE = 3,      /**< praet_ordo_efficere ran. */
    PRAET_OPUS_COMPLETED = 4, /**< praet_ordo_completed ran. */
    PRAET_OPUS_RESOLVE = 5,   /**< praet_ordo_resolve ran. */
    PRAET_OPUS_POLL = 6,      /**< praet_ordo_poll ran. */
    PRAET_OPUS_POLL_SHORT = 7, /**< A poll that found nothing to do and returned. */
    PRAET_OPUS_POLL_WALK = 8,  /**< A poll that walked every channel. */
    PRAET_OPUS_ALVEUS = 9,    /**< One channel visited inside a walk. */
    PRAET_OPUS_PROGRESS = 10,  /**< The port was asked how far a channel had got. */
    PRAET_OPUS_KINDS = 11      /**< How many kinds there are, which is what the counters are sized by. */
} PraetOpus;

/**
 * @brief Counts one piece of work.
 *
 * @param[in] kind What was done, as a PraetOpus.
 */
void praet_procurator_opus(unsigned kind);

EMBED_END_DECLS

#else

/**
 * @brief Expands to nothing where PRAET_OPTIMIZE is 0.
 *
 * @param kind_ What was done, discarded.
 */
#define praet_procurator_opus(kind_) ((void)0)

#endif

#endif
