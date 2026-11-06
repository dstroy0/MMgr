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
static embed_atomic_usize s_kept = EMBED_ATOMIC_INIT(0u);
// The owners every case takes for, declared with the cellblocks as the program's table
#define CROWDED_OWNERS(owner_) owner_(s_crowd, "crowd") owner_(s_scratch, "scratch") owner_(own_0, "own00") owner_(own_1, "own01") owner_(own_2, "own02") owner_(own_3, "own03") owner_(own_4, "own04") owner_(own_5, "own05") owner_(own_6, "own06") owner_(own_7, "own07") owner_(own_8, "own08") owner_(own_9, "own09") owner_(own_10, "own10") owner_(own_11, "own11") owner_(own_12, "own12") owner_(own_13, "own13") owner_(own_14, "own14") owner_(own_15, "own15")
MMGR_OWNERS(CROWDED_OWNERS)

// the threads' own owners, one each, as the table holds them
EMBED_STATIC_ASSERT(MMGR_HOST_THREADS_MOST == 16u, "the table declares one owner for each of sixteen threads");
static const CellOwner *const s_own[MMGR_HOST_THREADS_MOST] = {
    &mmgr_owner_table[mmgr_owner_place_own_0],
    &mmgr_owner_table[mmgr_owner_place_own_1],
    &mmgr_owner_table[mmgr_owner_place_own_2],
    &mmgr_owner_table[mmgr_owner_place_own_3],
    &mmgr_owner_table[mmgr_owner_place_own_4],
    &mmgr_owner_table[mmgr_owner_place_own_5],
    &mmgr_owner_table[mmgr_owner_place_own_6],
    &mmgr_owner_table[mmgr_owner_place_own_7],
    &mmgr_owner_table[mmgr_owner_place_own_8],
    &mmgr_owner_table[mmgr_owner_place_own_9],
    &mmgr_owner_table[mmgr_owner_place_own_10],
    &mmgr_owner_table[mmgr_owner_place_own_11],
    &mmgr_owner_table[mmgr_owner_place_own_12],
    &mmgr_owner_table[mmgr_owner_place_own_13],
    &mmgr_owner_table[mmgr_owner_place_own_14],
    &mmgr_owner_table[mmgr_owner_place_own_15],
};

// An owner's counts put back to nothing for the case
static void owner_ready(const CellOwner *owner)
{
    mmgr_owner_bytes_given(owner, mmgr_owner_in_use(owner));
}

void setUp(void)
{
    mmgr_series_cellularum_emptied(&crowded_shared_ctx);
    embed_atomic_u32_store(&s_next_index, 0u);
    embed_atomic_usize_store(&s_refused, 0u);
    embed_atomic_usize_store(&s_overwritten, 0u);
    embed_atomic_usize_store(&s_kept, 0u);
    owner_ready(s_crowd);
    owner_ready(s_scratch);
}

void tearDown(void)
{
}

/** @brief The bytes both tiers hold, read off the two boundaries. */
static size_t tiers_held(void)
{
    return mmgr_series_cellularum_persistent_end(&crowded_shared_ctx) +
           (SHARED_BYTES - mmgr_series_cellularum_temporary_top(&crowded_shared_ctx));
}

static unsigned int contend_index_taken(void)
{
    return embed_atomic_u32_fetch_add(&s_next_index, 1u);
}

// Each thread fills every persistent cell it holds with its own mark and reads the mark back before
// giving the cell up
static void contend_with(const CellOwner *owner, unsigned int index)
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
    contend_with(s_crowd, contend_index_taken());
}

static void contend_own_owner(void)
{
    const unsigned int index = contend_index_taken();
    contend_with(s_own[index], index);
}

// Each thread takes temporary cells for owner, fills and reads them back as the persistent case does,
// and where giving is set gives each back one at a time through the release that reads the tier off
// the address; otherwise the cells stay for the rewind
static void contend_temporary_with(const CellOwner *owner, embed_bool giving)
{
    const unsigned int index = contend_index_taken();
    const uint8_t mark = (uint8_t)(index + 1u);
    size_t refused = 0u;
    size_t overwritten = 0u;
    size_t kept = 0u;
    for (size_t round = 0u; round < CONTEND_ROUNDS; round++)
    {
        uint8_t *held[CONTEND_HELD];
        size_t wanted[CONTEND_HELD];
        for (size_t at = 0u; at < CONTEND_HELD; at++)
        {
            wanted[at] = 8u + (8u * ((round + at + index) % 6u));
            held[at] = (uint8_t *)crowded.shared.temporary_buf_alloc(wanted[at], owner);
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
            if (giving)
            {
                crowded.shared.persistent_buf_release(held[at], owner);
            }
            else
            {
                kept += (held[at] != NULL) ? mmgr_align_up_buf(wanted[at]) : 0u;
            }
        }
    }
    (void)embed_atomic_usize_fetch_add(&s_refused, refused);
    (void)embed_atomic_usize_fetch_add(&s_overwritten, overwritten);
    (void)embed_atomic_usize_fetch_add(&s_kept, kept);
}

