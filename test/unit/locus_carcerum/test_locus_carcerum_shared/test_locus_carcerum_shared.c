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
static embed_atomic_usize s_registered = EMBED_ATOMIC_INIT(0u);
static CellOwner s_crowd = MMGR_CELL_OWNER("crowd");
static CellOwner s_scratch = MMGR_CELL_OWNER("scratch");
static CellOwner s_contested = MMGR_CELL_OWNER("contested");
static CellOwner s_own[MMGR_HOST_THREADS_MOST];
static char s_own_names[MMGR_HOST_THREADS_MOST][8];

// An owner registered by the first case that names it, its counts put back to zero for every case:
// the owner queue lives as long as the program does
static void owner_ready(CellOwner *owner)
{
    if (embed_atomic_usize_load(&owner->place) == 0u)
    {
        TEST_ASSERT_TRUE(mmgr_owner_register(owner));
    }
    embed_atomic_usize_store(&owner->in_use, 0u);
    embed_atomic_usize_store(&owner->hw, 0u);
}

void setUp(void)
{
    mmgr_cellblock_emptied(&crowded_shared_ctx);
    embed_atomic_u32_store(&s_next_index, 0u);
    embed_atomic_usize_store(&s_refused, 0u);
    embed_atomic_usize_store(&s_overwritten, 0u);
    embed_atomic_usize_store(&s_kept, 0u);
    embed_atomic_usize_store(&s_registered, 0u);
    owner_ready(&s_crowd);
    owner_ready(&s_scratch);
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

// Each thread takes temporary cells for owner, fills and reads them back as the persistent case does,
// and where giving is set gives each back one at a time through the release that reads the tier off
// the address; otherwise the cells stay for the rewind
static void contend_temporary_with(CellOwner *owner, embed_bool giving)
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
            held[at] = (uint8_t *)crowded.shared.temporary_buf_alloc(wanted[at], owner, 1000u);
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
    contend_temporary_with(&s_scratch, EMBED_TRUE);
}

// Every thread registers an owner of its own and tries the one contested owner, all at once
static void contend_registration(void)
{
    const unsigned int index = contend_index_taken();

    (void)embed_atomic_usize_fetch_add(&s_registered, mmgr_owner_register(&s_contested) ? 1u : 0u);
    (void)embed_atomic_usize_fetch_add(&s_refused, mmgr_owner_register(&s_own[index]) ? 0u : 1u);
}

static void contend_temporary_shared_owner(void)
{
    contend_temporary_with(&s_crowd, EMBED_TRUE);
}

static void contend_temporary_kept(void)
{
    contend_temporary_with(&s_crowd, EMBED_FALSE);
}

void test_every_entry_leaves_the_gap_equal_to_what_the_tiers_leave(void)
{
    void *const prisoner = crowded.shared.persistent_buf_alloc(16u, &s_crowd);

    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    crowded.shared.persistent_buf_release(prisoner, &s_crowd);
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    const size_t mark = crowded.shared.temporary_buf_mark();
    void *const scratch = crowded.shared.temporary_buf_alloc(16u, &s_scratch, 1000u);
    TEST_ASSERT_NOT_NULL(scratch);
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    crowded.shared.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES - tiers_held(), crowded.shared.buf_available());
    (void)crowded.shared.temporary_buf_alloc(16u, &s_scratch, 1000u);
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

void test_owners_registering_at_once_are_each_queued_once(void)
{
    const size_t before = mmgr_owner_count();

    for (size_t at = 0u; at < MMGR_HOST_THREADS_MOST; at++)
    {
        s_own_names[at][0] = 'o';
        s_own_names[at][1] = 'w';
        s_own_names[at][2] = 'n';
        s_own_names[at][3] = (char)('0' + ((at / 10u) % 10u));
        s_own_names[at][4] = (char)('0' + (at % 10u));
        s_own_names[at][5] = '\0';
        s_own[at] = (CellOwner)MMGR_CELL_OWNER(s_own_names[at]);
    }
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_registration, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, (size_t)embed_atomic_usize_load(&s_registered),
                                     "one owner tried by every thread at once registers once");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_refused), "each thread's own owner registers");
    TEST_ASSERT_EQUAL_size_t(before + 1u + MMGR_HOST_THREADS_MOST, mmgr_owner_count());
    for (size_t at = 0u; at < mmgr_owner_count(); at++)
    {
        TEST_ASSERT_EQUAL_size_t_MESSAGE(at + 1u, (size_t)embed_atomic_usize_load(&mmgr_owner_at(at)->place),
                                         "each owner's place is where the queue holds it");
    }
}

void test_an_owner_for_each_thread_is_counted_back_to_nothing(void)
{
    for (size_t at = 0u; at < MMGR_HOST_THREADS_MOST; at++)
    {
        owner_ready(&s_own[at]);
    }
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_own_owner, MMGR_HOST_THREADS_MOST));
    // a thread whose every take the gap refused has a peak of 0, and the tier promises it no take; the
    // threads together took cells
    size_t peaks = 0u;
    for (size_t at = 0u; at < MMGR_HOST_THREADS_MOST; at++)
    {
        TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&s_own[at].in_use));
        peaks += (size_t)embed_atomic_usize_load(&s_own[at].hw);
    }
    TEST_ASSERT_TRUE(peaks > 0u);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&s_overwritten));
}

void test_temporary_cells_given_back_one_at_a_time_count_their_owner_back_to_nothing(void)
{
    // every temporary take counts to the owner it names, and the single give counts the cell off the
    // owner its footer names whatever owner the give is handed
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_temporary_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_overwritten), "a temporary cell was handed to two threads at once");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_crowd.in_use), "a temporary take or give was counted twice or not at all");
    TEST_ASSERT_TRUE(embed_atomic_usize_load(&s_crowd.hw) > 0u);
    crowded.shared.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_crowd.in_use), "the reset found a cell still counted");
}

void test_a_reset_counts_every_temporary_cell_off_its_owner(void)
{
    // the threads keep every cell they are handed, and the reset alone counts them off
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_temporary_kept, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_overwritten), "a temporary cell was handed to two threads at once");
    TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)embed_atomic_usize_load(&s_kept), (size_t)embed_atomic_usize_load(&s_crowd.in_use),
                                     "the owner counts every payload the tier holds");
    crowded.shared.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_crowd.in_use), "the reset left a cell counted");
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES, crowded.shared.buf_available());
}
