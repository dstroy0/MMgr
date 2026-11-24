/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_memoria_vector.c
 * @brief The calls of a vector through a guard: what each answers, the growth to the ceiling, the release, and pushes
 *        and pops from many threads at once.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-23
 *
 * @note Each case starts from a vector with no element and no cell. setUp releases each vector, and the owner of
 *       the cells reads zero bytes then.
 * @note The threaded cases push and pop the same vector from several threads. A lost element or an element that
 *       comes out twice changes the sum of the elements, which each case holds to the sum that went in.
 */
#include "memoria_vector/memoria_vector.h"

#include "embed_atomic.h"
#include "host_threads.h"
#include "unity.h"

#define CELLS_BYTES 65536u
#define CELLS_ENTRIES 8u
#define SMALL_BYTES 256u
#define WIDE_LIMIT 40u
#define CROWD_THREADS 4u
#define CROWD_EACH 200u

ParsMemoriaeInternae(cells, CELLS_BYTES);
ParsMemoriaeInternae(cells_option, MMGR_MOVABLE_STORAGE_BYTES(CELLS_ENTRIES, CROWD_THREADS, 0u));
ParsMemoriaeInternae(small, SMALL_BYTES);
ParsMemoriaeInternae(small_option, MMGR_MOVABLE_STORAGE_BYTES(2u, 1u, 0u));
LocusCarcerum(site, MMGR_MINIMUM_SECURITY(cells));
LocusCarcerum(tight, MMGR_MAXIMUM_SECURITY(small));

#define VECTOR_OWNERS(owner_) owner_(s_elements, "elements")
MMGR_OWNERS(VECTOR_OWNERS)

/** @brief An element with two fields and padding, to hold the stride at the size of the type. */
typedef struct
{
    uint32_t value;
    uint16_t tag;
} Element;

MemoriaVector(s_numbers, uint64_t, WIDE_LIMIT, s_elements);
MemoriaVector(s_records, Element, 4u, s_elements);
MemoriaVector(s_crowd, uint64_t, (CROWD_THREADS * CROWD_EACH), s_elements);
MemoriaVector(s_tight, uint64_t, 64u, s_elements);

static embed_bool s_assigned;
static embed_atomic_usize s_next_thread = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_popped_sum = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_popped_count = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_refused_pushes = EMBED_ATOMIC_INIT(0u);

// The calls of a test vector go through the minimum security guard, the tight one through the max security guard
#define THROUGH_SITE MMGR_VECTOR_THROUGH(site.cells)
#define THROUGH_TIGHT MMGR_VECTOR_THROUGH(tight.small)

// Each flag refuses the next call of its entry once, and then clears
static embed_bool s_refuse_read;
static embed_bool s_refuse_write;
static embed_bool s_refuse_give;
// Set, the next write pops one element of s_numbers before it writes, and then clears
static embed_bool s_pop_inside_write;
static uint64_t s_popped_inside;
// Set, the next write holds until the reader thread starts, and then clears
static embed_atomic_usize s_hold_write = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_write_held = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_reader_began = EMBED_ATOMIC_INIT(0u);

// The yields of the write that holds, after the reader starts, so the reader claims the held bit before the write ends
#define HELD_WRITE_YIELDS 10000u

static const CarcerMovableEntry *stub_take(size_t size, const CellOwner *owner)
{
    return site.cells.movable_buf_alloc(size, owner);
}

static embed_bool stub_give(const CarcerMovableEntry *cell, const CellOwner *owner)
{
    const embed_bool refused = s_refuse_give;
    s_refuse_give = EMBED_FALSE;
    return !refused && site.cells.movable_buf_release(cell, owner);
}

static embed_bool stub_read(const CarcerMovableCfg *args)
{
    const embed_bool refused = s_refuse_read;
    s_refuse_read = EMBED_FALSE;
    return !refused && site.cells.movable_buf_read(args);
}

static embed_atomic_usize_value flag_of(embed_atomic_usize *word)
{
    return EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg, .word = word);
}

static void flag_set(embed_atomic_usize *word, embed_atomic_usize_value value)
{
    EMBED_CALL(embed_atomic_usize_table.store, EmbedAtomicUsizeStoreCfg, .word = word, .value = value);
}

#define THROUGH_STUB .take = stub_take, .give = stub_give, .read = stub_read, .write = stub_write

