#include "locus_carcerum/locus_carcerum.h"

#include "unity.h"

#define PLAIN_BYTES 1024u
#define WIPED_BYTES 1024u

ParsMemoriaeInternae(plain, PLAIN_BYTES);
ParsMemoriaeInternae(wiped, WIPED_BYTES);

LocusCarcerum(counted, MMGR_MINIMUM_SECURITY(plain), MMGR_MAXIMUM_SECURITY(wiped));

// The owners every case takes for, registered once by the first setUp and their counts put back to
// zero by every one: the owner queue lives as long as the program does
static CellOwner reader = MMGR_CELL_OWNER("reader");
static CellOwner writer = MMGR_CELL_OWNER("writer");
static CellOwner scratch = MMGR_CELL_OWNER("scratch");
static CellOwner keys = MMGR_CELL_OWNER("keys");

// The clock the library reads, which each case sets
static uint32_t s_now_ms;

// What the backstop told: how many cells, and the last one's owner, bytes and time past its lifetime
static size_t s_expired_count;
static const CellOwner *s_expired_owner;
static size_t s_expired_bytes;
static uint32_t s_expired_overdue_ms;

// What a give or growth naming the wrong owner told: how many, and the last one's two owners and bytes
static size_t s_misnamed_count;
static const CellOwner *s_misnamed_named;
static const CellOwner *s_misnamed_cell_owner;
static size_t s_misnamed_bytes;

uint32_t mmgr_carcer_hw_clock_ms(void)
{
    return s_now_ms;
}

void mmgr_carcer_hw_expired(const CellOwner *owner, size_t bytes, uint32_t overdue_ms)
{
    s_expired_count += 1u;
    s_expired_owner = owner;
    s_expired_bytes = bytes;
    s_expired_overdue_ms = overdue_ms;
}

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    s_misnamed_count += 1u;
    s_misnamed_named = named;
    s_misnamed_cell_owner = cell_owner;
    s_misnamed_bytes = bytes;
}

static void owner_ready(CellOwner *owner)
{
    if (embed_atomic_usize_load(&owner->place) == 0u)
    {
        TEST_ASSERT_TRUE(mmgr_owner_register(owner));
    }
    embed_atomic_usize_store(&owner->in_use, 0u);
    embed_atomic_usize_store(&owner->hw, 0u);
}

static size_t in_use_of(const CellOwner *owner)
{
    return (size_t)embed_atomic_usize_load(&owner->in_use);
}

static size_t hw_of(const CellOwner *owner)
{
    return (size_t)embed_atomic_usize_load(&owner->hw);
}

void setUp(void)
{
    mmgr_cellblock_emptied(&counted_plain_ctx);
    mmgr_cellblock_emptied(&counted_wiped_ctx);
    owner_ready(&reader);
    owner_ready(&writer);
    owner_ready(&scratch);
    owner_ready(&keys);
    s_now_ms = 0u;
    s_expired_count = 0u;
    s_expired_owner = NULL;
    s_expired_bytes = 0u;
    s_expired_overdue_ms = 0u;
    s_misnamed_count = 0u;
    s_misnamed_named = NULL;
    s_misnamed_cell_owner = NULL;
    s_misnamed_bytes = 0u;
}

void tearDown(void)
{
}

void test_a_new_owner_has_nothing_counted_and_no_key(void)
{
    const CellOwner owner = MMGR_CELL_OWNER("unregistered");

    TEST_ASSERT_EQUAL_STRING("unregistered", owner.name);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&owner.place));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&owner));
    TEST_ASSERT_EQUAL_size_t(0u, hw_of(&owner));
}

void test_a_take_counts_the_cell_to_its_owner(void)
{
    TEST_ASSERT_NOT_NULL(counted.plain.persistent_buf_alloc(33u, &reader));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mmgr_align_up_buf(33u), in_use_of(&reader),
                                     "the cell's payload is what is counted");
    TEST_ASSERT_EQUAL_size_t(in_use_of(&reader), hw_of(&reader));
}

void test_a_give_takes_the_same_bytes_off(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(33u, &reader);

    counted.plain.persistent_buf_release(prisoner, &reader);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, in_use_of(&reader), "a matched take and give leave nothing counted");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mmgr_align_up_buf(33u), hw_of(&reader), "the give does not lower the peak");
}

