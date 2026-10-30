/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_locus_carcerum_shared.c
 * @brief One cellblock reached from many threads at once: no cell handed to two, every count exact,
 *        and the cellblock whole once every thread has given back what it took.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note Each thread fills every cell it holds with its own mark and reads the mark back before giving
 *       the cell up. A cell handed to two threads at once carries the other thread's mark on that
 *       read, and the count of such reads is what the cases hold at zero.
 * @note The suite's own counts are atomic words too, added to by every thread and read once they end.
 */
#include "locus_carcerum/locus_carcerum.h"

#include "embed_atomic.h"
#include "host_threads.h"
#include "unity.h"

#define SHARED_BYTES 65536u
#define CONTEND_ROUNDS 2000u
#define CONTEND_HELD 4u

ParsMemoriaeInternae(shared, SHARED_BYTES);
LocusCarcerum(crowded, MMGR_MINIMUM_SECURITY(shared));

static embed_atomic_u32 s_next_index = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_refused = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_overwritten = EMBED_ATOMIC_INIT(0u);
static CellOwner s_crowd = MMGR_CELL_OWNER("crowd");
static CellOwner s_own[MMGR_HOST_THREADS_MOST];

void setUp(void)
{
    mmgr_cellblock_emptied(&crowded_shared_ctx);
    embed_atomic_u32_store(&s_next_index, 0u);
    embed_atomic_usize_store(&s_refused, 0u);
    embed_atomic_usize_store(&s_overwritten, 0u);
    s_crowd = (CellOwner)MMGR_CELL_OWNER("crowd");
}

void tearDown(void)
{
}

/** @brief The bytes both tiers hold, read off the word that holds both boundaries. */
static size_t tiers_held(void)
{
    const embed_atomic_usize_value tiers = embed_atomic_usize_load(&crowded_shared_ctx.tiers);

    return mmgr_tiers_persistent_end(tiers) + (SHARED_BYTES - mmgr_tiers_temporary_top(tiers));
}

static unsigned int contend_index_taken(void)
{
    return embed_atomic_u32_fetch_add(&s_next_index, 1u);
}

// Each thread fills every persistent cell it holds with its own mark and reads the mark back before
// giving the cell up
static void contend_with(CellOwner *owner, unsigned int index)
{
    const uint8_t mark = (uint8_t)(index + 1u);
    size_t refused = 0u;
    size_t overwritten = 0u;
    for (size_t round = 0u; round < CONTEND_ROUNDS; round++)
    {
        uint8_t *held[CONTEND_HELD];
        size_t wanted[CONTEND_HELD];
        for (size_t at = 0u; at < CONTEND_HELD; at++)
        {
            wanted[at] = 8u + (8u * ((round + at + index) % 6u));
            held[at] = (uint8_t *)crowded.shared.persistent_buf_alloc(wanted[at], owner);
            refused += (held[at] == NULL) ? 1u : 0u;
            for (size_t byte = 0u; (held[at] != NULL) && (byte < wanted[at]); byte++)
            {
                held[at][byte] = mark;
            }
        }
        for (size_t at = 0u; at < CONTEND_HELD; at++)
        {
            for (size_t byte = 0u; (held[at] != NULL) && (byte < wanted[at]); byte++)
            {
                overwritten += (held[at][byte] != mark) ? 1u : 0u;
            }
            crowded.shared.persistent_buf_release(held[at], owner);
        }
    }
    (void)embed_atomic_usize_fetch_add(&s_refused, refused);
    (void)embed_atomic_usize_fetch_add(&s_overwritten, overwritten);
}

static void contend_shared_owner(void)
{
    contend_with(&s_crowd, contend_index_taken());
}

static void contend_own_owner(void)
{
    const unsigned int index = contend_index_taken();
    contend_with(&s_own[index], index);
}

