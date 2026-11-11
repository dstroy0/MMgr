#include "locus_carcerum/locus_carcerum.h"

#include "unity.h"

#define PLAIN_BYTES 1024u
#define WIPED_BYTES 1024u

ParsMemoriaeInternae(plain, PLAIN_BYTES);
ParsMemoriaeInternae(wiped, WIPED_BYTES);

LocusCarcerum(counted, MMGR_MINIMUM_SECURITY(plain), MMGR_MAXIMUM_SECURITY(wiped));

// The owners every case takes for, declared with the cellblocks as the program's table
#define COUNTED_OWNERS(owner_)                                                                                         \
    owner_(reader, "reader") owner_(writer, "writer") owner_(scratch, "scratch") owner_(keys, "keys")
MMGR_OWNERS(COUNTED_OWNERS)

// What a give or growth that names the wrong owner told: how many, and the last one's two owners and bytes
static size_t s_misnamed_count;
static const CellOwner *s_misnamed_named;
static const CellOwner *s_misnamed_cell_owner;
static size_t s_misnamed_bytes;

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    s_misnamed_count += 1u;
    s_misnamed_named = named;
    s_misnamed_cell_owner = cell_owner;
    s_misnamed_bytes = bytes;
}

// An owner's counts put back to nothing for the case, its peak among them. The record is MMgr's to write, and the
// identity's const says only that a holder does not
static void owner_ready(const CellOwner *owner)
{
    mmgr_owner_bytes_given(owner, mmgr_owner_in_use(owner));
    embed_atomic_usize_store(&((CellOwner *)(uintptr_t)owner)->hw, 0u);
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
    mmgr_series_cellularum_emptied(&counted_plain_ctx);
    mmgr_series_cellularum_emptied(&counted_wiped_ctx);
    owner_ready(reader);
    owner_ready(writer);
    owner_ready(scratch);
    owner_ready(keys);
    s_misnamed_count = 0u;
    s_misnamed_named = NULL;
    s_misnamed_cell_owner = NULL;
    s_misnamed_bytes = 0u;
}

void tearDown(void)
{
}

void test_the_table_holds_the_owners_declared_in_their_order(void)
{
    TEST_ASSERT_EQUAL_size_t(4u, mmgr_owner_table_count);
    TEST_ASSERT_EQUAL_PTR(&mmgr_owner_table[0], reader);
    TEST_ASSERT_EQUAL_PTR(&mmgr_owner_table[3], keys);
    TEST_ASSERT_EQUAL_STRING("reader", reader->name);
    TEST_ASSERT_EQUAL_size_t(1u, reader->place);
    TEST_ASSERT_EQUAL_size_t(4u, keys->place);
    TEST_ASSERT_TRUE(mmgr_owner_listed(reader));
    TEST_ASSERT_FALSE_MESSAGE(mmgr_owner_listed(NULL), "no owner is none of the table's");
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(reader));
    TEST_ASSERT_EQUAL_size_t(0u, hw_of(reader));
}

void test_a_take_counts_the_cell_to_its_owner(void)
{
    TEST_ASSERT_NOT_NULL(counted.plain.persistent_buf_alloc(33u, reader));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mmgr_align_up_buf(33u), in_use_of(reader),
                                     "the cell's payload is what is counted");
    TEST_ASSERT_EQUAL_size_t(in_use_of(reader), hw_of(reader));
}

void test_a_give_takes_the_same_bytes_off(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(33u, reader);

    counted.plain.persistent_buf_release(prisoner, reader);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, in_use_of(reader), "a matched take and give leave nothing counted");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mmgr_align_up_buf(33u), hw_of(reader), "the give does not lower the peak");
}