void test_the_peak_is_the_most_held_at_once(void)
{
    void *const first = counted.plain.persistent_buf_alloc(64u, &reader);
    void *const second = counted.plain.persistent_buf_alloc(32u, &reader);

    counted.plain.persistent_buf_release(first, &reader);
    void *const third = counted.plain.persistent_buf_alloc(16u, &reader);

    TEST_ASSERT_EQUAL_size_t(48u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(96u, hw_of(&reader), "the peak is the two first cells together");
    counted.plain.persistent_buf_release(second, &reader);
    counted.plain.persistent_buf_release(third, &reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_size_t(96u, hw_of(&reader));
}

void test_a_cell_given_back_below_the_end_counts_off_at_once(void)
{
    void *const wide = counted.plain.persistent_buf_alloc(64u, &reader);
    void *const pin = counted.plain.persistent_buf_alloc(8u, &reader);

    // the wide cell waits in the tier under the pin, and its bytes come off the owner the moment it is
    // given back, not when the trim reaches it
    counted.plain.persistent_buf_release(wide, &reader);
    TEST_ASSERT_EQUAL_size_t(8u, in_use_of(&reader));
    counted.plain.persistent_buf_release(pin, &reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&reader));

    // both cells are the gap's again, and the next take lands where the wide one stood
    void *const again = counted.plain.persistent_buf_alloc(48u, &reader);

    TEST_ASSERT_EQUAL_PTR(wide, again);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(48u, in_use_of(&reader), "a fresh cell counts the bytes asked, rounded");
    counted.plain.persistent_buf_release(again, &reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&reader));
}

void test_a_refused_take_counts_nothing(void)
{
    TEST_ASSERT_NULL(counted.plain.persistent_buf_alloc(PLAIN_BYTES, &reader));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_size_t(0u, hw_of(&reader));
}

void test_a_take_naming_no_owner_is_refused(void)
{
    TEST_ASSERT_NULL(counted.plain.persistent_buf_alloc(32u, NULL));
    TEST_ASSERT_NULL(counted.plain.temporary_buf_alloc(32u, NULL, 100u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(PLAIN_BYTES, mmgr_buf_available(&counted_plain_ctx),
                                     "a refused take moves no boundary");
}

void test_a_take_naming_an_owner_not_registered_is_refused(void)
{
    CellOwner stranger = MMGR_CELL_OWNER("stranger");

    TEST_ASSERT_NULL(counted.plain.persistent_buf_alloc(32u, &stranger));
    TEST_ASSERT_NULL(counted.plain.temporary_buf_alloc(32u, &stranger, 100u));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&stranger));
    TEST_ASSERT_EQUAL_size_t(PLAIN_BYTES, mmgr_buf_available(&counted_plain_ctx));
}

void test_a_give_naming_no_owner_gives_nothing_back(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(32u, &reader);
    const size_t available = mmgr_buf_available(&counted_plain_ctx);

    counted.plain.persistent_buf_release(prisoner, NULL);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(32u, in_use_of(&reader), "the cell stays counted to its owner");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(available, mmgr_buf_available(&counted_plain_ctx), "and stays in the tier");
    counted.plain.persistent_buf_release(prisoner, &reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&reader));
}

void test_a_growth_naming_no_owner_is_refused(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(32u, &reader);

    TEST_ASSERT_NULL(counted.plain.buf_grow(prisoner, 64u, NULL));
    TEST_ASSERT_EQUAL_size_t(32u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_PTR(prisoner, counted.plain.buf_grow(prisoner, 64u, &reader));
    TEST_ASSERT_EQUAL_size_t(64u, in_use_of(&reader));
}

void test_two_owners_are_counted_apart(void)
{
    void *const read_cell = counted.plain.persistent_buf_alloc(16u, &reader);
    void *const write_cell = counted.plain.persistent_buf_alloc(40u, &writer);

    TEST_ASSERT_EQUAL_size_t(16u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_size_t(40u, in_use_of(&writer));
    counted.plain.persistent_buf_release(write_cell, &writer);
    TEST_ASSERT_EQUAL_size_t(16u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&writer));
    counted.plain.persistent_buf_release(read_cell, &reader);
}

void test_a_maximum_security_give_counts_like_a_plain_one(void)
{
    void *const prisoner = counted.wiped.persistent_buf_alloc(24u, &keys);

    TEST_ASSERT_EQUAL_size_t(24u, in_use_of(&keys));
    counted.wiped.persistent_buf_release(prisoner, &keys);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&keys));
    TEST_ASSERT_EQUAL_size_t(24u, hw_of(&keys));
}

