/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file praet_procurator.c
 * @brief The counters behind the examination arm, and the table it prints.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note Built and driven in test. Nothing here is proposed for src until it has been run.
 * @note Compiled only where PRAET_PROCURATOR is 1. The header turns every entry into a macro expanding
 *       to nothing otherwise, so a build that did not ask for this carries none of it.
 * @warning Included by test_praet_correctness.c rather than compiled on its own, the same way the
 *          engine and the schedule are.
 */
#include "praet_procurator.h"

#if PRAET_PROCURATOR

#include <stdio.h>

/**
 * @brief How many core states there are, which is what the transition table is squared over.
 */
#define PRAET_PROCURATOR_CORES (PRAET_CORE_MASK + 1u)

/**
 * @brief Times each core transition was taken, indexed by the state before and the state after.
 *
 * @note The diagonal stays zero. The one writer skips a word that did not change, so a transition to
 *       the state a channel was already in never reaches this.
 */
static unsigned long s_core_moves[PRAET_PROCURATOR_CORES][PRAET_PROCURATOR_CORES];

/**
 * @brief Times each status came on, indexed by its token id.
 */
static unsigned long s_status_set[PRAET_STATUS_COUNT];

/**
 * @brief Times each status went off, indexed by its token id.
 */
static unsigned long s_status_cleared[PRAET_STATUS_COUNT];

/**
 * @brief Flag words written, whatever changed in them.
 *
 * @note Every counter here starts at zero because it has static storage, and one run of the suite is
 *       one program. There is nothing to clear and no entry that clears it.
 */
static unsigned long s_writes;

/**
 * @brief What each core state is called in the report.
 *
 * @note Indexed by the state's own value, which is what makes the table read in the order the states
 *       are numbered rather than the order somebody listed them.
 */
static const char *const s_core_names[PRAET_PROCURATOR_CORES] = {"detached", "attached", "busy", "ok"};

/**
 * @brief What each status is called in the report, indexed by token id.
 *
 * @note One name per id, in id order. A status added to the map without a name here reads as a blank
 *       column, which is why the assertion below counts them.
 */
static const char *const s_status_names[PRAET_STATUS_COUNT] = {
    "claimed", "settling", "detaching", "error", "stalled", "abandoned", "scrubbed", "measured",
};

EMBED_STATIC_ASSERT((sizeof s_status_names / sizeof s_status_names[0]) == PRAET_STATUS_COUNT,
                    "the examination arm has a name for every status or for none of them");

#if PRAET_OPTIMIZE

/**
 * @brief Times each piece of work was done, indexed by its PraetOpus id.
 */
static unsigned long s_work[PRAET_OPUS_KINDS];

/**
 * @brief What each piece of work is called in the report, indexed by its id.
 */
static const char *const s_work_names[PRAET_OPUS_KINDS] = {
    "attach", "detach", "submit",      "kick",         "completed", "resolve",
    "poll",   "  poll short circuited", "  poll walked", "  channel visited", "  port asked progress",
};

EMBED_STATIC_ASSERT((sizeof s_work_names / sizeof s_work_names[0]) == PRAET_OPUS_KINDS,
                    "the optimization arm has a name for every kind of work or for none of them");

void praet_procurator_opus(unsigned kind)
{
    if (kind < (unsigned)PRAET_OPUS_KINDS)
    {
        s_work[kind]++;
    }
}

/**
 * @brief Prints one figure derived from two counts, where the divisor is not zero.
 *
 * @param[in] label   What the figure is called [BORROWS].
 * @param[in] top     The count being spread.
 * @param[in] bottom  The count it is spread over.
 * @note Printed as a whole number and a remainder in hundredths, because this file reaches no floating
 *       point and a ratio is easier to read than the two counts it came from.
 */
static void praet_procurator_ratio(const char *label, unsigned long top, unsigned long bottom)
{
    if (bottom == 0uL)
    {
        printf("%-30s %10s\n", label, "no runs");
        return;
    }
    printf("%-30s %7lu.%02lu\n", label, top / bottom, ((top % bottom) * 100uL) / bottom);
}

/**
 * @brief Prints the work the run did, and what a context costs in state.
 *
 * @note No times here. A count of work is the same on every run of one build and is a fact this host
 *       can state; what that work costs is a property of a part, and a number taken on this machine
 *       would be a number about this machine wearing the library's name.
 */