static embed_bool stub_write(const CarcerMovableCfg *args)
{
    if (s_pop_inside_write)
    {
        s_pop_inside_write = EMBED_FALSE;
        TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_STUB, .vector = s_numbers,
                                    .dst = &s_popped_inside));
    }
    if (flag_of(&s_hold_write) != 0u)
    {
        flag_set(&s_hold_write, 0u);
        flag_set(&s_write_held, 1u);
        while (flag_of(&s_reader_began) == 0u)
        {
            mmgr_host_threads_yield();
        }
        for (size_t turn = 0u; turn < HELD_WRITE_YIELDS; turn++)
        {
            mmgr_host_threads_yield();
        }
    }
    const embed_bool refused = s_refuse_write;
    s_refuse_write = EMBED_FALSE;
    return !refused && site.cells.movable_buf_write(args);
}

void setUp(void)
{
    if (!s_assigned)
    {
        TEST_ASSERT_TRUE(site.cells.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_cells_option,
                                                                         .storage_bytes = cells_option_bytes,
                                                                         .cells = CELLS_ENTRIES,
                                                                         .width = CROWD_THREADS}));
        TEST_ASSERT_TRUE(tight.small.movable_init(&(CarcerMovableInitCfg){
            .storage = mmgr_pars_storage_small_option, .storage_bytes = small_option_bytes, .cells = 2u, .width = 1u}));
        s_assigned = EMBED_TRUE;
    }
    TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.release, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers));
    TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.release, MemoriaVectorCfg, THROUGH_SITE, .vector = s_records));
    TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.release, MemoriaVectorCfg, THROUGH_SITE, .vector = s_crowd));
    TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.release, MemoriaVectorCfg, THROUGH_TIGHT, .vector = s_tight));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(s_elements));
    EMBED_CALL(embed_atomic_usize_table.store, EmbedAtomicUsizeStoreCfg, .word = &s_next_thread, .value = 0u);
    EMBED_CALL(embed_atomic_usize_table.store, EmbedAtomicUsizeStoreCfg, .word = &s_popped_sum, .value = 0u);
    EMBED_CALL(embed_atomic_usize_table.store, EmbedAtomicUsizeStoreCfg, .word = &s_popped_count, .value = 0u);
    EMBED_CALL(embed_atomic_usize_table.store, EmbedAtomicUsizeStoreCfg, .word = &s_refused_pushes, .value = 0u);
    s_refuse_read = EMBED_FALSE;
    s_refuse_write = EMBED_FALSE;
    s_refuse_give = EMBED_FALSE;
    s_pop_inside_write = EMBED_FALSE;
    flag_set(&s_hold_write, 0u);
    flag_set(&s_write_held, 0u);
    flag_set(&s_reader_began, 0u);
}

void tearDown(void)
{
}

static embed_bool push_number(uint64_t value)
{
    return EMBED_CALL(memoria_vector.push_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .src = &value);
}

void test_a_new_vector_has_no_element_and_refuses_each_read(void)
{
    uint64_t out = 7u;
    TEST_ASSERT_EQUAL_size_t(0u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    TEST_ASSERT_FALSE(
        EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .index = 0u, .dst = &out));
    TEST_ASSERT_FALSE(
        EMBED_CALL(memoria_vector.set, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .index = 0u, .src = &out));
    TEST_ASSERT_FALSE(
        EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .dst = &out));
    TEST_ASSERT_EQUAL_UINT64(7u, out);
}

void test_pushes_read_back_in_their_order(void)
{
    for (uint64_t value = 0u; value < 10u; value++)
    {
        TEST_ASSERT_TRUE(push_number(100u + value));
    }
    TEST_ASSERT_EQUAL_size_t(10u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    for (size_t index = 0u; index < 10u; index++)
    {
        uint64_t out = 0u;
        TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers,
                                    .index = index, .dst = &out));
        TEST_ASSERT_EQUAL_UINT64(100u + index, out);
    }
}

void test_an_index_at_the_count_is_refused_and_changes_nothing(void)
{
    TEST_ASSERT_TRUE(push_number(5u));
    uint64_t out = 9u;
    TEST_ASSERT_FALSE(
        EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .index = 1u, .dst = &out));
    TEST_ASSERT_FALSE(
        EMBED_CALL(memoria_vector.set, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .index = 1u, .src = &out));
    TEST_ASSERT_EQUAL_UINT64(9u, out);
    TEST_ASSERT_EQUAL_size_t(1u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
}

