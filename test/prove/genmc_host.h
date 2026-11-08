/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file genmc_host.h
 * @brief The host a GenMC client runs on: no processor pause, a clock that stands still, and the address wait and
 *        wake as theory/locus_carcerum/LocusCarcerum/Futex.lean models them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note Included first in a client, before any MMgr or embedded_types header, and once.
 * @note The wait is the model of Futex.lean, step for step. It reads the count of wakes, then the word, and returns
 *       where the low 32 bits of the word moved. A timed wait returns at once. A wait with no end returns at once
 *       on one branch, the spurious return, and on the other waits until the count moves past the one it read.
 *       Futex.lean proves the model reaches every state the futex contract reaches and no other. A blocked
 *       execution is a sleep a real waiter can stay in forever: a lost wake.
 * @note The compare reads 32 bits, the bits the Linux futex compares. Futex.lean proves a wait that compares the
 *       whole word, as WaitOnAddress and the 64-bit compare-and-wait do, reaches no state the 32-bit wait does
 *       not. The one model holds every host.
 * @note A wake adds one to the count, and every wait the count moves past ends: a wake that ends every sleep, as
 *       the host's wakes do. One count serves every word, a spurious wake the contract allows.
 */
#ifndef MMGR_TEST_GENMC_HOST_H
#define MMGR_TEST_GENMC_HOST_H

#include <genmc.h>
#include <stdint.h>

// No processor runs the client: a spin's pause is no instruction
#define EMBED_TURNSTILE_PAUSE() ((void)0)

#include "embed_turnstile.h"

/** @brief The wakes issued, every word's. */
static embed_atomic_word s_genmc_wakes;

int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u64 nanoseconds)
{
    const embed_atomic_word_value epoch = embed_atomic_word_load(&s_genmc_wakes);

    // Explicit casts keep the low 32 bits of each value, the bits the futex compares
    if ((uint32_t)embed_atomic_word_load(word) != (uint32_t)seen)
    {
        return 1;
    }
    if (nanoseconds != EMBED_WAIT_FOREVER)
    {
        return 0;
    }
    if (__VERIFIER_nondet_int() != 0)
    {
        return 0;
    }
    __VERIFIER_assume(embed_atomic_word_load(&s_genmc_wakes) != epoch);
    return 0;
}

void embed_word_wake(embed_atomic_word *word)
{
    (void)word;
    (void)embed_atomic_word_fetch_add(&s_genmc_wakes, 1u);
}

embed_u64 embed_clock_ns(void)
{
    return 0u;
}

#endif
