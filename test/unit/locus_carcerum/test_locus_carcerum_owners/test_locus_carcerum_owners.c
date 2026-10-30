#include "locus_carcerum/locus_carcerum.h"

#include "unity.h"

#define PLAIN_BYTES 1024u
#define WIPED_BYTES 1024u

ParsMemoriaeInternae(plain, PLAIN_BYTES);
ParsMemoriaeInternae(wiped, WIPED_BYTES);

LocusCarcerum(counted, MMGR_MINIMUM_SECURITY(plain), MMGR_MAXIMUM_SECURITY(wiped));

void setUp(void)
{
    mmgr_cellblock_emptied(&counted_plain_ctx);
    mmgr_cellblock_emptied(&counted_wiped_ctx);
}

void tearDown(void)
{
}

void test_a_new_owner_has_nothing_counted(void)
{
    const CellOwner owner = MMGR_CELL_OWNER("reader");

    TEST_ASSERT_EQUAL_STRING("reader", owner.name);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.in_use));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.hw));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_uptr_load(&owner.listed_in));
}

void test_a_take_counts_the_cell_to_its_owner(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");

    TEST_ASSERT_NOT_NULL(counted.plain.persistent_buf_alloc(33u, &owner));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mmgr_align_up_buf(33u), (size_t)embed_atomic_usize_load(&owner.in_use), "the cell's payload is what is counted");
    TEST_ASSERT_EQUAL_size_t((size_t)embed_atomic_usize_load(&owner.in_use), (size_t)embed_atomic_usize_load(&owner.hw));
}

void test_a_give_takes_the_same_bytes_off(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");
    void *const prisoner = counted.plain.persistent_buf_alloc(33u, &owner);

    counted.plain.persistent_buf_release(prisoner, &owner);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&owner.in_use), "a matched take and give leave nothing counted");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mmgr_align_up_buf(33u), (size_t)embed_atomic_usize_load(&owner.hw), "the give does not lower the peak");
}

void test_the_peak_is_the_most_held_at_once(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");
    void *const first = counted.plain.persistent_buf_alloc(64u, &owner);
    void *const second = counted.plain.persistent_buf_alloc(32u, &owner);

    counted.plain.persistent_buf_release(first, &owner);
    void *const third = counted.plain.persistent_buf_alloc(16u, &owner);

    TEST_ASSERT_EQUAL_size_t(48u, (size_t)embed_atomic_usize_load(&owner.in_use));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(96u, (size_t)embed_atomic_usize_load(&owner.hw), "the peak is the two first cells together");
    counted.plain.persistent_buf_release(second, &owner);
    counted.plain.persistent_buf_release(third, &owner);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.in_use));
    TEST_ASSERT_EQUAL_size_t(96u, (size_t)embed_atomic_usize_load(&owner.hw));
}

void test_a_cell_given_back_below_the_end_counts_off_at_once(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");
    void *const wide = counted.plain.persistent_buf_alloc(64u, &owner);
    void *const pin = counted.plain.persistent_buf_alloc(8u, &owner);

    // the wide cell waits in the tier under the pin, and its bytes come off the owner the moment it is
    // given back, not when the trim reaches it
    counted.plain.persistent_buf_release(wide, &owner);
    TEST_ASSERT_EQUAL_size_t(8u, (size_t)embed_atomic_usize_load(&owner.in_use));
    counted.plain.persistent_buf_release(pin, &owner);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.in_use));

    // both cells are the gap's again, and the next take lands where the wide one stood
    void *const again = counted.plain.persistent_buf_alloc(48u, &owner);

    TEST_ASSERT_EQUAL_PTR(wide, again);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(48u, (size_t)embed_atomic_usize_load(&owner.in_use), "a fresh cell counts the bytes asked, rounded");
    counted.plain.persistent_buf_release(again, &owner);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.in_use));
}

void test_a_refused_take_counts_nothing(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");

    TEST_ASSERT_NULL(counted.plain.persistent_buf_alloc(PLAIN_BYTES, &owner));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.in_use));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.hw));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_uptr_load(&owner.listed_in));
}

void test_a_null_owner_counts_nothing(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");
    void *const counted_cell = counted.plain.persistent_buf_alloc(32u, &owner);
    void *const uncounted_cell = counted.plain.persistent_buf_alloc(32u, NULL);

    TEST_ASSERT_NOT_NULL(uncounted_cell);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(32u, (size_t)embed_atomic_usize_load(&owner.in_use), "a take naming no owner adds to none");
    counted.plain.persistent_buf_release(counted_cell, NULL);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(32u, (size_t)embed_atomic_usize_load(&owner.in_use), "a give naming no owner takes off none");
    counted.plain.persistent_buf_release(uncounted_cell, NULL);
}

void test_two_owners_are_counted_apart(void)
{
    CellOwner reader = MMGR_CELL_OWNER("reader");
    CellOwner writer = MMGR_CELL_OWNER("writer");
    void *const read_cell = counted.plain.persistent_buf_alloc(16u, &reader);
    void *const write_cell = counted.plain.persistent_buf_alloc(40u, &writer);

    TEST_ASSERT_EQUAL_size_t(16u, (size_t)embed_atomic_usize_load(&reader.in_use));
    TEST_ASSERT_EQUAL_size_t(40u, (size_t)embed_atomic_usize_load(&writer.in_use));
    counted.plain.persistent_buf_release(write_cell, &writer);
    TEST_ASSERT_EQUAL_size_t(16u, (size_t)embed_atomic_usize_load(&reader.in_use));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&writer.in_use));
    counted.plain.persistent_buf_release(read_cell, &reader);
}

void test_a_maximum_security_give_counts_like_a_plain_one(void)
{
    CellOwner owner = MMGR_CELL_OWNER("keys");
    void *const prisoner = counted.wiped.persistent_buf_alloc(24u, &owner);

    TEST_ASSERT_EQUAL_size_t(24u, (size_t)embed_atomic_usize_load(&owner.in_use));
    counted.wiped.persistent_buf_release(prisoner, &owner);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.in_use));
    TEST_ASSERT_EQUAL_size_t(24u, (size_t)embed_atomic_usize_load(&owner.hw));
}

void test_one_owner_counts_across_cellblocks(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");
    void *const plain_cell = counted.plain.persistent_buf_alloc(16u, &owner);
    void *const wiped_cell = counted.wiped.persistent_buf_alloc(16u, &owner);

    TEST_ASSERT_EQUAL_size_t(32u, (size_t)embed_atomic_usize_load(&owner.in_use));
    counted.plain.persistent_buf_release(plain_cell, &owner);
    counted.wiped.persistent_buf_release(wiped_cell, &owner);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.in_use));
    TEST_ASSERT_EQUAL_size_t(32u, (size_t)embed_atomic_usize_load(&owner.hw));
}
