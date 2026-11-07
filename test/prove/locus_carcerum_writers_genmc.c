/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file locus_carcerum_writers_genmc.c
 * @brief Two writers on one closed bit, every execution the memory model allows: a temporary take only a spill
 *        can meet, against the give of the cell at the temporary top. Whichever holds the bit first, the other
 *        backs off and sleeps on the closed word or leaves its trim owed, and once both join and every cell goes
 *        back the cellblock is idle, nothing owed, every owner at zero.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note A client for GenMC, run by harness.py prove --arm genmc. The host's waits are the futex contract of
 *       genmc_host.h: a sleep no wake ends leaves its thread blocked, which GenMC reports.
 */
#include "genmc_host.h"

#include <assert.h>
#include <pthread.h>

#include "locus_carcerum/locus_carcerum.h"

#define MMGR_OWNERS_IN_THIS_UNIT 1
#define MMGR_MISNAMED_IN_THIS_UNIT 1
#include "locus_carcerum/locus_carcerum.c"

#define GENMC_BYTES 160u
#define GENMC_PAYLOAD 16u
#define GENMC_PERSISTENT_MOST 8u

#define GENMC_OWNERS(owner_) owner_(s_writer, "writer")
MMGR_OWNERS(GENMC_OWNERS)

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
    assert(0);
}

ParsMemoriaeInternae(written, GENMC_BYTES);
LocusCarcerum(written_cells, MMGR_MINIMUM_SECURITY(written));

#define GENMC_BLOCK (&written_cells_written_ctx)

static void *s_temporary[2];
static void *s_persistent[GENMC_PERSISTENT_MOST];
static unsigned int s_persistent_count;
static void *s_spilled;

/** @brief The take only a spill can meet. */
static void *spilled(void *unused)
{
    (void)unused;
    s_spilled = mmgr_temporary_buf_alloc(GENMC_BLOCK, GENMC_PAYLOAD, s_writer);
    return NULL;
}

/** @brief The give of the cell at the temporary top. */
static void *given(void *unused)
{
    (void)unused;
    mmgr_persistent_buf_release(GENMC_BLOCK, s_temporary[1], s_writer);
    return NULL;
}

int main(void)
{
    pthread_t threads[2];

    s_temporary[0] = mmgr_temporary_buf_alloc(GENMC_BLOCK, GENMC_PAYLOAD, s_writer);
    s_temporary[1] = mmgr_temporary_buf_alloc(GENMC_BLOCK, GENMC_PAYLOAD, s_writer);
    assert((s_temporary[0] != NULL) && (s_temporary[1] != NULL));
    // persistent cells until a take is refused: the gap left holds no cell
    while (s_persistent_count < GENMC_PERSISTENT_MOST)
    {
        void *const cell = mmgr_persistent_buf_alloc(GENMC_BLOCK, GENMC_PAYLOAD, s_writer);
        if (cell == NULL)
        {
            break;
        }
        s_persistent[s_persistent_count] = cell;
        s_persistent_count += 1u;
    }
    assert(s_persistent_count < GENMC_PERSISTENT_MOST);

    (void)pthread_create(&threads[0], NULL, spilled, NULL);
    (void)pthread_create(&threads[1], NULL, given, NULL);
    (void)pthread_join(threads[0], NULL);
    (void)pthread_join(threads[1], NULL);

    // the spill met its take where the give's cell was trimmed past before its walk, and nowhere else
    assert((s_spilled == NULL) || (s_spilled == s_temporary[1]));

    if (s_spilled != NULL)
    {
        mmgr_persistent_buf_release(GENMC_BLOCK, s_spilled, s_writer);
    }
    mmgr_persistent_buf_release(GENMC_BLOCK, s_temporary[0], s_writer);
    for (unsigned int at = 0u; at < s_persistent_count; at += 1u)
    {
        mmgr_persistent_buf_release(GENMC_BLOCK, s_persistent[s_persistent_count - 1u - at], s_writer);
    }
    // the gap, read first, runs any trim a give left owed on the open word
    (void)mmgr_buf_available(GENMC_BLOCK);
    assert(mmgr_series_cellularum_temporary_top(GENMC_BLOCK) == GENMC_BYTES);
    assert(mmgr_series_cellularum_persistent_end(GENMC_BLOCK) == 0u);
    assert(mmgr_owner_in_use(s_writer) == 0u);
    assert((embed_atomic_word_load(&GENMC_BLOCK->closed) & (CARCER_CLOSED | CARCER_TRIM_PENDING)) == 0u);
    return 0;
}