void test_the_peak_is_the_most_held_at_once(void)
{
    void *const first = counted.plain.persistent_buf_alloc(64u, reader);
    void *const second = counted.plain.persistent_buf_alloc(32u, reader);

    counted.plain.persistent_buf_release(first, reader);
    void *const third = counted.plain.persistent_buf_alloc(16u, reader);

    TEST_ASSERT_EQUAL_size_t(48u, in_use_of(reader));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(96u, hw_of(reader), "the peak is the two first cells together");
    counted.plain.persistent_buf_release(second, reader);
    counted.plain.persistent_buf_release(third, reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(reader));
    TEST_ASSERT_EQUAL_size_t(96u, hw_of(reader));
}

void test_a_cell_given_back_below_the_end_counts_off_at_once(void)
{
    void *const wide = counted.plain.persistent_buf_alloc(64u, reader);
    void *const pin = counted.plain.persistent_buf_alloc(8u, reader);

    // the wide cell waits in the tier under the pin. Its bytes come off the owner the moment it goes
    // back, not when the trim reaches it
    counted.plain.persistent_buf_release(wide, reader);
    TEST_ASSERT_EQUAL_size_t(8u, in_use_of(reader));
    counted.plain.persistent_buf_release(pin, reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(reader));

    // both cells are the gap's again, and the next take lands where the wide one stood
    void *const again = counted.plain.persistent_buf_alloc(48u, reader);

    TEST_ASSERT_EQUAL_PTR(wide, again);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(48u, in_use_of(reader), "a fresh cell counts the bytes asked, rounded");
    counted.plain.persistent_buf_release(again, reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(reader));
}

void test_a_refused_take_counts_nothing(void)
{
    TEST_ASSERT_NULL(counted.plain.persistent_buf_alloc(PLAIN_BYTES, reader));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(reader));
    TEST_ASSERT_EQUAL_size_t(0u, hw_of(reader));
}

void test_a_take_naming_no_owner_is_refused(void)
{
    TEST_ASSERT_NULL(counted.plain.persistent_buf_alloc(32u, NULL));
    TEST_ASSERT_NULL(counted.plain.temporary_buf_alloc(32u, NULL));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(PLAIN_BYTES, mmgr_buf_available(&counted_plain_ctx),
                                     "a refused take moves no boundary");
}

void test_a_take_naming_an_owner_the_queue_did_not_hand_out_is_refused(void)
{
    // a record of the caller's own: its address is none the queue holds
    CellOwner stranger = {"stranger", 0u, EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)};

    TEST_ASSERT_FALSE(mmgr_owner_listed(&stranger));

    TEST_ASSERT_NULL(counted.plain.persistent_buf_alloc(32u, &stranger));
    TEST_ASSERT_NULL(counted.plain.temporary_buf_alloc(32u, &stranger));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(&stranger));
    TEST_ASSERT_EQUAL_size_t(PLAIN_BYTES, mmgr_buf_available(&counted_plain_ctx));
}

void test_a_give_naming_no_owner_gives_nothing_back(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(32u, reader);
    const size_t available = mmgr_buf_available(&counted_plain_ctx);

    counted.plain.persistent_buf_release(prisoner, NULL);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(32u, in_use_of(reader), "the cell stays counted to its owner");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(available, mmgr_buf_available(&counted_plain_ctx), "and stays in the tier");
    counted.plain.persistent_buf_release(prisoner, reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(reader));
}

void test_a_growth_naming_no_owner_is_refused(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(32u, reader);

    TEST_ASSERT_NULL(counted.plain.buf_grow(prisoner, 64u, NULL));
    TEST_ASSERT_EQUAL_size_t(32u, in_use_of(reader));
    TEST_ASSERT_EQUAL_PTR(prisoner, counted.plain.buf_grow(prisoner, 64u, reader));
    TEST_ASSERT_EQUAL_size_t(64u, in_use_of(reader));
}

void test_two_owners_are_counted_apart(void)
{
    void *const read_cell = counted.plain.persistent_buf_alloc(16u, reader);
    void *const write_cell = counted.plain.persistent_buf_alloc(40u, writer);

    TEST_ASSERT_EQUAL_size_t(16u, in_use_of(reader));
    TEST_ASSERT_EQUAL_size_t(40u, in_use_of(writer));
    counted.plain.persistent_buf_release(write_cell, writer);
    TEST_ASSERT_EQUAL_size_t(16u, in_use_of(reader));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(writer));
    counted.plain.persistent_buf_release(read_cell, reader);
}

