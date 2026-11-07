/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_trim_genmc.c
 * @brief The two cells at the temporary top given by two threads at once, every execution the memory model allows:
 *        once both threads join, the top stands at the cellblock's end and nothing is left owed.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note A client for GenMC, which runs it under RC11, IMM and ARMv8 through harness.py prove --arm genmc. The
 *       translation unit is compiled in whole, as the race suite compiles it.
 */
#include "genmc_host.h"

#include <assert.h>
#include <pthread.h>

#include "locus_carcerum/locus_carcerum.h"

#define MMGR_OWNERS_IN_THIS_UNIT 1
#define MMGR_MISNAMED_IN_THIS_UNIT 1
#include "locus_carcerum/locus_carcerum.c"

#define GENMC_BYTES 256u
#define GENMC_PAYLOAD 16u

#define GENMC_OWNERS(owner_) owner_(s_racer, "racer")
MMGR_OWNERS(GENMC_OWNERS)

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
    assert(0);
}

ParsMemoriaeInternae(raced, GENMC_BYTES);
LocusCarcerum(raced_cells, MMGR_MINIMUM_SECURITY(raced));

#define GENMC_BLOCK (&raced_cells_raced_ctx)

static void *s_cells[2];

/** @brief One thread's give: the cell its argument names. */
static void *given(void *index)
{
    mmgr_persistent_buf_release(GENMC_BLOCK, s_cells[(uintptr_t)index], s_racer);
    return NULL;
}

int main(void)
{
    pthread_t threads[2];

    s_cells[0] = mmgr_temporary_buf_alloc(GENMC_BLOCK, GENMC_PAYLOAD, s_racer);
    s_cells[1] = mmgr_temporary_buf_alloc(GENMC_BLOCK, GENMC_PAYLOAD, s_racer);
    assert((s_cells[0] != NULL) && (s_cells[1] != NULL));
    for (uintptr_t at = 0u; at < 2u; at += 1u)
    {
        (void)pthread_create(&threads[at], NULL, given, (void *)at);
    }
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        (void)pthread_join(threads[at], NULL);
    }
    // the gap, read first, runs any trim a give left owed on the open word
    (void)mmgr_buf_available(GENMC_BLOCK);
    assert(mmgr_series_cellularum_temporary_top(GENMC_BLOCK) == GENMC_BYTES);
    assert((embed_atomic_word_load(&GENMC_BLOCK->closed) & (CARCER_CLOSED | CARCER_TRIM_PENDING)) == 0u);
    return 0;
}