// Each thread takes temporary cells, fills and reads them back as the persistent case does, and gives
// each back one at a time through the release that reads the tier off the address
static void contend_temporary(void)
{
    const unsigned int index = contend_index_taken();
    const uint8_t mark = (uint8_t)(index + 1u);
    size_t refused = 0u;
    size_t overwritten = 0u;
    for (size_t round = 0u; round < CONTEND_ROUNDS; round++)
    {
        uint8_t *held[CONTEND_HELD];
        size_t wanted[CONTEND_HELD];
        for (size_t at = 0u; at < CONTEND_HELD; at++)
        {
            wanted[at] = 8u + (8u * ((round + at + index) % 6u));
            held[at] = (uint8_t *)crowded.shared.temporary_buf_alloc(wanted[at]);
            refused += (held[at] == NULL) ? 1u : 0u;
            for (size_t byte = 0u; (held[at] != NULL) && (byte < wanted[at]); byte++)
            {
                held[at][byte] = mark;
            }
        }
        for (size_t at = 0u; at < CONTEND_HELD; at++)
        {
            for (size_t byte = 0u; (held[at] != NULL) && (byte < wanted[at]); byte++)
            {
                overwritten += (held[at][byte] != mark) ? 1u : 0u;
            }
            crowded.shared.persistent_buf_release(held[at], NULL);
        }
    }
    (void)embed_atomic_usize_fetch_add(&s_refused, refused);
    (void)embed_atomic_usize_fetch_add(&s_overwritten, overwritten);
}

void test_every_entry_leaves_the_gap_equal_to_what_the_tiers_leave(void)
{
    void *const prisoner = crowded.shared.persistent_buf_alloc(16u, NULL);

    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    crowded.shared.persistent_buf_release(prisoner, NULL);
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    const size_t mark = crowded.shared.temporary_buf_mark();
    void *const scratch = crowded.shared.temporary_buf_alloc(16u);
    TEST_ASSERT_NOT_NULL(scratch);
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    crowded.shared.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    (void)crowded.shared.temporary_buf_alloc(16u);
    crowded.shared.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, tiers_held(), "nothing held, nothing in either tier");
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES, crowded.shared.buf_available());
}

void test_threads_taking_and_giving_at_once_never_share_a_cell(void)
{
    // a cell given back below the end waits there until the cells above it are given back, and sixteen
    // threads giving back in their own order can fill the tier for a while; a take refused then is the
    // tier's shape, and the case holds what the tier promises: no cell handed to two
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_overwritten), "a cell was handed to two threads at once");
}

void test_threads_taking_and_giving_at_once_leave_the_cellblock_whole(void)
{
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_cellblock_persistent_end(&crowded_shared_ctx),
                                     "every cell given back trims the tier away");
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES, crowded.shared.buf_available());
}

void test_threads_taking_and_giving_temporary_cells_at_once_never_share_a_cell(void)
{
    // the temporary tier reuses nothing below its top until a rewind, and sixteen threads giving cells
    // back one at a time leave dead cells below cells still busy; a take refused once the gap is spent
    // is that tier's shape, and the case holds what the tier promises: no cell handed to two, and the
    // tier whole once it is reset
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_temporary, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_overwritten), "a temporary cell was handed to two threads at once");
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellblock_persistent_end(&crowded_shared_ctx));
    crowded.shared.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(SHARED_BYTES, mmgr_cellblock_temporary_top(&crowded_shared_ctx),
                                     "the reset gives the whole tier back");
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES, crowded.shared.buf_available());
}

void test_an_owner_shared_by_every_thread_is_counted_back_to_nothing(void)
{
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_crowd.in_use), "a take or give was counted twice or not at all");
    TEST_ASSERT_TRUE(embed_atomic_usize_load(&s_crowd.hw) > 0u);
    TEST_ASSERT_TRUE(embed_atomic_usize_load(&s_crowd.hw) <= SHARED_BYTES);
}

void test_an_owner_for_each_thread_is_counted_back_to_nothing(void)
{
    for (size_t at = 0u; at < MMGR_HOST_THREADS_MOST; at++)
    {
        s_own[at] = (CellOwner)MMGR_CELL_OWNER("own");
    }
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_own_owner, MMGR_HOST_THREADS_MOST));
    for (size_t at = 0u; at < MMGR_HOST_THREADS_MOST; at++)
    {
        TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&s_own[at].in_use));
        TEST_ASSERT_TRUE(embed_atomic_usize_load(&s_own[at].hw) > 0u);
    }
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&s_overwritten));
}
