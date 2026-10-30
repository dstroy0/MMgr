#include "locus_carcerum/locus_carcerum.h"

#include "unity.h"

// The owner queue lives as long as the program does and no case empties it: the cases run in the
// order they are written, and each reads the queue as the ones before it left it
#define ROLL_BYTES 256u

ParsMemoriaeInternae(listed, ROLL_BYTES);

LocusCarcerum(roll, MMGR_MINIMUM_SECURITY(listed));

static CellOwner writer = MMGR_CELL_OWNER("writer");
static CellOwner reader = MMGR_CELL_OWNER("reader");
static CellOwner second_writer = MMGR_CELL_OWNER("writer");
static CellOwner nameless = {NULL, EMBED_ATOMIC_INIT(0u), 0u, EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)};

// Owners enough to fill the queue's room, each named by its place
static CellOwner s_crowd[MMGR_CARCER_OWNERS];
static char s_crowd_names[MMGR_CARCER_OWNERS][8];

// The clock the library reads, which each case sets
static uint32_t s_now_ms;

uint32_t mmgr_carcer_hw_clock_ms(void)
{
    return s_now_ms;
}

void setUp(void)
{
    mmgr_cellblock_emptied(&roll_listed_ctx);
}

void tearDown(void)
{
}

static size_t place_of(const CellOwner *owner)
{
    return (size_t)embed_atomic_usize_load(&owner->place);
}

void test_no_owner_is_registered_before_the_first_registers(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_count());
    TEST_ASSERT_NULL(mmgr_owner_at(0u));
}

void test_an_owner_not_registered_takes_nothing(void)
{
    TEST_ASSERT_NULL(roll.listed.persistent_buf_alloc(16u, &writer));
    TEST_ASSERT_NULL(roll.listed.temporary_buf_alloc(16u, &writer, 10u));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&writer.in_use));
    TEST_ASSERT_EQUAL_size_t(ROLL_BYTES, mmgr_buf_available(&roll_listed_ctx));
}

void test_the_first_registration_is_the_first_place_at_the_time_it_lands(void)
{
    s_now_ms = 1234u;
    TEST_ASSERT_TRUE(mmgr_owner_register(&writer));
    TEST_ASSERT_EQUAL_size_t(1u, place_of(&writer));
    TEST_ASSERT_EQUAL_UINT32(1234u, writer.registered_ms);
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_owner_count());
    TEST_ASSERT_EQUAL_PTR(&writer, mmgr_owner_at(0u));
    TEST_ASSERT_NOT_NULL_MESSAGE(roll.listed.persistent_buf_alloc(16u, &writer), "a registered owner takes");
}

void test_owners_are_queued_in_the_order_they_register(void)
{
    s_now_ms = 1240u;
    TEST_ASSERT_TRUE(mmgr_owner_register(&reader));
    TEST_ASSERT_EQUAL_size_t(2u, place_of(&reader));
    TEST_ASSERT_EQUAL_UINT32(1240u, reader.registered_ms);
    TEST_ASSERT_EQUAL_size_t(2u, mmgr_owner_count());
    TEST_ASSERT_EQUAL_PTR(&writer, mmgr_owner_at(0u));
    TEST_ASSERT_EQUAL_PTR(&reader, mmgr_owner_at(1u));
}

void test_an_owner_registers_once(void)
{
    s_now_ms = 9999u;
    TEST_ASSERT_FALSE(mmgr_owner_register(&writer));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, place_of(&writer), "the key stands as the first registration gave it");
    TEST_ASSERT_EQUAL_UINT32(1234u, writer.registered_ms);
    TEST_ASSERT_EQUAL_size_t(2u, mmgr_owner_count());
}

void test_a_second_owner_of_a_registered_name_is_refused(void)
{
    TEST_ASSERT_FALSE(mmgr_owner_register(&second_writer));
    TEST_ASSERT_EQUAL_size_t(0u, place_of(&second_writer));
    TEST_ASSERT_EQUAL_size_t(2u, mmgr_owner_count());
    TEST_ASSERT_NULL(roll.listed.persistent_buf_alloc(16u, &second_writer));
}

void test_no_owner_and_an_owner_with_no_name_are_refused(void)
{
    TEST_ASSERT_FALSE(mmgr_owner_register(NULL));
    TEST_ASSERT_FALSE(mmgr_owner_register(&nameless));
    TEST_ASSERT_EQUAL_size_t(0u, place_of(&nameless));
    TEST_ASSERT_EQUAL_size_t(2u, mmgr_owner_count());
}

void test_the_queue_takes_owners_up_to_its_room_and_refuses_the_next(void)
{
    const size_t room = MMGR_CARCER_OWNERS - mmgr_owner_count();

    for (size_t at = 0u; at < MMGR_CARCER_OWNERS; at++)
    {
        s_crowd_names[at][0] = 'c';
        s_crowd_names[at][1] = (char)('0' + ((at / 100u) % 10u));
        s_crowd_names[at][2] = (char)('0' + ((at / 10u) % 10u));
        s_crowd_names[at][3] = (char)('0' + (at % 10u));
        s_crowd_names[at][4] = '\0';
        s_crowd[at] = (CellOwner)MMGR_CELL_OWNER(s_crowd_names[at]);
    }
    for (size_t at = 0u; at < room; at++)
    {
        TEST_ASSERT_TRUE(mmgr_owner_register(&s_crowd[at]));
        TEST_ASSERT_EQUAL_size_t(at + 3u, place_of(&s_crowd[at]));
    }
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_OWNERS, mmgr_owner_count());
    TEST_ASSERT_EQUAL_PTR(&s_crowd[room - 1u], mmgr_owner_at(MMGR_CARCER_OWNERS - 1u));
    TEST_ASSERT_FALSE_MESSAGE(mmgr_owner_register(&s_crowd[room]), "a full queue refuses the next owner");
    TEST_ASSERT_EQUAL_size_t(0u, place_of(&s_crowd[room]));
    TEST_ASSERT_NULL(mmgr_owner_at(MMGR_CARCER_OWNERS));
}
