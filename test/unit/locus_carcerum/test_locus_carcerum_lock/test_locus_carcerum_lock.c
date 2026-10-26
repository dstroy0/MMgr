#include "locus_carcerum/locus_carcerum.h"

#include "embed_spinlock.h"
#include "host_threads.h"
#include "unity.h"

#define SHARED_BYTES 65536u
#define CONTEND_ROUNDS 2000u
#define CONTEND_HELD 4u

ParsMemoriaeInternae(shared, SHARED_BYTES);

LocusCarcerum(crowded, MMGR_MINIMUM_SECURITY(shared));

// The suite's own counts, written by every thread under a lock of their own
static embed_spinlock s_count_lock = EMBED_SPINLOCK_INIT;
static unsigned int s_next_index;
static size_t s_refused;
static size_t s_overwritten;
static CellOwner s_crowd = MMGR_CELL_OWNER("crowd");
static CellOwner s_own[MMGR_HOST_THREADS_MOST];

void setUp(void)
{
    crowded_shared_ctx.persistent_end = 0u;
    s_next_index = 0u;
    s_refused = 0u;
    s_overwritten = 0u;
    s_crowd = (CellOwner)MMGR_CELL_OWNER("crowd");
}

void tearDown(void)
{
}

static unsigned int contend_index_taken(void)
{
    EMBED_SPINLOCK_ACQUIRE(&s_count_lock);
    const unsigned int index = s_next_index;
    s_next_index += 1u;
    EMBED_SPINLOCK_RELEASE(&s_count_lock);
    return index;
}

static void contend_counts_added(size_t refused, size_t overwritten)
{
    EMBED_SPINLOCK_ACQUIRE(&s_count_lock);
    s_refused += refused;
    s_overwritten += overwritten;
    EMBED_SPINLOCK_RELEASE(&s_count_lock);
}

// Each thread fills every cell it holds with its own mark and reads the mark back before giving the
// cell up. A cell handed to two threads at once carries the other thread's mark on that read
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
    contend_counts_added(refused, overwritten);
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

void test_every_entry_frees_the_lock_it_took(void)
{
    void *const prisoner = crowded.shared.persistent_buf_alloc(16u, NULL);

    TEST_ASSERT_EQUAL_UINT32(0u, embed_spinlock_read(&crowded_shared_lock));
    crowded.shared.persistent_buf_release(prisoner, NULL);
    TEST_ASSERT_EQUAL_UINT32(0u, embed_spinlock_read(&crowded_shared_lock));
    const size_t mark = crowded.shared.temporary_buf_mark();
    TEST_ASSERT_EQUAL_UINT32(0u, embed_spinlock_read(&crowded_shared_lock));
    (void)crowded.shared.temporary_buf_alloc(16u);
    TEST_ASSERT_EQUAL_UINT32(0u, embed_spinlock_read(&crowded_shared_lock));
    crowded.shared.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_UINT32(0u, embed_spinlock_read(&crowded_shared_lock));
    crowded.shared.temporary_buf_reset();
    TEST_ASSERT_EQUAL_UINT32(0u, embed_spinlock_read(&crowded_shared_lock));
    (void)crowded.shared.buf_available();
    TEST_ASSERT_EQUAL_UINT32(0u, embed_spinlock_read(&crowded_shared_lock));
}

void test_threads_taking_and_giving_at_once_never_share_a_cell(void)
{
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, s_overwritten, "a cell was handed to two threads at once");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, s_refused, "a take was refused with the cellblock far from full");
}

void test_threads_taking_and_giving_at_once_leave_the_cellblock_whole(void)
{
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, crowded_shared_ctx.persistent_end,
                                     "every cell given back trims the tier away");
    TEST_ASSERT_EQUAL_size_t(SHARED_BYTES, crowded.shared.buf_available());
}

void test_an_owner_shared_by_every_thread_is_counted_back_to_nothing(void)
{
    TEST_ASSERT_TRUE(mmgr_host_threads_run(contend_shared_owner, MMGR_HOST_THREADS_MOST));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, s_crowd.in_use, "a take or give was counted twice or not at all");
    TEST_ASSERT_TRUE(s_crowd.hw > 0u);
    TEST_ASSERT_TRUE(s_crowd.hw <= SHARED_BYTES);
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
        TEST_ASSERT_EQUAL_size_t(0u, s_own[at].in_use);
        TEST_ASSERT_TRUE(s_own[at].hw > 0u);
    }
    TEST_ASSERT_EQUAL_size_t(0u, s_overwritten);
}