void test_one_owner_counts_across_cellblocks(void)
{
    void *const plain_cell = counted.plain.persistent_buf_alloc(16u, &reader);
    void *const wiped_cell = counted.wiped.persistent_buf_alloc(16u, &reader);

    TEST_ASSERT_EQUAL_size_t(32u, in_use_of(&reader));
    counted.plain.persistent_buf_release(plain_cell, &reader);
    counted.wiped.persistent_buf_release(wiped_cell, &reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_size_t(32u, hw_of(&reader));
}

void test_a_temporary_take_counts_the_cell_to_its_owner(void)
{
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(33u, &scratch, 100u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mmgr_align_up_buf(33u), in_use_of(&scratch),
                                     "the cell's payload is what is counted");
    TEST_ASSERT_EQUAL_size_t(in_use_of(&scratch), hw_of(&scratch));
    counted.plain.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&scratch));
}

void test_a_rewind_counts_the_dropped_cells_off_their_owners(void)
{
    void *const kept = counted.plain.temporary_buf_alloc(16u, &reader, 100u);
    const size_t mark = counted.plain.temporary_buf_mark();

    TEST_ASSERT_NOT_NULL(kept);
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(24u, &reader, 100u));
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(40u, &writer, 100u));
    TEST_ASSERT_EQUAL_size_t(40u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_size_t(40u, in_use_of(&writer));
    counted.plain.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(16u, in_use_of(&reader), "the cell below the mark stays counted");
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&writer));
    counted.plain.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_size_t(40u, hw_of(&reader));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, s_expired_count,
                                     "a rewind is the holder's give, and the backstop tells nothing");
}

void test_a_temporary_cell_given_back_alone_counts_off_its_owner(void)
{
    void *const prisoner = counted.plain.temporary_buf_alloc(32u, &scratch, 100u);

    counted.plain.persistent_buf_release(prisoner, &scratch);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&scratch));
    counted.plain.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, in_use_of(&scratch),
                                     "the reset counts a cell given back once already off no owner");
}

void test_a_give_naming_another_owner_is_refused_and_told(void)
{
    void *const kept = counted.plain.persistent_buf_alloc(32u, &writer);
    void *const scratched = counted.plain.temporary_buf_alloc(16u, &scratch, 100u);
    const size_t available = mmgr_buf_available(&counted_plain_ctx);

    counted.plain.persistent_buf_release(kept, &reader);
    TEST_ASSERT_EQUAL_size_t(1u, s_misnamed_count);
    TEST_ASSERT_EQUAL_PTR(&reader, s_misnamed_named);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&writer, s_misnamed_cell_owner, "told which owner the cell counts to");
    TEST_ASSERT_EQUAL_size_t(32u, s_misnamed_bytes);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(32u, in_use_of(&writer), "the cell stays counted to its owner");
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&reader));
    counted.plain.persistent_buf_release(scratched, &reader);
    TEST_ASSERT_EQUAL_size_t(2u, s_misnamed_count);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(16u, in_use_of(&scratch), "a temporary cell is held the same way");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(available, mmgr_buf_available(&counted_plain_ctx), "and no cell left a tier");
    counted.plain.persistent_buf_release(kept, &writer);
    counted.plain.persistent_buf_release(scratched, &scratch);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&writer));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&scratch));
    TEST_ASSERT_EQUAL_size_t(2u, s_misnamed_count);
}

void test_a_maximum_security_give_naming_another_owner_zeroes_nothing(void)
{
    uint8_t *const secret = (uint8_t *)counted.wiped.persistent_buf_alloc(16u, &keys);

    secret[0] = 0xA5u;
    counted.wiped.persistent_buf_release(secret, &reader);
    TEST_ASSERT_EQUAL_size_t(1u, s_misnamed_count);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, secret[0], "the holder's bytes are left as they are");
    counted.wiped.persistent_buf_release(secret, &keys);
    TEST_ASSERT_EQUAL_UINT8(0u, secret[0]);
}