void test_a_maximum_security_give_counts_like_a_plain_one(void)
{
    void *const prisoner = counted.wiped.persistent_buf_alloc(24u, keys);

    TEST_ASSERT_EQUAL_size_t(24u, in_use_of(keys));
    counted.wiped.persistent_buf_release(prisoner, keys);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(keys));
    TEST_ASSERT_EQUAL_size_t(24u, hw_of(keys));
}

void test_one_owner_counts_across_series_cellularum(void)
{
    void *const plain_cell = counted.plain.persistent_buf_alloc(16u, reader);
    void *const wiped_cell = counted.wiped.persistent_buf_alloc(16u, reader);

    TEST_ASSERT_EQUAL_size_t(32u, in_use_of(reader));
    counted.plain.persistent_buf_release(plain_cell, reader);
    counted.wiped.persistent_buf_release(wiped_cell, reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(reader));
    TEST_ASSERT_EQUAL_size_t(32u, hw_of(reader));
}

void test_a_temporary_take_counts_the_cell_to_its_owner(void)
{
    void *const cell = counted.plain.temporary_buf_alloc(33u, scratch);

    TEST_ASSERT_NOT_NULL(cell);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mmgr_align_up_buf(33u), in_use_of(scratch),
                                     "the cell's payload is what is counted");
    TEST_ASSERT_EQUAL_size_t(in_use_of(scratch), hw_of(scratch));
    counted.plain.persistent_buf_release(cell, scratch);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(scratch));
}

void test_temporary_cells_given_back_count_off_their_owners(void)
{
    void *const kept = counted.plain.temporary_buf_alloc(16u, reader);
    void *const read = counted.plain.temporary_buf_alloc(24u, reader);
    void *const written = counted.plain.temporary_buf_alloc(40u, writer);

    TEST_ASSERT_NOT_NULL(kept);
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_NOT_NULL(written);
    TEST_ASSERT_EQUAL_size_t(40u, in_use_of(reader));
    TEST_ASSERT_EQUAL_size_t(40u, in_use_of(writer));
    counted.plain.persistent_buf_release(read, reader);
    counted.plain.persistent_buf_release(written, writer);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(16u, in_use_of(reader), "the cell still held stays counted");
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(writer));
    counted.plain.persistent_buf_release(kept, reader);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(reader));
    TEST_ASSERT_EQUAL_size_t(40u, hw_of(reader));
}

void test_a_temporary_cell_given_back_twice_counts_off_once(void)
{
    void *const prisoner = counted.plain.temporary_buf_alloc(32u, scratch);

    counted.plain.persistent_buf_release(prisoner, scratch);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(scratch));
    counted.plain.persistent_buf_release(prisoner, scratch);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, in_use_of(scratch), "a second give of the same cell counts off no owner");
}

void test_a_give_naming_another_owner_is_refused_and_told(void)
{
    void *const kept = counted.plain.persistent_buf_alloc(32u, writer);
    void *const scratched = counted.plain.temporary_buf_alloc(16u, scratch);
    const size_t available = mmgr_buf_available(&counted_plain_ctx);

    counted.plain.persistent_buf_release(kept, reader);
    TEST_ASSERT_EQUAL_size_t(1u, s_misnamed_count);
    TEST_ASSERT_EQUAL_PTR(reader, s_misnamed_named);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(writer, s_misnamed_cell_owner, "told which owner the cell counts to");
    TEST_ASSERT_EQUAL_size_t(32u, s_misnamed_bytes);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(32u, in_use_of(writer), "the cell stays counted to its owner");
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(reader));
    counted.plain.persistent_buf_release(scratched, reader);
    TEST_ASSERT_EQUAL_size_t(2u, s_misnamed_count);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(16u, in_use_of(scratch), "a temporary cell is held the same way");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(available, mmgr_buf_available(&counted_plain_ctx), "and no cell left a tier");
    counted.plain.persistent_buf_release(kept, writer);
    counted.plain.persistent_buf_release(scratched, scratch);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(writer));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(scratch));
    TEST_ASSERT_EQUAL_size_t(2u, s_misnamed_count);
}