void test_set_changes_one_element_alone(void)
{
    for (uint64_t value = 0u; value < 4u; value++)
    {
        TEST_ASSERT_TRUE(push_number(value));
    }
    const uint64_t changed = 77u;
    TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.set, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .index = 2u,
                                .src = &changed));
    const uint64_t expected[4] = {0u, 1u, 77u, 3u};
    for (size_t index = 0u; index < 4u; index++)
    {
        uint64_t out = 0u;
        TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers,
                                    .index = index, .dst = &out));
        TEST_ASSERT_EQUAL_UINT64(expected[index], out);
    }
}

void test_pop_back_gives_the_last_element_and_the_count_falls(void)
{
    TEST_ASSERT_TRUE(push_number(1u));
    TEST_ASSERT_TRUE(push_number(2u));
    uint64_t out = 0u;
    TEST_ASSERT_TRUE(
        EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .dst = &out));
    TEST_ASSERT_EQUAL_UINT64(2u, out);
    TEST_ASSERT_EQUAL_size_t(1u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    TEST_ASSERT_TRUE(
        EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .dst = &out));
    TEST_ASSERT_EQUAL_UINT64(1u, out);
    TEST_ASSERT_FALSE(
        EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .dst = &out));
}

void test_growths_keep_every_element_up_to_the_ceiling(void)
{
    for (uint64_t value = 0u; value < WIDE_LIMIT; value++)
    {
        TEST_ASSERT_TRUE(push_number(value * 3u));
    }
    for (size_t index = 0u; index < WIDE_LIMIT; index++)
    {
        uint64_t out = 0u;
        TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers,
                                    .index = index, .dst = &out));
        TEST_ASSERT_EQUAL_UINT64(index * 3u, out);
    }
    TEST_ASSERT_EQUAL_size_t(WIDE_LIMIT * sizeof(uint64_t), mmgr_owner_in_use(s_elements));
}

void test_a_push_at_the_ceiling_is_refused_and_changes_nothing(void)
{
    for (uint64_t value = 0u; value < WIDE_LIMIT; value++)
    {
        TEST_ASSERT_TRUE(push_number(value));
    }
    TEST_ASSERT_FALSE(push_number(999u));
    TEST_ASSERT_EQUAL_size_t(WIDE_LIMIT, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    uint64_t out = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers,
                                .index = WIDE_LIMIT - 1u, .dst = &out));
    TEST_ASSERT_EQUAL_UINT64(WIDE_LIMIT - 1u, out);
}

void test_a_release_gives_the_cell_back_and_a_later_push_starts_again(void)
{
    TEST_ASSERT_TRUE(push_number(4u));
    TEST_ASSERT_TRUE(mmgr_owner_in_use(s_elements) > 0u);
    TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.release, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(s_elements));
    TEST_ASSERT_EQUAL_size_t(0u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    TEST_ASSERT_TRUE(push_number(8u));
    uint64_t out = 0u;
    TEST_ASSERT_TRUE(
        EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_numbers, .index = 0u, .dst = &out));
    TEST_ASSERT_EQUAL_UINT64(8u, out);
}

void test_an_element_type_copies_whole_with_its_stride(void)
{
    const Element first = {.value = 0x01020304u, .tag = 0x0506u};
    const Element second = {.value = 0x0A0B0C0Du, .tag = 0x0E0Fu};
    TEST_ASSERT_TRUE(
        EMBED_CALL(memoria_vector.push_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_records, .src = &first));
    TEST_ASSERT_TRUE(
        EMBED_CALL(memoria_vector.push_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_records, .src = &second));
    Element out = {0};
    TEST_ASSERT_TRUE(
        EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_records, .index = 1u, .dst = &out));
    TEST_ASSERT_EQUAL_HEX32(second.value, out.value);
    TEST_ASSERT_EQUAL_HEX16(second.tag, out.tag);
    TEST_ASSERT_EQUAL_size_t(sizeof(Element), s_records->stride);
}

void test_a_growth_the_cellblock_cannot_meet_refuses_its_push(void)
{
    size_t pushed = 0u;
    embed_bool refused = EMBED_FALSE;
    for (uint64_t value = 0u; (value < 64u) && !refused; value++)
    {
        refused =
            !EMBED_CALL(memoria_vector.push_back, MemoriaVectorCfg, THROUGH_TIGHT, .vector = s_tight, .src = &value);
        pushed += refused ? 0u : 1u;
    }
    TEST_ASSERT_TRUE(refused);
    TEST_ASSERT_EQUAL_size_t(pushed, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_tight));
    for (size_t index = 0u; index < pushed; index++)
    {
        uint64_t out = 0u;
        TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_TIGHT, .vector = s_tight,
                                    .index = index, .dst = &out));
        TEST_ASSERT_EQUAL_UINT64(index, out);
    }
}