void test_a_growth_naming_another_owner_is_refused_and_told(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(32u, &writer);

    TEST_ASSERT_NULL(counted.plain.buf_grow(prisoner, 64u, &reader));
    TEST_ASSERT_EQUAL_size_t(1u, s_misnamed_count);
    TEST_ASSERT_EQUAL_size_t(32u, in_use_of(&writer));
    TEST_ASSERT_EQUAL_PTR(prisoner, counted.plain.buf_grow(prisoner, 64u, &writer));
    TEST_ASSERT_EQUAL_size_t(64u, in_use_of(&writer));
}

void test_a_cell_answers_the_owner_it_counts_to(void)
{
    void *const kept = counted.plain.persistent_buf_alloc(32u, &writer);
    void *const scratched = counted.wiped.temporary_buf_alloc(16u, &scratch, 100u);

    TEST_ASSERT_EQUAL_PTR(&writer, mmgr_owner_of_buf(&counted_plain_ctx, kept));
    TEST_ASSERT_EQUAL_PTR(&scratch, mmgr_owner_of_buf(&counted_wiped_ctx, scratched));
    TEST_ASSERT_NULL_MESSAGE(mmgr_owner_of_buf(&counted_wiped_ctx, kept), "a cell of another cellblock answers none");
    TEST_ASSERT_NULL(mmgr_owner_of_buf(&counted_plain_ctx, NULL));
    counted.plain.persistent_buf_release(kept, mmgr_owner_of_buf(&counted_plain_ctx, kept));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&writer));
    TEST_ASSERT_NULL_MESSAGE(mmgr_owner_of_buf(&counted_plain_ctx, kept), "a cell given back answers none");
    counted.wiped.temporary_buf_reset();
}

void test_a_cell_given_back_twice_counts_off_once_and_tells_nothing(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(32u, &writer);
    void *const pin = counted.plain.persistent_buf_alloc(8u, &writer);

    counted.plain.persistent_buf_release(prisoner, &writer);
    counted.plain.persistent_buf_release(prisoner, &writer);
    TEST_ASSERT_EQUAL_size_t(8u, in_use_of(&writer));
    TEST_ASSERT_EQUAL_size_t(0u, s_misnamed_count);
    counted.plain.persistent_buf_release(pin, &writer);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&writer));
}

void test_a_maximum_security_rewind_counts_the_dropped_cells_off_their_owner(void)
{
    const size_t mark = counted.wiped.temporary_buf_mark();
    uint8_t *const prisoner = (uint8_t *)counted.wiped.temporary_buf_alloc(24u, &keys, 100u);

    TEST_ASSERT_NOT_NULL(prisoner);
    prisoner[0] = 0xA5u;
    TEST_ASSERT_EQUAL_size_t(24u, in_use_of(&keys));
    counted.wiped.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&keys));
    TEST_ASSERT_EQUAL_UINT8(0u, prisoner[0]);
}

void test_a_lifetime_outside_its_range_is_refused(void)
{
    TEST_ASSERT_NULL_MESSAGE(counted.plain.temporary_buf_alloc(16u, &scratch, 0u),
                             "a cell must live at least a millisecond");
    TEST_ASSERT_NULL_MESSAGE(counted.plain.temporary_buf_alloc(16u, &scratch, 0x80000000u),
                             "a lifetime the wrapping clock cannot tell from the past is refused");
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&scratch));
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(16u, &scratch, 0x7FFFFFFFu));
    counted.plain.temporary_buf_reset();
}

void test_a_cell_in_its_lifetime_is_left_to_its_holder(void)
{
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(16u, &scratch, 10u));
    s_now_ms = 9u;
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(16u, &writer, 10u));
    TEST_ASSERT_EQUAL_size_t(16u, in_use_of(&scratch));
    TEST_ASSERT_EQUAL_size_t(0u, s_expired_count);
    counted.plain.temporary_buf_reset();
}

