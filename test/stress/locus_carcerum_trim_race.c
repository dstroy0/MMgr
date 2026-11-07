/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_trim_race.c
 * @brief Two threads giving the two cells at the temporary top at one instant, round after round: the cell at the
 *        top and the one above it. Whichever give comes second finds the other's trim begun or done, and every round
 *        ends with the top wound back over both, the cellblock idle.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note A round that ends with a free cell left standing at the top counts as lost, and the cellblock is emptied
 *       for the next round. Any round lost fails the program.
 */
#include "locus_carcerum/locus_carcerum.h"

#include "embed_atomic.h"
#include "host_threads.h"

#include <stdio.h>
#include <stdlib.h>

#define RACE_BYTES 4096u
#define RACE_PAYLOAD 64u

#define RACE_OWNERS(owner_) owner_(s_racer, "racer")
MMGR_OWNERS(RACE_OWNERS)

ParsMemoriaeInternae(race, RACE_BYTES);
LocusCarcerum(race_cells, MMGR_MINIMUM_SECURITY(race));

#define RACE_BLOCK (&race_cells_race_ctx)

static unsigned int s_rounds;
static embed_atomic_u32 s_next = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_ready = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_done = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_lost = EMBED_ATOMIC_INIT(0u);
static void *s_cells[2];

/** @brief Spins until the word reads at least @p want. */
static void reached(embed_atomic_u32 *word, uint32_t want)
{
    while (embed_atomic_u32_load(word) < want)
    {
        mmgr_host_threads_yield();
    }
}

/** @brief One round's gives: the cell this thread names given once both threads stand at the round's start. */
static void raced(unsigned int index)
{
    for (unsigned int round = 0u; round < s_rounds; round += 1u)
    {
        if (index == 0u)
        {
            s_cells[0] = mmgr_temporary_buf_alloc(RACE_BLOCK, RACE_PAYLOAD, s_racer);
            s_cells[1] = mmgr_temporary_buf_alloc(RACE_BLOCK, RACE_PAYLOAD, s_racer);
            embed_atomic_u32_store(&s_ready, round + 1u);
        }
        reached(&s_ready, round + 1u);
        mmgr_persistent_buf_release(RACE_BLOCK, s_cells[index], s_racer);
        (void)embed_atomic_u32_fetch_add(&s_done, 1u);
        reached(&s_done, 2u * (round + 1u));
        if (index == 0u)
        {
            // the gap, read first, runs any trim a give left owed on the open word
            (void)mmgr_buf_available(RACE_BLOCK);
            const size_t top = mmgr_series_cellularum_temporary_top(RACE_BLOCK);
            if (top != RACE_BYTES)
            {
                (void)embed_atomic_u32_fetch_add(&s_lost, 1u);
                mmgr_series_cellularum_emptied(RACE_BLOCK);
            }
        }
    }
}

static void each(void)
{
    raced(embed_atomic_u32_fetch_add(&s_next, 1u));
}

int main(int argc, char **argv)
{
    s_rounds = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 10) : 200000u;
    if (!mmgr_host_threads_run(each, 2u))
    {
        fprintf(stderr, "the threads did not both start\n");
        return 2;
    }
    const uint32_t lost = embed_atomic_u32_load(&s_lost);
    printf("rounds=%u lost=%u %s\n", s_rounds, (unsigned int)lost, (lost == 0u) ? "clean" : "BROKEN");
    return (lost == 0u) ? 0 : 1;
}