// Each thread pushes CROWD_EACH values of its own, from its place times CROWD_EACH
static void crowd_pushes(void)
{
    // Explicit cast reads the place as the index of the thread
    const size_t place = (size_t)EMBED_CALL(embed_atomic_usize_table.fetch_add, EmbedAtomicUsizeFetchAddCfg,
                                            .word = &s_next_thread, .value = 1u);
    for (size_t step = 0u; step < CROWD_EACH; step++)
    {
        const uint64_t value = (uint64_t)(place * CROWD_EACH + step + 1u);
        const embed_bool pushed =
            EMBED_CALL(memoria_vector.push_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_crowd, .src = &value);
        // Explicit cast counts a refused push as one
        (void)EMBED_CALL(embed_atomic_usize_table.fetch_add, EmbedAtomicUsizeFetchAddCfg, .word = &s_refused_pushes,
                         .value = (embed_atomic_usize_value)(pushed ? 0u : 1u));
    }
}

// Each thread pops until the vector has no element, and counts and sums what it took
static void crowd_pops(void)
{
    uint64_t out = 0u;
    embed_bool popped =
        EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_crowd, .dst = &out);
    while (popped)
    {
        // Explicit cast adds the value at the width of the sum
        (void)EMBED_CALL(embed_atomic_usize_table.fetch_add, EmbedAtomicUsizeFetchAddCfg, .word = &s_popped_sum,
                         .value = (embed_atomic_usize_value)out);
        (void)EMBED_CALL(embed_atomic_usize_table.fetch_add, EmbedAtomicUsizeFetchAddCfg, .word = &s_popped_count,
                         .value = 1u);
        popped = EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_crowd, .dst = &out);
    }
}

void test_pushes_from_many_threads_keep_every_element_once(void)
{
    TEST_ASSERT_TRUE(mmgr_host_threads_run(crowd_pushes, CROWD_THREADS));
    TEST_ASSERT_EQUAL_size_t(
        0u, (size_t)EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg, .word = &s_refused_pushes));
    const size_t total = CROWD_THREADS * CROWD_EACH;
    TEST_ASSERT_EQUAL_size_t(total, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_crowd));
    uint64_t sum = 0u;
    for (size_t index = 0u; index < total; index++)
    {
        uint64_t out = 0u;
        TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_crowd,
                                    .index = index, .dst = &out));
        sum += out;
    }
    TEST_ASSERT_EQUAL_UINT64((uint64_t)total * (total + 1u) / 2u, sum);
}

void test_pops_from_many_threads_take_every_element_once(void)
{
    const size_t total = CROWD_THREADS * CROWD_EACH;
    for (uint64_t value = 1u; value <= total; value++)
    {
        TEST_ASSERT_TRUE(
            EMBED_CALL(memoria_vector.push_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_crowd, .src = &value));
    }
    TEST_ASSERT_TRUE(mmgr_host_threads_run(crowd_pops, CROWD_THREADS));
    TEST_ASSERT_EQUAL_size_t(0u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_crowd));
    TEST_ASSERT_EQUAL_size_t(
        total, (size_t)EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg, .word = &s_popped_count));
    TEST_ASSERT_EQUAL_UINT64(
        (uint64_t)total * (total + 1u) / 2u,
        (uint64_t)EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg, .word = &s_popped_sum));
}

// Each thread pushes a value of its own and pops one, in turn. A push and a pop at one count meet at its CAS
static void crowd_mixes(void)
{
    // Explicit cast reads the place as the index of the thread
    const size_t place = (size_t)EMBED_CALL(embed_atomic_usize_table.fetch_add, EmbedAtomicUsizeFetchAddCfg,
                                            .word = &s_next_thread, .value = 1u);
    for (size_t step = 0u; step < CROWD_EACH; step++)
    {
        const uint64_t value = (uint64_t)(place * CROWD_EACH + step + 1u);
        const embed_bool pushed =
            EMBED_CALL(memoria_vector.push_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_crowd, .src = &value);
        // Explicit cast counts a refused push as one
        (void)EMBED_CALL(embed_atomic_usize_table.fetch_add, EmbedAtomicUsizeFetchAddCfg, .word = &s_refused_pushes,
                         .value = (embed_atomic_usize_value)(pushed ? 0u : 1u));
        uint64_t out = 0u;
        if (EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_SITE, .vector = s_crowd, .dst = &out))
        {
            // Explicit cast adds the value at the width of the sum
            (void)EMBED_CALL(embed_atomic_usize_table.fetch_add, EmbedAtomicUsizeFetchAddCfg, .word = &s_popped_sum,
                             .value = (embed_atomic_usize_value)out);
            (void)EMBED_CALL(embed_atomic_usize_table.fetch_add, EmbedAtomicUsizeFetchAddCfg, .word = &s_popped_count,
                             .value = 1u);
        }
    }
}

