#include "locus_carcerum/locus_carcerum.h"

#include "unity.h"

// The owner table as MMGR_OWNERS lays it: the owners declared stand in it in their order, each at its place from 1,
// and a record of the caller's own is none of the table's and takes nothing
#define ROLL_BYTES 256u

ParsMemoriaeInternae(listed, ROLL_BYTES);

LocusCarcerum(roll, MMGR_MINIMUM_SECURITY(listed));

#define ROLL_OWNERS(owner_) owner_(writer, "writer") owner_(reader, "reader")
MMGR_OWNERS(ROLL_OWNERS)

void setUp(void)
{
    mmgr_series_cellularum_emptied(&roll_listed_ctx);
}

void tearDown(void)
{
}

void test_the_table_counts_the_owners_declared(void)
{
    TEST_ASSERT_EQUAL_size_t(2u, mmgr_owner_table_count);
}

void test_each_owner_is_its_record_in_the_table_at_its_place(void)
{
    TEST_ASSERT_EQUAL_PTR(&mmgr_owner_table[0], writer);
    TEST_ASSERT_EQUAL_PTR(&mmgr_owner_table[1], reader);
    TEST_ASSERT_EQUAL_STRING("writer", writer->name);
    TEST_ASSERT_EQUAL_STRING("reader", reader->name);
    TEST_ASSERT_EQUAL_size_t(1u, writer->place);
    TEST_ASSERT_EQUAL_size_t(2u, reader->place);
    TEST_ASSERT_TRUE(mmgr_owner_listed(writer));
    TEST_ASSERT_TRUE(mmgr_owner_listed(reader));
}

void test_a_declared_owner_takes_and_the_cell_answers_it(void)
{
    void *const cell = roll.listed.persistent_buf_alloc(16u, writer);

    TEST_ASSERT_NOT_NULL(cell);
    TEST_ASSERT_EQUAL_PTR(writer, mmgr_owner_of_buf(&roll_listed_ctx, cell));
    roll.listed.persistent_buf_release(cell, writer);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(writer));
}

void test_a_record_of_the_callers_own_is_unlisted_and_takes_nothing(void)
{
    CellOwner stranger = {"writer", 0u, EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)};

    TEST_ASSERT_FALSE(mmgr_owner_listed(&stranger));
    TEST_ASSERT_FALSE(mmgr_owner_listed(NULL));
    TEST_ASSERT_NULL(roll.listed.persistent_buf_alloc(16u, &stranger));
    TEST_ASSERT_NULL(roll.listed.temporary_buf_alloc(16u, &stranger));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(&stranger));
    TEST_ASSERT_EQUAL_size_t(ROLL_BYTES, mmgr_buf_available(&roll_listed_ctx));
}

void test_an_address_between_two_records_is_none_of_the_tables(void)
{
    // Explicit casts step one byte into the first record: on no record's first byte, it is none of the table's
    const CellOwner *const between = (const CellOwner *)((const uint8_t *)&mmgr_owner_table[0] + 1u);

    TEST_ASSERT_FALSE(mmgr_owner_listed(between));
    TEST_ASSERT_FALSE(mmgr_owner_listed(&mmgr_owner_table[2]));
}
