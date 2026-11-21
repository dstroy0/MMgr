/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file genmc_host.h
 * @brief The host a GenMC client runs on. No processor pause, a clock that is stationary, and the address wait and
 *        wake as theory/locus_carcerum/LocusCarcerum/Futex.lean models them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note Included first in a client, before any MMgr or embedded_types header, and once.
 * @note The wait is the model of Futex.lean, step for step. It reads the count of wakes, then the word, and returns
 *       where the low 32 bits of the word moved. A timed wait returns at once, and a wait with no end returns at
 *       once on one branch, the spurious return. On the other it waits until the count moves past the one it read.
 *       Futex.lean proves the model reaches every state the futex contract reaches and no other. A blocked
 *       execution is a sleep a real waiter can stay in forever: a lost wake.
 * @note The compare reads 32 bits, the bits the Linux futex compares. Futex.lean proves a wait that compares the
 *       whole word, as WaitOnAddress and the 64-bit compare-and-wait do, reaches no state the 32-bit wait does
 *       not. The one model holds every host.
 * @note A wake adds one to the count, and every wait the count moves past ends. That is a wake that ends every
 *       sleep, as the host's wakes do. One count serves every word, a spurious wake the contract allows.
 */
#ifndef MMGR_TEST_GENMC_HOST_H
#define MMGR_TEST_GENMC_HOST_H

#include <genmc.h>
#include <stdint.h>

// No processor runs the client: a spin's pause is no instruction
#define EMBED_TURNSTILE_SPIN_PAUSE() ((void)0)

#include "embed_turnstile.h"

/** @brief The wakes issued, every word's. */
static embed_atomic_word s_genmc_wakes;

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    const embed_atomic_word_value epoch =
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_genmc_wakes);

    // Explicit casts retain the low 32 bits of each value, the bits the futex compares
    if ((uint32_t)EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word) !=
        (uint32_t)args->old_value)
    {
        return 1;
    }
    if (args->nanoseconds != EMBED_WAIT_INFINITE)
    {
        return 0;
    }
    if (__VERIFIER_nondet_int() != 0)
    {
        return 0;
    }
    __VERIFIER_assume(EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_genmc_wakes) !=
                      epoch);
    return 0;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    (void)args;
    (void)EMBED_CALL(embed_atomic_word_table.fetch_add, EmbedAtomicWordFetchAddCfg, .word = &s_genmc_wakes,
                     .value = 1u);
}

embed_word embed_clock_ns(void)
{
    return 0u;
}

/**
 * @brief The place of the thread, 0 for every thread.
 *
 * @return 0.
 * @note The clients run with no regions, and no take reads the place. The turnstile table names the hook, and so
 *       the host defines it.
 */
embed_u32 embed_thread_place(void)
{
    return 0u;
}

#endif