void test_pushes_and_pops_mixed_on_many_threads_keep_every_element_once(void)
{
    TEST_ASSERT_TRUE(mmgr_host_threads_run(crowd_mixes, CROWD_THREADS));
    TEST_ASSERT_EQUAL_size_t(
        0u, (size_t)EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg, .word = &s_refused_pushes));
    const size_t total = CROWD_THREADS * CROWD_EACH;
    const size_t left = EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_crowd);
    TEST_ASSERT_EQUAL_size_t(total, left + (size_t)EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg,
                                                              .word = &s_popped_count));
    uint64_t sum = (uint64_t)EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg, .word = &s_popped_sum);
    for (size_t index = 0u; index < left; index++)
    {
        uint64_t out = 0u;
        TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_SITE, .vector = s_crowd,
                                    .index = index, .dst = &out));
        sum += out;
    }
    TEST_ASSERT_EQUAL_UINT64((uint64_t)total * (total + 1u) / 2u, sum);
}

static embed_bool stub_push(uint64_t value)
{
    return EMBED_CALL(memoria_vector.push_back, MemoriaVectorCfg, THROUGH_STUB, .vector = s_numbers, .src = &value);
}

static uint64_t stub_at(size_t index)
{
    uint64_t out = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_STUB, .vector = s_numbers, .index = index,
                                .dst = &out));
    return out;
}

