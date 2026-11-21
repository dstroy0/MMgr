/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_sleep_prove.c
 * @brief Sleep.lean's c_sleep_gated proved on the C. Each wait that carcer_passed or carcer_shut makes is on the
 *        closed word. The value has the closed bit and the sleeper bit, and the word has the sleeper bit.
 *
 * @note The translation unit comes in whole, as the thread clients take it, and so the harness reaches the two
 *       static functions. The harness is the host. It defines embed_word_wait, embed_word_wake and embed_clock_ns,
 *       and the link leaves host_wait.c out.
 * @note The writer opens the cellblock at a step of its choice: in the work of the pass, at a pause, or at a
 *       sleep. After PROVE_PAUSES pauses it opens, and so each loop has a bound. The clock reads any value, and so
 *       each step of a backoff can be a pause alone or a wait.
 * @note A wait returns at once where the word does not hold the value, as the futex compare does. A sleep ends at
 *       the open of the writer, as Futex.lean models it.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 */
/** @brief The pause of a backoff step, which the harness hooks so that the writer can open there. */
static void prove_paused(void);
#define EMBED_TURNSTILE_SPIN_PAUSE() prove_paused()

#include "locus_carcerum/locus_carcerum.h"

#include "prove.h"

#define MMGR_OWNERS_IN_THIS_UNIT 1
#define MMGR_MISNAMED_IN_THIS_UNIT 1
#include "locus_carcerum/locus_carcerum.c"

#define PROVE_OWNERS(owner_) owner_(s_passer, "passer")
MMGR_OWNERS(PROVE_OWNERS)
PROVE_NO_MISNAMED()

/** @brief The pauses after which the writer opens. */
#define PROVE_PAUSES 2u

/** @brief The cellblock of the passes: every member zero, and so one open lane and no option. */
static SeriesCellularum s_series;

/** @brief The pauses since the last open. */
static unsigned int s_pauses;

/** @brief The open of the writer: the fetch-and that clears the closed bit and the sleeper bit, Step.open_. */
static void prove_writer_opened(void)
{
    (void)EMBED_CALL(embed_atomic_word_table.fetch_and, EmbedAtomicWordFetchAndCfg, .word = &s_series.closed,
                     .value = ~(CARCER_CLOSED | CARCER_CLOSED_SLEEPERS));
    s_pauses = 0u;
}

static void prove_paused(void)
{
    s_pauses += 1u;
    if (((nondet_byte() & 1u) != 0u) || (s_pauses >= PROVE_PAUSES))
    {
        prove_writer_opened();
    }
}

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    const embed_atomic_word_value current_word =
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word);

    PROVE_ASSERT(args->word == &s_series.closed, "each wait is on the closed word");
    PROVE_ASSERT((args->old_value & CARCER_CLOSED) != 0u, "c_sleep_gated: the value read has the closed bit");
    PROVE_ASSERT((args->old_value & CARCER_CLOSED_SLEEPERS) != 0u, "c_sleep_gated: the value has the sleeper bit set");
    PROVE_ASSERT((current_word & CARCER_CLOSED_SLEEPERS) != 0u, "flag_is_step: the fetch-or comes ahead of the wait");
    // Explicit cast widens the limit to 64 bits for the bound of wait_bounded
    PROVE_ASSERT((embed_u64)args->nanoseconds <= (1ULL << 30u), "wait_bounded: the limit is 2^30 ns at max");
    if (current_word != args->old_value)
    {
        return 1;
    }
    prove_writer_opened();
    return 0;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    (void)args;
}

embed_word embed_clock_ns(void)
{
    // Explicit cast takes the input to the width of the clock, which rolls at the word
    return (embed_word)nondet_size();
}

/** @brief The place of the thread, 0: the cellblock has no regions, and the turnstile table names the hook. */
embed_u32 embed_thread_place(void)
{
    return 0u;
}

/** @brief The work of a pass: the writer can open while the pass is in, and the answer is 1 where it lands. */
static uintptr_t prove_work(SeriesCellularum *series, const void *context, size_t permit)
{
    (void)series;
    (void)context;
    if ((nondet_byte() & 1u) != 0u)
    {
        prove_writer_opened();
    }
    // Explicit cast carries the permit's low bit as the answer
    return (uintptr_t)(permit & 1u);
}

/** @brief A pass through a cellblock whose closed word holds any value. It lands, and the gate holds at each wait. */
static void prove_passed(void)
{
    // Explicit cast takes the input to the width of the closed word
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_series.closed,
               .value = (embed_atomic_word_value)nondet_size());
    s_pauses = 0u;
    const uintptr_t answer = carcer_passed(&s_series, 1u, prove_work, NULL);

    PROVE_ASSERT(answer == 1u, "the pass lands once the writer opens");
}

/** @brief A writer's shut of a cellblock whose closed word holds any value. It takes the closed bit, and the gate
 *         holds at each wait on the bit of another writer. */
static void prove_shut(void)
{
    // Explicit cast takes the input to the width of the closed word
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_series.closed,
               .value = (embed_atomic_word_value)nondet_size());
    s_pauses = 0u;
    carcer_shut(&s_series);
    PROVE_ASSERT((EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_series.closed) &
                  CARCER_CLOSED) != 0u,
                 "the writer holds the closed bit");
}

int main(void)
{
    prove_passed();
    prove_shut();
    PROVE_DONE();
    return 0;
}