static void praet_procurator_opus_report(void)
{
    printf("work done, by kind\n");
    for (unsigned kind = 0u; kind < (unsigned)PRAET_OPUS_KINDS; kind++)
    {
        printf("%-24s%10lu\n", s_work_names[kind], s_work[kind]);
    }

    printf("\nwork per transfer, and per poll\n");
    praet_procurator_ratio("polls per submit", s_work[PRAET_OPUS_POLL], s_work[PRAET_OPUS_RELATIO]);
    praet_procurator_ratio("channels walked per poll", s_work[PRAET_OPUS_ALVEUS], s_work[PRAET_OPUS_POLL]);
    praet_procurator_ratio("port asked per poll", s_work[PRAET_OPUS_PROGRESS], s_work[PRAET_OPUS_POLL]);
    praet_procurator_ratio("flag writes per submit", s_writes, s_work[PRAET_OPUS_RELATIO]);

    // What the short circuit is worth on this run, as a count out of a hundred. A suite drives the
    // machine and then looks at it, so nearly every poll here has something waiting. A program polling
    // a channel that is doing nothing is the other case entirely, and this number is where that shows
    praet_procurator_ratio("polls short circuited in 100", s_work[PRAET_OPUS_POLL_SHORT] * 100uL,
                        s_work[PRAET_OPUS_POLL]);

    printf("\nstate one context costs\n");
    printf("%-30s %10u\n", "channels", (unsigned)PRAET_CHANNELS);
    printf("%-30s %10u\n", "bytes in a context", (unsigned)sizeof(PraetOrdo));

    // Spread over the channels, and the context wide members are in the number. Naming it that way
    // rather than calling it a per channel cost, which it is not: the settle deadline, the two
    // volatiles and the elapsed count are carried once however many channels there are
    printf("%-30s %10u\n", "bytes per channel, all in", (unsigned)(sizeof(PraetOrdo) / PRAET_CHANNELS));

    printf("\nno times are reported here. what this work costs is a property of a part\n");
}

#endif

void praet_procurator_transitus(embed_word channel, uint32_t was, uint32_t now)
{
    (void)channel;

    s_writes++;

    const uint32_t before = was & PRAET_CORE_MASK;
    const uint32_t after = now & PRAET_CORE_MASK;

    if (before != after)
    {
        s_core_moves[before][after]++;
    }

    // Only the bits that changed, and each counted on the side it moved to. A status that was already
    // on and stayed on says nothing about a path being walked
    const uint32_t turned = (was ^ now) & PRAET_STATUS_MASK;

    for (unsigned id = 0u; id < PRAET_STATUS_COUNT; id++)
    {
        const uint32_t bit = PRAET_STATUS_BIT(id);

        if ((turned & bit) == 0u)
        {
            continue;
        }
        if ((now & bit) != 0u)
        {
            s_status_set[id]++;
        }
        else
        {
            s_status_cleared[id]++;
        }
    }
}

void praet_procurator_report(void)
{
    unsigned reached = 0u;
    unsigned possible = 0u;

    printf("\n--- praet examination: what this run reached ---\n");
    printf("flag words written: %lu\n\n", s_writes);

    printf("core transitions, from the row to the column\n");
    printf("%-10s", "");
    for (unsigned after = 0u; after < PRAET_PROCURATOR_CORES; after++)
    {
        printf("%10s", s_core_names[after]);
    }
    printf("\n");

    for (unsigned before = 0u; before < PRAET_PROCURATOR_CORES; before++)
    {
        printf("%-10s", s_core_names[before]);
        for (unsigned after = 0u; after < PRAET_PROCURATOR_CORES; after++)
        {
            if (before == after)
            {
                // The one writer never records a word that did not change, so this cell cannot fill
                printf("%10s", "-");
                continue;
            }
            possible++;
            if (s_core_moves[before][after] != 0uL)
            {
                reached++;
                printf("%10lu", s_core_moves[before][after]);
            }
            else
            {
                printf("%10s", "never");
            }
        }
        printf("\n");
    }

    printf("\nstatuses, times each came on and went off\n");
    printf("%-12s%10s%10s\n", "", "on", "off");
    for (unsigned id = 0u; id < PRAET_STATUS_COUNT; id++)
    {
        printf("%-12s", s_status_names[id]);
        if (s_status_set[id] != 0uL)
        {
            printf("%10lu", s_status_set[id]);
        }
        else
        {
            printf("%10s", "never");
        }
        if (s_status_cleared[id] != 0uL)
        {
            printf("%10lu\n", s_status_cleared[id]);
        }
        else
        {
            printf("%10s\n", "never");
        }
    }

    printf("\n%u of %u core transitions reached\n", reached, possible);

#if PRAET_OPTIMIZE
    printf("\n");
    praet_procurator_opus_report();
#endif

    printf("--- end of examination ---\n\n");
}

#endif