void test_a_refused_read_in_a_growth_refuses_the_push_and_gives_the_new_cell_back(void)
{
    TEST_ASSERT_TRUE(stub_push(21u));
    s_refuse_read = EMBED_TRUE;
    TEST_ASSERT_FALSE(stub_push(22u));
    TEST_ASSERT_EQUAL_size_t(1u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    TEST_ASSERT_EQUAL_size_t(sizeof(uint64_t), mmgr_owner_in_use(s_elements));
    TEST_ASSERT_EQUAL_UINT64(21u, stub_at(0u));
}

void test_a_refused_write_in_a_growth_refuses_the_push_and_gives_the_new_cell_back(void)
{
    TEST_ASSERT_TRUE(stub_push(31u));
    s_refuse_write = EMBED_TRUE;
    TEST_ASSERT_FALSE(stub_push(32u));
    TEST_ASSERT_EQUAL_size_t(1u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    TEST_ASSERT_EQUAL_size_t(sizeof(uint64_t), mmgr_owner_in_use(s_elements));
    TEST_ASSERT_EQUAL_UINT64(31u, stub_at(0u));
}

void test_a_refused_read_refuses_at_and_leaves_dst(void)
{
    TEST_ASSERT_TRUE(stub_push(41u));
    uint64_t out = 6u;
    s_refuse_read = EMBED_TRUE;
    TEST_ASSERT_FALSE(
        EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_STUB, .vector = s_numbers, .index = 0u, .dst = &out));
    TEST_ASSERT_EQUAL_UINT64(6u, out);
}

void test_a_refused_write_refuses_set_and_leaves_the_element(void)
{
    TEST_ASSERT_TRUE(stub_push(51u));
    const uint64_t changed = 52u;
    s_refuse_write = EMBED_TRUE;
    TEST_ASSERT_FALSE(EMBED_CALL(memoria_vector.set, MemoriaVectorCfg, THROUGH_STUB, .vector = s_numbers, .index = 0u,
                                 .src = &changed));
    TEST_ASSERT_EQUAL_UINT64(51u, stub_at(0u));
}

void test_a_refused_write_refuses_a_push_with_room_and_the_count_holds(void)
{
    TEST_ASSERT_TRUE(stub_push(61u));
    TEST_ASSERT_TRUE(stub_push(62u));
    TEST_ASSERT_TRUE(stub_push(63u));
    s_refuse_write = EMBED_TRUE;
    TEST_ASSERT_FALSE(stub_push(64u));
    TEST_ASSERT_EQUAL_size_t(3u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    TEST_ASSERT_TRUE(stub_push(65u));
    TEST_ASSERT_EQUAL_UINT64(65u, stub_at(3u));
}

void test_a_pop_whose_read_is_refused_answers_false_and_the_element_is_gone(void)
{
    TEST_ASSERT_TRUE(stub_push(71u));
    TEST_ASSERT_TRUE(stub_push(72u));
    uint64_t out = 0u;
    s_refuse_read = EMBED_TRUE;
    TEST_ASSERT_FALSE(
        EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_STUB, .vector = s_numbers, .dst = &out));
    TEST_ASSERT_EQUAL_size_t(1u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    TEST_ASSERT_EQUAL_UINT64(71u, stub_at(0u));
}

void test_a_release_that_the_guard_refuses_keeps_the_cell_and_the_elements(void)
{
    TEST_ASSERT_TRUE(stub_push(81u));
    s_refuse_give = EMBED_TRUE;
    TEST_ASSERT_FALSE(EMBED_CALL(memoria_vector.release, MemoriaVectorCfg, THROUGH_STUB, .vector = s_numbers));
    TEST_ASSERT_EQUAL_size_t(1u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    TEST_ASSERT_EQUAL_size_t(sizeof(uint64_t), mmgr_owner_in_use(s_elements));
    TEST_ASSERT_EQUAL_UINT64(81u, stub_at(0u));
}

void test_a_push_whose_count_falls_under_it_starts_again_at_the_new_count(void)
{
    TEST_ASSERT_TRUE(stub_push(10u));
    TEST_ASSERT_TRUE(stub_push(11u));
    TEST_ASSERT_TRUE(stub_push(12u));
    // The pop inside the write takes the count from 3 to 2, so the CAS of the push finds 2 and the push starts again
    s_pop_inside_write = EMBED_TRUE;
    TEST_ASSERT_TRUE(stub_push(99u));
    TEST_ASSERT_EQUAL_UINT64(12u, s_popped_inside);
    TEST_ASSERT_EQUAL_size_t(3u, EMBED_CALL(memoria_vector.size, MemoriaVectorCfg, .vector = s_numbers));
    TEST_ASSERT_EQUAL_UINT64(10u, stub_at(0u));
    TEST_ASSERT_EQUAL_UINT64(11u, stub_at(1u));
    TEST_ASSERT_EQUAL_UINT64(99u, stub_at(2u));
}

static embed_bool s_push_answer;
static embed_bool s_read_answer;
static uint64_t s_read_under_push;

// The first thread pushes, and its write holds the bit of the element. The second reads that element meanwhile
static void under_push_each(void)
{
    if (EMBED_CALL(embed_atomic_usize_table.fetch_add, EmbedAtomicUsizeFetchAddCfg, .word = &s_next_thread,
                   .value = 1u) == 0u)
    {
        s_push_answer = stub_push(55u);
        return;
    }
    while (flag_of(&s_write_held) == 0u)
    {
        mmgr_host_threads_yield();
    }
    flag_set(&s_reader_began, 1u);
    s_read_answer = EMBED_CALL(memoria_vector.at, MemoriaVectorCfg, THROUGH_STUB, .vector = s_numbers, .index = 1u,
                               .dst = &s_read_under_push);
}

void test_a_read_of_the_element_under_a_push_waits_for_the_push_and_reads_its_value(void)
{
    TEST_ASSERT_TRUE(stub_push(10u));
    TEST_ASSERT_TRUE(stub_push(11u));
    uint64_t out = 0u;
    TEST_ASSERT_TRUE(
        EMBED_CALL(memoria_vector.pop_back, MemoriaVectorCfg, THROUGH_STUB, .vector = s_numbers, .dst = &out));
    // The count is 1 and the capacity is 2, so the push writes element 1 with no growth
    flag_set(&s_hold_write, 1u);
    TEST_ASSERT_TRUE(mmgr_host_threads_run(under_push_each, 2u));
    TEST_ASSERT_TRUE(s_push_answer);
    TEST_ASSERT_TRUE_MESSAGE(s_read_answer, "the read waits out the push, which then holds element 1");
    TEST_ASSERT_EQUAL_UINT64(55u, s_read_under_push);
}