void test_a_cell_past_its_lifetime_is_given_back_by_the_next_take_and_told(void)
{
    uint8_t *const lapsed = (uint8_t *)counted.plain.temporary_buf_alloc(16u, &scratch, 10u);

    TEST_ASSERT_NOT_NULL(lapsed);
    s_now_ms = 15u;
    uint8_t *const next = (uint8_t *)counted.plain.temporary_buf_alloc(16u, &writer, 10u);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, in_use_of(&scratch), "the lapsed cell's bytes come off its owner");
    TEST_ASSERT_EQUAL_size_t(16u, in_use_of(&writer));
    TEST_ASSERT_EQUAL_size_t(1u, s_expired_count);
    TEST_ASSERT_EQUAL_PTR(&scratch, s_expired_owner);
    TEST_ASSERT_EQUAL_size_t(16u, s_expired_bytes);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, s_expired_overdue_ms, "told how long past its lifetime the cell stood");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(lapsed, next, "the take lands where the lapsed cell stood");
    counted.plain.temporary_buf_reset();
}

void test_every_lapsed_cell_at_the_top_is_given_back_in_turn(void)
{
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(16u, &scratch, 10u));
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(24u, &writer, 20u));
    s_now_ms = 30u;
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(8u, &reader, 10u));
    TEST_ASSERT_EQUAL_size_t(2u, s_expired_count);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&scratch));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&writer));
    TEST_ASSERT_EQUAL_size_t(8u, in_use_of(&reader));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(PLAIN_BYTES - (3u * sizeof(embed_atomic_usize)) - 8u,
                                     mmgr_cellblock_temporary_top(&counted_plain_ctx),
                                     "the tier holds the new cell alone");
    counted.plain.temporary_buf_reset();
}

void test_a_lapsed_cell_under_one_in_its_lifetime_waits(void)
{
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(16u, &scratch, 10u));
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(16u, &writer, 100u));
    s_now_ms = 50u;
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(16u, &reader, 100u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, s_expired_count,
                                     "the top reads a cell in its lifetime, and the walk ends there");
    TEST_ASSERT_EQUAL_size_t(16u, in_use_of(&scratch));
    counted.plain.temporary_buf_reset();
}

void test_a_lifetime_holds_across_the_clock_wrapping(void)
{
    s_now_ms = 0xFFFFFFF0u;
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(16u, &scratch, 0x20u));
    s_now_ms = 0x0Fu;
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(8u, &reader, 0x20u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, s_expired_count, "one millisecond short of the end, past the wrap");
    counted.plain.temporary_buf_reset();
    s_now_ms = 0xFFFFFFF0u;
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(16u, &scratch, 0x20u));
    s_now_ms = 0x11u;
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(8u, &reader, 0x20u));
    TEST_ASSERT_EQUAL_size_t(1u, s_expired_count);
    TEST_ASSERT_EQUAL_UINT32(1u, s_expired_overdue_ms);
    counted.plain.temporary_buf_reset();
}

void test_a_persistent_take_the_gap_cannot_meet_gives_lapsed_cells_back_and_tries_again(void)
{
    TEST_ASSERT_NOT_NULL(counted.plain.temporary_buf_alloc(PLAIN_BYTES / 2u, &scratch, 10u));
    TEST_ASSERT_NULL(counted.plain.persistent_buf_alloc(PLAIN_BYTES * 3u / 4u, &reader));
    TEST_ASSERT_EQUAL_size_t(0u, s_expired_count);
    s_now_ms = 10u;
    TEST_ASSERT_NOT_NULL(counted.plain.persistent_buf_alloc(PLAIN_BYTES * 3u / 4u, &reader));
    TEST_ASSERT_EQUAL_size_t(1u, s_expired_count);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&scratch));
}

void test_a_maximum_security_cell_past_its_lifetime_is_given_back_too(void)
{
    TEST_ASSERT_NOT_NULL(counted.wiped.temporary_buf_alloc(24u, &keys, 5u));
    s_now_ms = 5u;
    TEST_ASSERT_NOT_NULL(counted.wiped.temporary_buf_alloc(8u, &reader, 5u));
    TEST_ASSERT_EQUAL_size_t(1u, s_expired_count);
    TEST_ASSERT_EQUAL_PTR(&keys, s_expired_owner);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&keys));
    counted.wiped.temporary_buf_reset();
}