static void contend_temporary(void)
{
    contend_temporary_with(s_scratch, EMBED_TRUE);
}

static void contend_temporary_shared_owner(void)
{
    contend_temporary_with(s_crowd, EMBED_TRUE);
}

static void contend_temporary_kept(void)
{
    contend_temporary_with(s_crowd, EMBED_FALSE);
}

void test_every_entry_leaves_the_gap_equal_to_what_the_tiers_leave(void)
{
    void *const prisoner = crowded.shared.persistent_buf_alloc(16u, s_crowd);

    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    crowded.shared.persistent_buf_release(prisoner, s_crowd);
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    const size_t mark = crowded.shared.temporary_buf_mark();
    void *const scratch = crowded.shared.temporary_buf_alloc(16u, s_scratch);
    TEST_ASSERT_NOT_NULL(scratch);
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    crowded.shared.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    (void)crowded.shared.temporary_buf_alloc(16u, s_scratch);
    crowded.shared.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, tiers_held(), "nothing held, nothing in either tier");
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES, crowded.shared.buf_available());
}

void test_threads_taking_and_giving_at_once_never_share_a_cell(void)
{
    // a take refused while sixteen threads hold cells is the pool's size, and the case holds what the
    // tier promises: no cell handed to two
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_overwritten),
                                     "a cell was handed to two threads at once");
}

void test_threads_taking_and_giving_at_once_leave_the_series_cellularum_whole(void)
{
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_series_cellularum_persistent_end(&crowded_shared_ctx),
                                     "every cell given back returns the tier to the gap");
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES, crowded.shared.buf_available());
}

void test_threads_taking_and_giving_temporary_cells_at_once_never_share_a_cell(void)
{
    // a take refused once the gap is spent is the pool's size, and the case holds what the tier promises:
    // no cell handed to two, and the tier whole once it is reset
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_temporary, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_overwritten),
                                     "a temporary cell was handed to two threads at once");
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_series_cellularum_persistent_end(&crowded_shared_ctx));
    crowded.shared.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(SHARED_BYTES, mmgr_series_cellularum_temporary_top(&crowded_shared_ctx),
                                     "the reset gives the whole tier back");
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES, crowded.shared.buf_available());
}

void test_an_owner_shared_by_every_thread_is_counted_back_to_nothing(void)
{
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_owner_in_use(s_crowd),
                                     "a take or give was counted twice or not at all");
    TEST_ASSERT_TRUE(mmgr_owner_hw(s_crowd) > 0u);
    TEST_ASSERT_TRUE(mmgr_owner_hw(s_crowd) <= SHARED_BYTES);
}

void test_an_owner_for_each_thread_is_counted_back_to_nothing(void)
{
    for (size_t at = 0u; at < MMGR_HOST_THREADS_MOST; at++)
    {
        owner_ready(s_own[at]);
    }
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_own_owner, MMGR_HOST_THREADS_MOST));
    // a thread whose every take the gap refused has a peak of 0, and the tier promises it no take; the
    // threads together took cells
    size_t peaks = 0u;
    for (size_t at = 0u; at < MMGR_HOST_THREADS_MOST; at++)
    {
        TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(s_own[at]));
        peaks += mmgr_owner_hw(s_own[at]);
    }
    TEST_ASSERT_TRUE(peaks > 0u);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&s_overwritten));
}

void test_temporary_cells_given_back_one_at_a_time_count_their_owner_back_to_nothing(void)
{
    // every temporary take counts to the owner it names, and the single give counts the cell off the
    // owner its footer names whatever owner the give is handed
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_temporary_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_overwritten),
                                     "a temporary cell was handed to two threads at once");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_owner_in_use(s_crowd),
                                     "a temporary take or give was counted twice or not at all");
    TEST_ASSERT_TRUE(mmgr_owner_hw(s_crowd) > 0u);
    crowded.shared.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_owner_in_use(s_crowd),
                                     "the reset found a cell still counted");
}

void test_a_reset_counts_every_temporary_cell_off_its_owner(void)
{
    // the threads keep every cell they are handed, and the reset alone counts them off
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_temporary_kept, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_overwritten),
                                     "a temporary cell was handed to two threads at once");
    TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)embed_atomic_usize_load(&s_kept),
                                     mmgr_owner_in_use(s_crowd),
                                     "the owner counts every payload the tier holds");
    crowded.shared.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_owner_in_use(s_crowd),
                                     "the reset left a cell counted");
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES, crowded.shared.buf_available());
}