void test_a_maximum_security_give_naming_another_owner_zeroes_nothing(void)
{
    uint8_t *const secret = (uint8_t *)counted.wiped.persistent_buf_alloc(16u, keys);

    secret[0] = 0xA5u;
    counted.wiped.persistent_buf_release(secret, reader);
    TEST_ASSERT_EQUAL_size_t(1u, s_misnamed_count);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, secret[0], "the holder's bytes are left as they are");
    counted.wiped.persistent_buf_release(secret, keys);
    TEST_ASSERT_EQUAL_UINT8(0u, secret[0]);
}

void test_a_growth_naming_another_owner_is_refused_and_told(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(32u, writer);

    TEST_ASSERT_NULL(counted.plain.buf_grow(prisoner, 64u, reader));
    TEST_ASSERT_EQUAL_size_t(1u, s_misnamed_count);
    TEST_ASSERT_EQUAL_size_t(32u, in_use_of(writer));
    TEST_ASSERT_EQUAL_PTR(prisoner, counted.plain.buf_grow(prisoner, 64u, writer));
    TEST_ASSERT_EQUAL_size_t(64u, in_use_of(writer));
}

void test_a_cell_answers_the_owner_it_counts_to(void)
{
    void *const kept = counted.plain.persistent_buf_alloc(32u, writer);
    void *const scratched = counted.wiped.temporary_buf_alloc(16u, scratch);

    TEST_ASSERT_EQUAL_PTR(writer, mmgr_owner_of_buf(&counted_plain_ctx, kept));
    TEST_ASSERT_EQUAL_PTR(scratch, mmgr_owner_of_buf(&counted_wiped_ctx, scratched));
    TEST_ASSERT_NULL_MESSAGE(mmgr_owner_of_buf(&counted_wiped_ctx, kept), "a cell of another cellblock answers none");
    TEST_ASSERT_NULL(mmgr_owner_of_buf(&counted_plain_ctx, NULL));
    counted.plain.persistent_buf_release(kept, mmgr_owner_of_buf(&counted_plain_ctx, kept));
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(writer));
    TEST_ASSERT_NULL_MESSAGE(mmgr_owner_of_buf(&counted_plain_ctx, kept), "a cell given back answers none");
    counted.wiped.persistent_buf_release(scratched, scratch);
}

void test_a_cell_given_back_twice_counts_off_once_and_tells_nothing(void)
{
    void *const prisoner = counted.plain.persistent_buf_alloc(32u, writer);
    void *const pin = counted.plain.persistent_buf_alloc(8u, writer);

    counted.plain.persistent_buf_release(prisoner, writer);
    counted.plain.persistent_buf_release(prisoner, writer);
    TEST_ASSERT_EQUAL_size_t(8u, in_use_of(writer));
    TEST_ASSERT_EQUAL_size_t(0u, s_misnamed_count);
    counted.plain.persistent_buf_release(pin, writer);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(writer));
}

void test_a_maximum_security_temporary_give_zeroes_the_cell_and_counts_it_off(void)
{
    uint8_t *const prisoner = (uint8_t *)counted.wiped.temporary_buf_alloc(24u, keys);

    TEST_ASSERT_NOT_NULL(prisoner);
    prisoner[0] = 0xA5u;
    TEST_ASSERT_EQUAL_size_t(24u, in_use_of(keys));
    counted.wiped.persistent_buf_release(prisoner, keys);
    TEST_ASSERT_EQUAL_size_t(0u, in_use_of(keys));
    TEST_ASSERT_EQUAL_UINT8(0u, prisoner[0]);
}
