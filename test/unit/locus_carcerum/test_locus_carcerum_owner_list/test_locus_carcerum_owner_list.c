#include "locus_carcerum/locus_carcerum.h"

#include "unity.h"

// Room for one more 8 byte cell than the list has places, each cell's header and payload within 32
#define LISTED_BYTES ((MMGR_CARCER_OWNERS + 1u) * 32u)
#define OTHER_BYTES 256u

ParsMemoriaeInternae(listed, LISTED_BYTES);
ParsMemoriaeInternae(other, OTHER_BYTES);

LocusCarcerum(roll, MMGR_MINIMUM_SECURITY(listed), MMGR_MINIMUM_SECURITY(other));

void setUp(void)
{
    mmgr_cellblock_emptied(&roll_listed_ctx);
    mmgr_cellblock_emptied(&roll_other_ctx);
}

void tearDown(void)
{
}

void test_a_fresh_cellblock_lists_no_owner(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellblock_owner_count(&roll_listed_ctx));
}

void test_an_owner_is_listed_by_the_cellblock_it_takes_from(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");

    (void)roll.listed.persistent_buf_alloc(16u, &owner);
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_cellblock_owner_count(&roll_listed_ctx));
    TEST_ASSERT_EQUAL_PTR(&owner, mmgr_cellblock_owner(&roll_listed_ctx, 0));
    TEST_ASSERT_EQUAL_PTR(&roll_listed_ctx, (const void *)(uintptr_t)embed_atomic_uptr_load(&owner.listed_in));
}

void test_an_owner_is_listed_once_over_many_takes(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");

    for (size_t take = 0u; take < 4u; take++)
    {
        (void)roll.listed.persistent_buf_alloc(16u, &owner);
    }
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_cellblock_owner_count(&roll_listed_ctx));
}

void test_an_owner_stays_listed_after_giving_everything_back(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");
    void *const prisoner = roll.listed.persistent_buf_alloc(16u, &owner);

    roll.listed.persistent_buf_release(prisoner, &owner);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.in_use));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, mmgr_cellblock_owner_count(&roll_listed_ctx), "the list records every owner that took");
    TEST_ASSERT_EQUAL_PTR(&owner, mmgr_cellblock_owner(&roll_listed_ctx, 0));
}

void test_owners_are_listed_in_the_order_of_their_first_take(void)
{
    CellOwner reader = MMGR_CELL_OWNER("reader");
    CellOwner writer = MMGR_CELL_OWNER("writer");

    (void)roll.listed.persistent_buf_alloc(16u, &writer);
    (void)roll.listed.persistent_buf_alloc(16u, &reader);
    (void)roll.listed.persistent_buf_alloc(16u, &writer);
    TEST_ASSERT_EQUAL_size_t(2u, mmgr_cellblock_owner_count(&roll_listed_ctx));
    TEST_ASSERT_EQUAL_PTR(&writer, mmgr_cellblock_owner(&roll_listed_ctx, 0));
    TEST_ASSERT_EQUAL_PTR(&reader, mmgr_cellblock_owner(&roll_listed_ctx, 1));
}

void test_an_owner_taking_from_two_cellblocks_is_listed_once_in_each(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");

    (void)roll.listed.persistent_buf_alloc(16u, &owner);
    (void)roll.other.persistent_buf_alloc(16u, &owner);
    TEST_ASSERT_EQUAL_PTR(&roll_other_ctx, (const void *)(uintptr_t)embed_atomic_uptr_load(&owner.listed_in));
    (void)roll.listed.persistent_buf_alloc(16u, &owner);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, mmgr_cellblock_owner_count(&roll_listed_ctx),
                                     "a return to the first cellblock adds no second entry");
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_cellblock_owner_count(&roll_other_ctx));
    TEST_ASSERT_EQUAL_PTR(&roll_listed_ctx, (const void *)(uintptr_t)embed_atomic_uptr_load(&owner.listed_in));
}

void test_a_take_naming_no_owner_lists_nothing(void)
{
    (void)roll.listed.persistent_buf_alloc(16u, NULL);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellblock_owner_count(&roll_listed_ctx));
}

void test_a_refused_take_lists_nothing(void)
{
    CellOwner owner = MMGR_CELL_OWNER("reader");

    TEST_ASSERT_NULL(roll.listed.persistent_buf_alloc(LISTED_BYTES, &owner));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellblock_owner_count(&roll_listed_ctx));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_uptr_load(&owner.listed_in));
}

void test_the_list_stops_at_its_room_and_the_owner_past_it_is_still_counted(void)
{
    static CellOwner crowd[MMGR_CARCER_OWNERS + 1u];

    for (size_t at = 0u; at <= MMGR_CARCER_OWNERS; at++)
    {
        crowd[at] = (CellOwner)MMGR_CELL_OWNER("crowd");
        TEST_ASSERT_NOT_NULL(roll.listed.persistent_buf_alloc(8u, &crowd[at]));
    }
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_OWNERS, mmgr_cellblock_owner_count(&roll_listed_ctx));
    TEST_ASSERT_EQUAL_PTR(&crowd[MMGR_CARCER_OWNERS - 1u], mmgr_cellblock_owner(&roll_listed_ctx, MMGR_CARCER_OWNERS - 1u));

    CellOwner *const past = &crowd[MMGR_CARCER_OWNERS];

    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, (size_t)embed_atomic_usize_load(&past->in_use), "an owner past the room keeps its own counts");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_uptr_load(&past->listed_in), "an owner no list carries names no cellblock as its list");
}
