/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_locus_carcerum_movable.c
 * @brief Cells named by a const identity and reached only through the guard: what the calls answer, what a
 *        move and the mover do, and no write lost while another thread moves the cells written to.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-31
 *
 * @note Each writing thread owns one cell, fills it with a count it raises every round and reads the
 *       count back on the next round. A write that landed in the copy a move left behind reads back
 *       as the count before it, and the number of such reads is what the threaded case holds at zero.
 * @note The mover's cases run mmgr_movable_run on the test's own thread, with a thread_wait that plays a
 *       script and answers EMBED_FALSE at its end, so each reading lands where the case says it does.
 */
#include "locus_carcerum/locus_carcerum.h"

#include "embed_atomic.h"
#include "host_threads.h"
#include "unity.h"

#include <string.h>

#define MOVING_BYTES 4096u
#define MOVING_CELLS 8u
#define MOVING_WIDTH 2u
#define MOVING_ROWS 3u
#define SECURE_BYTES 1024u
#define NARROW_BYTES 2048u
#define CROWD_BYTES 262144u
#define CROWD_WRITERS 7u
#define CROWD_WIDTH 4u
#define CROWD_ROUNDS 4000u
#define CROWD_CELL_WORDS 32u

ParsMemoriaeInternae(moving, MOVING_BYTES);
ParsMemoriaeInternae(secure, SECURE_BYTES);
ParsMemoriaeInternae(narrow, NARROW_BYTES);
ParsMemoriaeInternae(bare, SECURE_BYTES);
ParsMemoriaeInternae(crowd, CROWD_BYTES);
ParsMemoriaeInternae(moving_option, MMGR_MOVABLE_STORAGE_BYTES(MOVING_CELLS, MOVING_WIDTH, MOVING_ROWS));
ParsMemoriaeInternae(secure_option, MMGR_MOVABLE_STORAGE_BYTES(2u, 1u, 0u));
ParsMemoriaeInternae(narrow_option, MMGR_MOVABLE_STORAGE_BYTES(MOVING_CELLS, 1u, 0u));
ParsMemoriaeInternae(crowd_option, MMGR_MOVABLE_STORAGE_BYTES(CROWD_WRITERS + 1u, CROWD_WIDTH, 0u));
LocusCarcerum(plain, MMGR_MINIMUM_SECURITY(moving));
LocusCarcerum(wiped, MMGR_MAXIMUM_SECURITY(secure));
LocusCarcerum(single, MMGR_MINIMUM_SECURITY(narrow));
LocusCarcerum(unassigned, MMGR_MINIMUM_SECURITY(bare));
LocusCarcerum(crowded, MMGR_MINIMUM_SECURITY(crowd));

static CellOwner s_mover = MMGR_CELL_OWNER("mover");
static CellOwner s_other = MMGR_CELL_OWNER("other");
static embed_atomic_u32 s_next_index = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_writers_done = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_moves = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_lost = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_refused = EMBED_ATOMIC_INIT(0u);

// What the scripted thread_wait plays: the ticks it answers EMBED_TRUE to, what it runs on one of them, and every
// wait it was asked for
static unsigned int s_ticks_left;
static unsigned int s_tick;
static unsigned int s_turn_on_tick;
static void (*s_turn)(void);
static uint32_t s_last_wait;
static uint32_t s_most_wait;
static unsigned int s_moving_told;
static unsigned int s_moving_ended;
static embed_bool s_started_answer;
static unsigned int s_started;
static void (*s_started_run)(void *context);
static void *s_started_context;
static size_t s_thread_id;
static embed_atomic_usize s_hold_turnstile = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_turnstile_taken = EMBED_ATOMIC_INIT(0u);
static void *s_take_when_misnamed;
static unsigned int s_misnamed;

static embed_bool script_wait(uint32_t milliseconds)
{
    s_last_wait = milliseconds;
    s_most_wait = (milliseconds > s_most_wait) ? milliseconds : s_most_wait;
    if (s_ticks_left == 0u)
    {
        return EMBED_FALSE;
    }
    s_ticks_left -= 1u;
    s_tick += 1u;
    if ((s_turn != NULL) && (s_tick == s_turn_on_tick))
    {
        s_turn();
    }
    return EMBED_TRUE;
}

static void script_moving(embed_bool moving)
{
    if (moving)
    {
        s_moving_told += 1u;
    }
    else
    {
        s_moving_ended += 1u;
    }
}

static embed_bool script_started(void (*run)(void *context), void *context)
{
    s_started += 1u;
    s_started_run = run;
    s_started_context = context;
    return s_started_answer;
}

static size_t script_id(void)
{
    return s_thread_id;
}

// Told of a give naming the wrong owner, from inside the give's turnstile. On the case's say-so it stands there until
// told to go, or takes a cell of its own while the turnstile stands taken
void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
    s_misnamed += 1u;
    embed_atomic_usize_store(&s_turnstile_taken, 1u);
    while (embed_atomic_usize_load(&s_hold_turnstile) != 0u)
    {
    }
    if (s_take_when_misnamed != NULL)
    {
        s_take_when_misnamed = plain.moving.persistent_buf_alloc(16u, &s_mover);
    }
}

// The option assigned over its storage, zeroed, with the script's calls
static void option_assigned(void)
{
    memset(mmgr_pars_storage_moving_option, 0, moving_option_bytes);
    TEST_ASSERT_TRUE(plain.moving.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_moving_option,
                                                                       .storage_bytes = moving_option_bytes,
                                                                       .cells = MOVING_CELLS,
                                                                       .width = MOVING_WIDTH,
                                                                       .rows = MOVING_ROWS,
                                                                       .thread_id = script_id,
                                                                       .thread_wait = script_wait,
                                                                       .thread_moving = script_moving}));
}

void setUp(void)
{
    if (embed_atomic_usize_load(&s_mover.place) == 0u)
    {
        TEST_ASSERT_TRUE(mmgr_owner_register(&s_mover));
        TEST_ASSERT_TRUE(mmgr_owner_register(&s_other));
    }
    embed_atomic_usize_store(&s_mover.in_use, 0u);
    embed_atomic_usize_store(&s_other.in_use, 0u);
    mmgr_cellblock_emptied(&plain_moving_ctx);
    mmgr_cellblock_emptied(&wiped_secure_ctx);
    mmgr_cellblock_emptied(&single_narrow_ctx);
    mmgr_cellblock_emptied(&unassigned_bare_ctx);
    mmgr_cellblock_emptied(&crowded_crowd_ctx);
    embed_atomic_u32_store(&s_next_index, 0u);
    embed_atomic_usize_store(&s_writers_done, 0u);
    embed_atomic_usize_store(&s_moves, 0u);
    embed_atomic_usize_store(&s_lost, 0u);
    embed_atomic_usize_store(&s_refused, 0u);
    embed_atomic_usize_store(&s_hold_turnstile, 0u);
    embed_atomic_usize_store(&s_turnstile_taken, 0u);
    s_ticks_left = 0u;
    s_tick = 0u;
    s_turn_on_tick = 0u;
    s_turn = NULL;
    s_last_wait = 0u;
    s_most_wait = 0u;
    s_moving_told = 0u;
    s_moving_ended = 0u;
    s_started_answer = EMBED_TRUE;
    s_started = 0u;
    s_started_run = NULL;
    s_started_context = NULL;
    s_thread_id = 0u;
    s_take_when_misnamed = NULL;
    s_misnamed = 0u;
    option_assigned();
}

void tearDown(void)
{
}

// A movable cell of size bytes, every byte of it mark
static const CarcerMovableSlot *marked_sized(size_t size, unsigned char mark)
{
    static unsigned char s_bytes[512];
    const CarcerMovableSlot *const cell = plain.moving.movable_buf_alloc(size, &s_mover);

    TEST_ASSERT_NOT_NULL(cell);
    memset(s_bytes, mark, size);
    TEST_ASSERT_TRUE(plain.moving.movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = s_bytes, .bytes = size}));
    return cell;
}

static const CarcerMovableSlot *marked(unsigned char mark)
{
    return marked_sized(64u, mark);
}

static void assert_marked(const CarcerMovableSlot *cell, size_t size, unsigned char mark)
{
    static unsigned char s_bytes[512];

    memset(s_bytes, 0, sizeof(s_bytes));
    TEST_ASSERT_TRUE(plain.moving.movable_buf_read(&(CarcerMovableCfg){.cell = cell, .dst = s_bytes, .bytes = size}));
    for (size_t at = 0u; at < size; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(mark, s_bytes[at], "a move keeps every byte");
    }
}

void test_a_cellblock_with_no_option_takes_and_gives_and_answers_no_identity(void)
{
    void *const cell = unassigned.bare.persistent_buf_alloc(64u, &s_mover);

    TEST_ASSERT_NOT_NULL(cell);
    unassigned.bare.persistent_buf_release(cell, &s_mover);
    TEST_ASSERT_NULL(unassigned.bare.movable_buf_alloc(64u, &s_mover));
    TEST_ASSERT_FALSE_MESSAGE(mmgr_movable_buf_move(&unassigned_bare_ctx), "nothing moves without the option");
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_movable_queue_depth(&unassigned_bare_ctx));
}

void test_the_option_is_assigned_once(void)
{
    TEST_ASSERT_FALSE(plain.moving.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_moving_option,
                                                                        .storage_bytes = moving_option_bytes,
                                                                        .cells = MOVING_CELLS,
                                                                        .width = MOVING_WIDTH}));
}

void test_the_option_refuses_short_storage_and_zero_sizes(void)
{
    TEST_ASSERT_FALSE(single.narrow.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_narrow_option,
                                                                         .storage_bytes = narrow_option_bytes - 1u,
                                                                         .cells = MOVING_CELLS,
                                                                         .width = 1u}));
    TEST_ASSERT_FALSE(single.narrow.movable_init(&(CarcerMovableInitCfg){
        .storage = mmgr_pars_storage_narrow_option, .storage_bytes = narrow_option_bytes, .cells = 0u, .width = 1u}));
    TEST_ASSERT_FALSE(single.narrow.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_narrow_option,
                                                                         .storage_bytes = narrow_option_bytes,
                                                                         .cells = MOVING_CELLS,
                                                                         .width = 0u}));
    TEST_ASSERT_FALSE(single.narrow.movable_init(&(CarcerMovableInitCfg){
        .storage = NULL, .storage_bytes = narrow_option_bytes, .cells = MOVING_CELLS, .width = 1u}));
    TEST_ASSERT_NULL_MESSAGE(single.narrow.movable_buf_alloc(64u, &s_mover), "a refused init assigns nothing");
}

void test_the_program_gives_the_mover_its_thread_at_init(void)
{
    memset(mmgr_pars_storage_narrow_option, 0, narrow_option_bytes);
    TEST_ASSERT_TRUE(single.narrow.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_narrow_option,
                                                                        .storage_bytes = narrow_option_bytes,
                                                                        .cells = MOVING_CELLS,
                                                                        .width = 1u,
                                                                        .thread_started = script_started,
                                                                        .thread_wait = script_wait}));
    TEST_ASSERT_EQUAL_UINT(1u, s_started);
    TEST_ASSERT_TRUE_MESSAGE(s_started_context == (void *)&single_narrow_ctx, "the thread is told its cellblock");
    s_ticks_left = 0u;
    s_started_run(s_started_context);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(MMGR_MOVABLE_READ_MS, s_last_wait, "and what it runs is the mover");
}

void test_a_thread_that_does_not_start_leaves_the_cells_working_and_nothing_moving(void)
{
    s_started_answer = EMBED_FALSE;
    memset(mmgr_pars_storage_narrow_option, 0, narrow_option_bytes);
    TEST_ASSERT_FALSE(single.narrow.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_narrow_option,
                                                                         .storage_bytes = narrow_option_bytes,
                                                                         .cells = MOVING_CELLS,
                                                                         .width = 1u,
                                                                         .thread_started = script_started,
                                                                         .thread_wait = script_wait}));
    TEST_ASSERT_NOT_NULL(single.narrow.movable_buf_alloc(64u, &s_mover));
}

void test_a_thread_is_not_started_without_its_wait(void)
{
    memset(mmgr_pars_storage_narrow_option, 0, narrow_option_bytes);
    TEST_ASSERT_TRUE(single.narrow.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_narrow_option,
                                                                        .storage_bytes = narrow_option_bytes,
                                                                        .cells = MOVING_CELLS,
                                                                        .width = 1u,
                                                                        .thread_started = script_started}));
    TEST_ASSERT_EQUAL_UINT(0u, s_started);
}

void test_a_movable_take_answers_an_identity_whose_bytes_read_back(void)
{
    const CarcerMovableSlot *const cell = marked(0x5Au);

    assert_marked(cell, 64u, 0x5Au);
    TEST_ASSERT_EQUAL_size_t(64u, (size_t)embed_atomic_usize_load(&s_mover.in_use));
}

void test_a_movable_take_the_cellblock_cannot_meet_answers_no_identity(void)
{
    TEST_ASSERT_NULL(plain.moving.movable_buf_alloc(MOVING_BYTES * 2u, &s_mover));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_mover.in_use), "nothing is counted");
}

void test_a_movable_take_a_full_table_cannot_name_gives_its_cell_back(void)
{
    for (unsigned int at = 0u; at < MOVING_CELLS; at += 1u)
    {
        TEST_ASSERT_NOT_NULL(plain.moving.movable_buf_alloc(64u, &s_mover));
    }
    const size_t counted = (size_t)embed_atomic_usize_load(&s_mover.in_use);

    TEST_ASSERT_NULL(plain.moving.movable_buf_alloc(64u, &s_mover));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(counted, (size_t)embed_atomic_usize_load(&s_mover.in_use), "the cell went back");
}

void test_a_read_or_a_write_past_the_cells_end_is_refused(void)
{
    const CarcerMovableSlot *const cell = marked(0x21u);
    unsigned char bytes[128];

    memset(bytes, 0x77, sizeof(bytes));
    TEST_ASSERT_FALSE(plain.moving.movable_buf_read(&(CarcerMovableCfg){.cell = cell, .dst = bytes, .bytes = 65u}));
    TEST_ASSERT_FALSE(
        plain.moving.movable_buf_read(&(CarcerMovableCfg){.cell = cell, .offset = 60u, .dst = bytes, .bytes = 5u}));
    TEST_ASSERT_FALSE(plain.moving.movable_buf_read(&(CarcerMovableCfg){.cell = cell, .offset = 65u, .dst = bytes}));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x77u, bytes[0], "nothing was copied");
    TEST_ASSERT_FALSE(plain.moving.movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = bytes, .bytes = 65u}));
    assert_marked(cell, 64u, 0x21u);
}

void test_a_read_and_a_write_land_at_their_offset(void)
{
    const CarcerMovableSlot *const cell = marked(0x31u);
    const unsigned char two[2] = {0xA1u, 0xA2u};
    unsigned char back[4] = {0u, 0u, 0u, 0u};

    TEST_ASSERT_TRUE(
        plain.moving.movable_buf_write(&(CarcerMovableCfg){.cell = cell, .offset = 62u, .src = two, .bytes = 2u}));
    TEST_ASSERT_TRUE(
        plain.moving.movable_buf_read(&(CarcerMovableCfg){.cell = cell, .offset = 60u, .dst = back, .bytes = 4u}));
    TEST_ASSERT_EQUAL_UINT8(0x31u, back[1]);
    TEST_ASSERT_EQUAL_UINT8(0xA1u, back[2]);
    TEST_ASSERT_EQUAL_UINT8(0xA2u, back[3]);
}

void test_compare_orders_a_cell_against_bytes_and_against_another_cell(void)
{
    const CarcerMovableSlot *const low = marked(0x10u);
    const CarcerMovableSlot *const high = marked(0x20u);
    unsigned char bytes[64];

    memset(bytes, 0x10, sizeof(bytes));
    TEST_ASSERT_EQUAL_INT(
        0, (int)plain.moving.movable_buf_compare(&(CarcerMovableCfg){.cell = low, .src = bytes, .bytes = 64u}));
    TEST_ASSERT_TRUE(plain.moving.movable_buf_compare(&(CarcerMovableCfg){.cell = high, .src = bytes, .bytes = 64u}) >
                     0);
    TEST_ASSERT_TRUE(
        plain.moving.movable_buf_compare(&(CarcerMovableCfg){.cell = low, .other_cell = high, .bytes = 64u}) < 0);
    TEST_ASSERT_EQUAL_INT(0, (int)plain.moving.movable_buf_compare(&(CarcerMovableCfg){
                                 .cell = low, .other_cell = low, .offset = 4u, .other_offset = 8u, .bytes = 8u}));
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        1, (int)plain.moving.movable_buf_compare(&(CarcerMovableCfg){.cell = low, .src = bytes, .bytes = 65u}),
        "past the cell's end");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1,
                                  (int)plain.moving.movable_buf_compare(&(CarcerMovableCfg){
                                      .cell = low, .other_cell = high, .other_offset = 8u, .bytes = 60u}),
                                  "past the other's");
}

void test_measure_answers_the_bytes_to_the_next_zero_or_the_end(void)
{
    const CarcerMovableSlot *const cell = marked(0x41u);
    const unsigned char zero = 0u;

    TEST_ASSERT_EQUAL_size_t_MESSAGE(64u, plain.moving.movable_buf_measure(&(CarcerMovableCfg){.cell = cell}),
                                     "no zero");
    TEST_ASSERT_TRUE(
        plain.moving.movable_buf_write(&(CarcerMovableCfg){.cell = cell, .offset = 10u, .src = &zero, .bytes = 1u}));
    TEST_ASSERT_EQUAL_size_t(10u, plain.moving.movable_buf_measure(&(CarcerMovableCfg){.cell = cell}));
    TEST_ASSERT_EQUAL_size_t(6u, plain.moving.movable_buf_measure(&(CarcerMovableCfg){.cell = cell, .offset = 4u}));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(
        0u, plain.moving.movable_buf_measure(&(CarcerMovableCfg){.cell = cell, .offset = 65u}), "past the end");
}

void test_a_released_identity_reads_nothing(void)
{
    const CarcerMovableSlot *const cell = marked(0x09u);
    unsigned char byte = 0u;

    TEST_ASSERT_TRUE(plain.moving.movable_buf_release(cell, &s_mover));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&s_mover.in_use));
    TEST_ASSERT_FALSE(plain.moving.movable_buf_read(&(CarcerMovableCfg){.cell = cell, .dst = &byte, .bytes = 1u}));
    TEST_ASSERT_FALSE_MESSAGE(plain.moving.movable_buf_release(cell, &s_mover), "a second release finds it free");
    TEST_ASSERT_FALSE_MESSAGE(plain.moving.movable_buf_release(NULL, &s_mover), "and no identity releases nothing");
}

void test_release_naming_another_owner_changes_nothing_and_is_told(void)
{
    const CarcerMovableSlot *const cell = marked(0x0Au);

    TEST_ASSERT_FALSE(plain.moving.movable_buf_release(cell, &s_other));
    TEST_ASSERT_EQUAL_UINT(1u, s_misnamed);
    assert_marked(cell, 64u, 0x0Au);
    TEST_ASSERT_TRUE(plain.moving.movable_buf_release(cell, &s_mover));
}

void test_a_released_place_in_the_table_is_taken_again_lowest_first(void)
{
    const CarcerMovableSlot *const first = marked(0x0Cu);
    const CarcerMovableSlot *const second = marked(0x0Du);
    const CarcerMovableSlot *const third = marked(0x0Eu);

    TEST_ASSERT_TRUE(plain.moving.movable_buf_release(second, &s_mover));
    TEST_ASSERT_TRUE(plain.moving.movable_buf_release(first, &s_mover));
    TEST_ASSERT_TRUE_MESSAGE(marked(0x0Fu) == first, "the lowest free place");
    TEST_ASSERT_TRUE_MESSAGE(marked(0x10u) == second, "and the next");
    TEST_ASSERT_TRUE_MESSAGE(marked(0x12u) > third, "then past the ones taken");
}

void test_a_take_wraps_and_finds_a_place_its_search_start_has_passed(void)
{
    const CarcerMovableSlot *cells[MOVING_CELLS];

    for (unsigned int at = 0u; at < MOVING_CELLS; at += 1u)
    {
        cells[at] = plain.moving.movable_buf_alloc(16u, &s_mover);
        TEST_ASSERT_NOT_NULL(cells[at]);
    }
    TEST_ASSERT_TRUE(plain.moving.movable_buf_release(cells[MOVING_CELLS - 1u], &s_mover));
    TEST_ASSERT_TRUE(plain.moving.movable_buf_release(cells[0], &s_mover));
    // The start stands at the first release's place until a later take raises it past the second
    TEST_ASSERT_TRUE(plain.moving.movable_buf_alloc(16u, &s_mover) == cells[0]);
    TEST_ASSERT_TRUE_MESSAGE(plain.moving.movable_buf_alloc(16u, &s_mover) == cells[MOVING_CELLS - 1u],
                             "the search reads every place before it refuses");
}

void test_a_move_takes_the_free_cell_below_and_the_identity_stands(void)
{
    void *const low = plain.moving.persistent_buf_alloc(64u, &s_mover);
    const CarcerMovableSlot *const cell = marked(0x22u);
    const size_t gap = plain.moving.buf_available();

    plain.moving.persistent_buf_release(low, &s_mover);
    TEST_ASSERT_TRUE_MESSAGE(mmgr_movable_buf_move(&plain_moving_ctx), "a free cell stands below it");
    assert_marked(cell, 64u, 0x22u);
    TEST_ASSERT_TRUE_MESSAGE(mmgr_owner_of_buf(&plain_moving_ctx, low) == &s_mover, "the free cell is taken");
    TEST_ASSERT_TRUE_MESSAGE(plain.moving.buf_available() > gap, "the tier's end comes back to the gap");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(64u, (size_t)embed_atomic_usize_load(&s_mover.in_use), "one cell is counted");
    TEST_ASSERT_TRUE(plain.moving.movable_buf_release(cell, &s_mover));
    TEST_ASSERT_EQUAL_size_t(MOVING_BYTES, plain.moving.buf_available());
}

void test_a_move_finds_nothing_where_no_free_cell_stands_below(void)
{
    const CarcerMovableSlot *const cell = marked(0x44u);

    TEST_ASSERT_FALSE(mmgr_movable_buf_move(&plain_moving_ctx));
    assert_marked(cell, 64u, 0x44u);
}

void test_a_move_never_goes_upward(void)
{
    const CarcerMovableSlot *const cell = marked(0x45u);
    void *const above = plain.moving.persistent_buf_alloc(64u, &s_mover);
    void *const held = plain.moving.persistent_buf_alloc(64u, &s_mover);

    plain.moving.persistent_buf_release(above, &s_mover);
    TEST_ASSERT_FALSE_MESSAGE(mmgr_movable_buf_move(&plain_moving_ctx), "the only free cell stands above it");
    assert_marked(cell, 64u, 0x45u);
    TEST_ASSERT_NOT_NULL(held);
}

void test_a_move_takes_the_highest_cell_first(void)
{
    void *const low = plain.moving.persistent_buf_alloc(64u, &s_mover);
    const CarcerMovableSlot *const lower = marked(0x55u);
    const CarcerMovableSlot *const higher = marked(0x66u);
    const size_t gap = plain.moving.buf_available();

    plain.moving.persistent_buf_release(low, &s_mover);
    TEST_ASSERT_TRUE(mmgr_movable_buf_move(&plain_moving_ctx));
    TEST_ASSERT_TRUE_MESSAGE(plain.moving.buf_available() > gap, "only the highest going gives the end back");
    assert_marked(lower, 64u, 0x55u);
    assert_marked(higher, 64u, 0x66u);
}

void test_a_smaller_cell_under_one_no_free_cell_takes_still_moves(void)
{
    void *const low = plain.moving.persistent_buf_alloc(64u, &s_mover);
    const CarcerMovableSlot *const small = marked_sized(32u, 0x61u);
    const CarcerMovableSlot *const large = marked_sized(256u, 0x62u);

    plain.moving.persistent_buf_release(low, &s_mover);
    TEST_ASSERT_TRUE_MESSAGE(mmgr_movable_buf_move(&plain_moving_ctx), "the smaller cell fits the free cell");
    TEST_ASSERT_TRUE_MESSAGE(mmgr_owner_of_buf(&plain_moving_ctx, low) == &s_mover, "and takes it");
    assert_marked(small, 32u, 0x61u);
    assert_marked(large, 256u, 0x62u);
}

void test_moves_run_until_none_answers_stand_as_they_left_things(void)
{
    void *const first = plain.moving.persistent_buf_alloc(64u, &s_mover);
    const CarcerMovableSlot *const one = marked(0x71u);
    void *const second = plain.moving.persistent_buf_alloc(64u, &s_mover);
    const CarcerMovableSlot *const two = marked(0x72u);
    const size_t gap = plain.moving.buf_available();

    plain.moving.persistent_buf_release(first, &s_mover);
    plain.moving.persistent_buf_release(second, &s_mover);
    unsigned int moved = 0u;

    while (mmgr_movable_buf_move(&plain_moving_ctx) && (moved < 16u))
    {
        moved += 1u;
    }
    TEST_ASSERT_TRUE_MESSAGE(moved < 16u, "moving comes to an end");
    const size_t settled = plain.moving.buf_available();

    TEST_ASSERT_FALSE(mmgr_movable_buf_move(&plain_moving_ctx));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(settled, plain.moving.buf_available(), "another call moves nothing");
    assert_marked(one, 64u, 0x71u);
    assert_marked(two, 64u, 0x72u);
    TEST_ASSERT_TRUE_MESSAGE(settled > gap, "the tier's end came back to the gap");
}

void test_a_move_and_a_release_on_a_wiping_cellblock_wipe_what_they_leave(void)
{
    memset(mmgr_pars_storage_secure_option, 0, secure_option_bytes);
    TEST_ASSERT_TRUE(wiped.secure.movable_init(&(CarcerMovableInitCfg){
        .storage = mmgr_pars_storage_secure_option, .storage_bytes = secure_option_bytes, .cells = 2u, .width = 1u}));
    unsigned char *const low = wiped.secure.persistent_buf_alloc(64u, &s_mover);
    const CarcerMovableSlot *const cell = wiped.secure.movable_buf_alloc(64u, &s_mover);
    unsigned char *const next = wiped.secure.persistent_buf_alloc(64u, &s_mover);
    // Three cells of one size stand side by side, and the movable one is the middle one
    unsigned char *const left = low + ((size_t)(next - low) / 2u);
    unsigned char bytes[64];

    memset(bytes, 0x7E, sizeof(bytes));
    TEST_ASSERT_TRUE(wiped.secure.movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = bytes, .bytes = 64u}));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x7Eu, left[0], "the case reads where the cell stands");
    wiped.secure.persistent_buf_release(low, &s_mover);
    TEST_ASSERT_TRUE(mmgr_movable_buf_move(&wiped_secure_ctx));
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, left[at], "the copy left behind is wiped");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x7Eu, low[at], "and the bytes stand in the cell it moved to");
    }
    TEST_ASSERT_TRUE(wiped.secure.movable_buf_release(cell, &s_mover));
    for (unsigned int at = 0u; at < 64u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, low[at], "a release wipes the cell it gives back");
    }
}

void test_fragmentation_reads_the_bytes_occupied_that_no_payload_holds(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_cellblock_fragmentation(&plain_moving_ctx), "nothing occupied");
    void *const low = plain.moving.persistent_buf_alloc(512u, &s_mover);
    void *const high = plain.moving.persistent_buf_alloc(512u, &s_mover);

    const size_t whole = mmgr_cellblock_fragmentation(&plain_moving_ctx);
    TEST_ASSERT_TRUE_MESSAGE(whole < 10u, "with no hole only the cells' words read");
    plain.moving.persistent_buf_release(low, &s_mover);
    TEST_ASSERT_TRUE_MESSAGE(mmgr_cellblock_fragmentation(&plain_moving_ctx) >= 40u, "half the tier is a hole");
    plain.moving.persistent_buf_release(high, &s_mover);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_cellblock_fragmentation(&plain_moving_ctx), "and the tier is gone");
}

void test_the_temporary_tier_reads_no_fragmentation(void)
{
    void *const persistent = plain.moving.persistent_buf_alloc(512u, &s_mover);
    const size_t whole = mmgr_cellblock_fragmentation(&plain_moving_ctx);
    void *const hole = plain.moving.temporary_buf_alloc(1024u, &s_mover);
    void *const below = plain.moving.temporary_buf_alloc(16u, &s_mover);

    TEST_ASSERT_NOT_NULL(hole);
    TEST_ASSERT_NOT_NULL(below);
    plain.moving.persistent_buf_release(hole, &s_mover);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(whole, mmgr_cellblock_fragmentation(&plain_moving_ctx),
                                     "a hole in the temporary tier is no work for a sort");
    plain.moving.persistent_buf_release(below, &s_mover);
    plain.moving.persistent_buf_release(persistent, &s_mover);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellblock_fragmentation(&plain_moving_ctx));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&plain_moving_ctx.persistent_in_use),
                                     "every persistent cell is counted back");
}

void test_a_cell_counts_to_the_tier_it_lies_in(void)
{
    uint8_t *const hole = plain.moving.temporary_buf_alloc(1024u, &s_mover);

    TEST_ASSERT_NOT_NULL(hole);
    TEST_ASSERT_NOT_NULL(plain.moving.temporary_buf_alloc(64u, &s_mover));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&plain_moving_ctx.persistent_in_use));
    while (plain.moving.persistent_buf_alloc(64u, &s_mover) != NULL)
    {
    }
    const size_t persistent = (size_t)embed_atomic_usize_load(&plain_moving_ctx.persistent_in_use);

    TEST_ASSERT_TRUE(persistent != 0u);
    plain.moving.persistent_buf_release(hole, &s_mover);
    uint8_t *const reached = plain.moving.persistent_buf_alloc(512u, &s_mover);

    TEST_ASSERT_NOT_NULL(reached);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(persistent, (size_t)embed_atomic_usize_load(&plain_moving_ctx.persistent_in_use),
                                     "a persistent take lying in the temporary tier counts to the temporary tier");
    plain.moving.persistent_buf_release(reached, &s_mover);
    TEST_ASSERT_EQUAL_size_t(persistent, (size_t)embed_atomic_usize_load(&plain_moving_ctx.persistent_in_use));
}

void test_each_cellblock_keeps_its_own_running_total(void)
{
    void *const here = plain.moving.persistent_buf_alloc(512u, &s_mover);
    void *const there = unassigned.bare.persistent_buf_alloc(512u, &s_mover);

    TEST_ASSERT_EQUAL_size_t(512u, (size_t)embed_atomic_usize_load(&plain_moving_ctx.in_use));
    TEST_ASSERT_EQUAL_size_t(512u, (size_t)embed_atomic_usize_load(&unassigned_bare_ctx.in_use));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1024u, (size_t)embed_atomic_usize_load(&s_mover.in_use), "the owner spans both");
    plain.moving.persistent_buf_release(here, &s_mover);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&plain_moving_ctx.in_use));
    TEST_ASSERT_EQUAL_size_t(512u, (size_t)embed_atomic_usize_load(&unassigned_bare_ctx.in_use));
    unassigned.bare.persistent_buf_release(there, &s_mover);
}

void test_the_running_total_follows_the_temporary_tier_and_a_growth(void)
{
    const size_t mark = plain.moving.temporary_buf_mark();
    void *const cell = plain.moving.temporary_buf_alloc(64u, &s_mover);

    TEST_ASSERT_EQUAL_size_t(64u, (size_t)embed_atomic_usize_load(&plain_moving_ctx.in_use));
    TEST_ASSERT_NOT_NULL(plain.moving.buf_grow(cell, 128u, &s_mover));
    TEST_ASSERT_EQUAL_size_t(128u, (size_t)embed_atomic_usize_load(&plain_moving_ctx.in_use));
    plain.moving.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&plain_moving_ctx.in_use));
}

void test_bytes_outside_any_cellblock_count_to_an_owner_alone(void)
{
    mmgr_owner_bytes_taken(&s_other, 4096u);
    TEST_ASSERT_EQUAL_size_t(4096u, (size_t)embed_atomic_usize_load(&s_other.in_use));
    TEST_ASSERT_EQUAL_size_t(4096u, (size_t)embed_atomic_usize_load(&s_other.hw) >= 4096u ? 4096u : 0u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&plain_moving_ctx.in_use), "no cellblock's");
    mmgr_owner_bytes_given(&s_other, 4096u);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&s_other.in_use));
}

void test_touches_sum_per_thread_after_the_fact(void)
{
    size_t id = 0u;
    size_t touches = 0u;

    s_thread_id = 7u;
    void *const one = plain.moving.persistent_buf_alloc(16u, &s_mover);
    void *const two = plain.moving.persistent_buf_alloc(16u, &s_mover);

    TEST_ASSERT_TRUE(mmgr_movable_touches(&plain_moving_ctx, 0u, &id, &touches));
    TEST_ASSERT_EQUAL_size_t(7u, id);
    TEST_ASSERT_EQUAL_size_t(2u, touches);
    s_thread_id = 8u;
    plain.moving.persistent_buf_release(one, &s_mover);
    TEST_ASSERT_TRUE(mmgr_movable_touches(&plain_moving_ctx, 1u, &id, &touches));
    TEST_ASSERT_EQUAL_size_t(8u, id);
    TEST_ASSERT_EQUAL_size_t(1u, touches);
    s_thread_id = 7u;
    plain.moving.persistent_buf_release(two, &s_mover);
    TEST_ASSERT_TRUE(mmgr_movable_touches(&plain_moving_ctx, 0u, &id, &touches));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u, touches, "a thread comes back to its row");
}

void test_the_last_row_sums_every_thread_past_the_rest(void)
{
    size_t id = 0u;
    size_t touches = 0u;

    for (size_t thread = 10u; thread < 14u; thread += 1u)
    {
        s_thread_id = thread;
        plain.moving.persistent_buf_release(plain.moving.persistent_buf_alloc(16u, &s_mover), &s_mover);
    }
    TEST_ASSERT_TRUE(mmgr_movable_touches(&plain_moving_ctx, MOVING_ROWS - 1u, &id, &touches));
    TEST_ASSERT_EQUAL_size_t(SIZE_MAX, id);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, touches, "the two threads past the first two");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_movable_touches(&plain_moving_ctx, MOVING_ROWS, &id, &touches), "past the rows");
}

void test_no_rows_keeps_no_touches(void)
{
    size_t id = 0u;
    size_t touches = 0u;

    memset(mmgr_pars_storage_narrow_option, 0, narrow_option_bytes);
    TEST_ASSERT_TRUE(single.narrow.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_narrow_option,
                                                                        .storage_bytes = narrow_option_bytes,
                                                                        .cells = MOVING_CELLS,
                                                                        .width = 1u}));
    single.narrow.persistent_buf_release(single.narrow.persistent_buf_alloc(16u, &s_mover), &s_mover);
    TEST_ASSERT_FALSE(mmgr_movable_touches(&single_narrow_ctx, 0u, &id, &touches));
}

// A hole below a movable cell large enough to read over the threshold, and the cell that would fill it
static const CarcerMovableSlot *fragmented(void)
{
    void *const low = plain.moving.persistent_buf_alloc(512u, &s_mover);
    const CarcerMovableSlot *const cell = marked_sized(128u, 0x33u);

    plain.moving.persistent_buf_release(low, &s_mover);
    TEST_ASSERT_TRUE(mmgr_cellblock_fragmentation(&plain_moving_ctx) >= MMGR_MOVABLE_SETTLE_PERCENT);
    return cell;
}

void test_the_mover_leaves_a_cellblock_under_the_threshold_alone(void)
{
    void *const low = plain.moving.persistent_buf_alloc(16u, &s_mover);
    const CarcerMovableSlot *const cell = marked_sized(256u, 0x35u);

    plain.moving.persistent_buf_release(low, &s_mover);
    TEST_ASSERT_TRUE(mmgr_cellblock_fragmentation(&plain_moving_ctx) < MMGR_MOVABLE_SETTLE_PERCENT);
    s_ticks_left = 2u * MMGR_MOVABLE_QUIET_READINGS;
    mmgr_movable_run(&plain_moving_ctx);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, s_moving_told, "nothing is moved under the threshold");
    assert_marked(cell, 256u, 0x35u);
}

void test_the_mover_waits_for_its_quiet_readings_before_it_sorts(void)
{
    (void)fragmented();
    // One reading short of quiet, with the lock's reading still to come
    s_ticks_left = MMGR_MOVABLE_QUIET_READINGS - 1u;
    mmgr_movable_run(&plain_moving_ctx);
    TEST_ASSERT_EQUAL_UINT(0u, s_moving_told);
}

void test_the_mover_sorts_over_the_threshold_at_a_quiet_time_and_stops(void)
{
    const CarcerMovableSlot *const cell = fragmented();
    const size_t gap = plain.moving.buf_available();

    // The readings, the one lock's wait, and one more to see the sort is not run again
    s_ticks_left = MMGR_MOVABLE_QUIET_READINGS + 2u;
    mmgr_movable_run(&plain_moving_ctx);
    TEST_ASSERT_TRUE_MESSAGE(s_moving_told >= 1u, "cells moved");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(s_moving_told, s_moving_ended, "each move told at both ends");
    TEST_ASSERT_TRUE_MESSAGE(plain.moving.buf_available() > gap, "the tier's end came back");
    assert_marked(cell, 128u, 0x33u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(MMGR_MOVABLE_READ_MS, s_most_wait, "nothing backed off");
}

void test_a_sort_that_can_move_nothing_is_not_run_again_until_the_reading_rises(void)
{
    // A hole above the only movable cell: over the threshold with nothing to move
    const CarcerMovableSlot *const cell = marked_sized(128u, 0x36u);
    void *const hole = plain.moving.persistent_buf_alloc(512u, &s_mover);
    void *const pin = plain.moving.persistent_buf_alloc(16u, &s_mover);

    plain.moving.persistent_buf_release(hole, &s_mover);
    TEST_ASSERT_TRUE(mmgr_cellblock_fragmentation(&plain_moving_ctx) >= MMGR_MOVABLE_SETTLE_PERCENT);
    s_ticks_left = MMGR_MOVABLE_QUIET_READINGS + 1u;
    mmgr_movable_run(&plain_moving_ctx);
    const unsigned int first_sort = s_moving_told;

    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, first_sort, "one try at a move, which finds none");
    assert_marked(cell, 128u, 0x36u);
    TEST_ASSERT_NOT_NULL(pin);
}

// Inside a give's turnstile, with the mover's lock on the other, a take finds no turnstile free
static void pressure_raised(void)
{
    void *const cell = plain.moving.persistent_buf_alloc(16u, &s_mover);

    s_take_when_misnamed = (void *)&s_mover;
    plain.moving.persistent_buf_release(cell, &s_other);
}

void test_the_mover_backs_off_where_a_locked_turnstile_raises_the_pressure(void)
{
    (void)fragmented();
    // The quiet readings, then the lock's wait, inside which the pressure rises
    s_turn = pressure_raised;
    s_turn_on_tick = MMGR_MOVABLE_QUIET_READINGS + 1u;
    s_ticks_left = MMGR_MOVABLE_QUIET_READINGS + 2u;
    mmgr_movable_run(&plain_moving_ctx);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, s_misnamed, "the case's give reached its hook");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(0u, s_moving_told, "nothing moved");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u * MMGR_MOVABLE_READ_MS, s_last_wait, "and the wait doubled");
    // Every lock let go: a take passes a turnstile again and goes on counting to its thread
    size_t id = 0u;
    size_t touches = 0u;

    s_thread_id = 99u;
    plain.moving.persistent_buf_release(plain.moving.persistent_buf_alloc(16u, &s_mover), &s_mover);
    TEST_ASSERT_TRUE(mmgr_movable_touches(&plain_moving_ctx, 1u, &id, &touches));
    TEST_ASSERT_EQUAL_size_t(99u, id);
}

void test_a_wait_ending_the_mover_inside_a_lock_lets_the_lock_go(void)
{
    (void)fragmented();
    // Ended at the lock's wait
    s_ticks_left = MMGR_MOVABLE_QUIET_READINGS;
    mmgr_movable_run(&plain_moving_ctx);
    TEST_ASSERT_EQUAL_UINT(0u, s_moving_told);
    size_t id = 0u;
    size_t touches = 0u;

    s_thread_id = 5u;
    plain.moving.persistent_buf_release(plain.moving.persistent_buf_alloc(16u, &s_mover), &s_mover);
    TEST_ASSERT_TRUE_MESSAGE(mmgr_movable_touches(&plain_moving_ctx, 1u, &id, &touches), "the turnstile opened again");
}

// The thread that gives with the wrong owner and stands in its turnstile, and the thread that runs the mover over a
// one-turnstile option while it does
static void occupied_each(void)
{
    if (embed_atomic_u32_fetch_add(&s_next_index, 1u) == 0u)
    {
        void *const cell = single.narrow.persistent_buf_alloc(16u, &s_mover);

        single.narrow.persistent_buf_release(cell, &s_other);
        return;
    }
    while (embed_atomic_usize_load(&s_turnstile_taken) == 0u)
    {
    }
    s_ticks_left = 2u * MMGR_MOVABLE_QUIET_READINGS;
    mmgr_movable_run(&single_narrow_ctx);
    embed_atomic_usize_store(&s_moves, s_moving_told);
    embed_atomic_usize_store(&s_hold_turnstile, 0u);
}

void test_occupied_turnstiles_keep_the_mover_from_sorting(void)
{
    memset(mmgr_pars_storage_narrow_option, 0, narrow_option_bytes);
    TEST_ASSERT_TRUE(single.narrow.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_narrow_option,
                                                                        .storage_bytes = narrow_option_bytes,
                                                                        .cells = MOVING_CELLS,
                                                                        .width = 1u,
                                                                        .thread_wait = script_wait,
                                                                        .thread_moving = script_moving}));
    void *const low = single.narrow.persistent_buf_alloc(512u, &s_mover);
    const CarcerMovableSlot *const cell = single.narrow.movable_buf_alloc(128u, &s_mover);

    TEST_ASSERT_NOT_NULL(cell);
    single.narrow.persistent_buf_release(low, &s_mover);
    TEST_ASSERT_TRUE(mmgr_cellblock_fragmentation(&single_narrow_ctx) >= MMGR_MOVABLE_SETTLE_PERCENT);
    embed_atomic_usize_store(&s_hold_turnstile, 1u);
    TEST_ASSERT_TRUE(mmgr_host_threads_run(occupied_each, 2u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_moves), "no move while it stands occupied");
    // With it free and one turnstile, there is none to lock, and occupancy decides alone
    s_ticks_left = MMGR_MOVABLE_QUIET_READINGS;
    mmgr_movable_run(&single_narrow_ctx);
    TEST_ASSERT_TRUE_MESSAGE(s_moving_told >= 1u, "and it sorts once it reads quiet");
}

void test_with_the_option_a_temporary_take_its_tier_cannot_meet_lands_in_a_free_persistent_cell(void)
{
    uint8_t *const hole = plain.moving.persistent_buf_alloc(1024u, &s_mover);
    uint8_t *const above = plain.moving.persistent_buf_alloc(64u, &s_mover);

    TEST_ASSERT_NOT_NULL(hole);
    TEST_ASSERT_NOT_NULL(above);
    while (plain.moving.temporary_buf_alloc(64u, &s_mover) != NULL)
    {
    }
    plain.moving.persistent_buf_release(hole, &s_mover);
    const size_t before = mmgr_owner_in_use(&s_mover);
    uint8_t *const taken = plain.moving.temporary_buf_alloc(512u, &s_mover);

    TEST_ASSERT_NOT_NULL_MESSAGE(taken, "1024 bytes stand free in the cellblock");
    TEST_ASSERT_TRUE_MESSAGE((taken >= hole) && (taken < above), "the free cell of the other tier takes it");
    TEST_ASSERT_EQUAL_size_t(before + 512u, mmgr_owner_in_use(&s_mover));
    plain.moving.persistent_buf_release(taken, &s_mover);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, mmgr_owner_in_use(&s_mover), "it is given back as any cell");
}

void test_with_the_option_a_persistent_take_its_tier_cannot_meet_lands_in_a_free_temporary_cell(void)
{
    uint8_t *const hole = plain.moving.temporary_buf_alloc(1024u, &s_mover);
    uint8_t *const below = plain.moving.temporary_buf_alloc(64u, &s_mover);

    TEST_ASSERT_NOT_NULL(hole);
    TEST_ASSERT_NOT_NULL(below);
    while (plain.moving.persistent_buf_alloc(64u, &s_mover) != NULL)
    {
    }
    plain.moving.persistent_buf_release(hole, &s_mover);
    const size_t before = mmgr_owner_in_use(&s_mover);
    uint8_t *const taken = plain.moving.persistent_buf_alloc(512u, &s_mover);

    TEST_ASSERT_NOT_NULL_MESSAGE(taken, "1024 bytes stand free in the cellblock");
    TEST_ASSERT_TRUE_MESSAGE((taken > below) && (taken < (hole + 1024)), "the free cell of the other tier takes it");
    plain.moving.persistent_buf_release(taken, &s_mover);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, mmgr_owner_in_use(&s_mover), "it is given back as any cell");
}

void test_with_no_option_a_take_keeps_to_its_own_tier(void)
{
    uint8_t *const hole = unassigned.bare.persistent_buf_alloc(256u, &s_mover);

    TEST_ASSERT_NOT_NULL(hole);
    TEST_ASSERT_NOT_NULL(unassigned.bare.persistent_buf_alloc(32u, &s_mover));
    while (unassigned.bare.temporary_buf_alloc(32u, &s_mover) != NULL)
    {
    }
    unassigned.bare.persistent_buf_release(hole, &s_mover);
    TEST_ASSERT_NULL_MESSAGE(unassigned.bare.temporary_buf_alloc(128u, &s_mover), "a rewind's tier stays its own");
    TEST_ASSERT_NOT_NULL_MESSAGE(unassigned.bare.persistent_buf_alloc(128u, &s_mover), "the tier's own take fits");
}

// The two threads of the moving bit's case: the first holds the bit, the second takes, and the first lets the bit go
// once the take has yielded to it or has answered without
static embed_atomic_usize s_bit_held = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_bit_yields = EMBED_ATOMIC_INIT(0u);
static embed_atomic_usize s_bit_answered = EMBED_ATOMIC_INIT(0u);
static void *s_bit_taken;

static void bit_yielded(void)
{
    (void)embed_atomic_usize_fetch_add(&s_bit_yields, 1u);
}

static void bit_each(void)
{
    if (embed_atomic_u32_fetch_add(&s_next_index, 1u) == 0u)
    {
        embed_atomic_usize_store(&plain_moving_ctx.temporary_moving, 1u);
        embed_atomic_usize_store(&s_bit_held, 1u);
        while ((embed_atomic_usize_load(&s_bit_yields) == 0u) && (embed_atomic_usize_load(&s_bit_answered) == 0u))
        {
        }
        embed_atomic_usize_store(&plain_moving_ctx.temporary_moving, 0u);
        return;
    }
    while (embed_atomic_usize_load(&s_bit_held) == 0u)
    {
    }
    s_bit_taken = plain.moving.temporary_buf_alloc(512u, &s_mover);
    embed_atomic_usize_store(&s_bit_answered, 1u);
}

void test_with_the_option_a_take_waits_its_turn_at_the_moving_bit_and_is_not_refused(void)
{
    mmgr_cellblock_emptied(&plain_moving_ctx);
    memset(mmgr_pars_storage_moving_option, 0, moving_option_bytes);
    TEST_ASSERT_TRUE(plain.moving.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_moving_option,
                                                                       .storage_bytes = moving_option_bytes,
                                                                       .cells = MOVING_CELLS,
                                                                       .width = MOVING_WIDTH,
                                                                       .rows = MOVING_ROWS,
                                                                       .thread_id = script_id,
                                                                       .thread_yield = bit_yielded}));
    uint8_t *const hole = plain.moving.temporary_buf_alloc(1024u, &s_mover);

    TEST_ASSERT_NOT_NULL(hole);
    TEST_ASSERT_NOT_NULL(plain.moving.temporary_buf_alloc(64u, &s_mover));
    while (plain.moving.persistent_buf_alloc(64u, &s_mover) != NULL)
    {
    }
    plain.moving.persistent_buf_release(hole, &s_mover);
    embed_atomic_usize_store(&s_bit_held, 0u);
    embed_atomic_usize_store(&s_bit_yields, 0u);
    embed_atomic_usize_store(&s_bit_answered, 0u);
    s_bit_taken = NULL;
    TEST_ASSERT_TRUE(mmgr_host_threads_run(bit_each, 2u));
    TEST_ASSERT_NOT_NULL_MESSAGE(s_bit_taken, "a free cell stands in the tier: the take waits for the bit");
    TEST_ASSERT_TRUE_MESSAGE((size_t)embed_atomic_usize_load(&s_bit_yields) > 0u, "and yields while it waits");
}

#if MMGR_ENABLE_HOST
LocusCarcerumHospes(hosted, MMGR_MINIMUM_SECURITY(hosted_pool));
#endif

void test_a_host_site_with_the_option_assigned_is_not_made_anew(void)
{
#if !MMGR_ENABLE_HOST
    TEST_IGNORE_MESSAGE("the host site is built where MMGR_ENABLE_HOST is 1");
#else
    TEST_ASSERT_TRUE(hosted_made(4096u));
    TEST_ASSERT_TRUE_MESSAGE(hosted_made(8192u), "made anew without the option");
    memset(mmgr_pars_storage_narrow_option, 0, narrow_option_bytes);
    TEST_ASSERT_TRUE(hosted.hosted_pool.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_narrow_option,
                                                                             .storage_bytes = narrow_option_bytes,
                                                                             .cells = MOVING_CELLS,
                                                                             .width = 1u}));
    TEST_ASSERT_FALSE_MESSAGE(hosted_made(16384u), "the option reads the block");
    const CarcerMovableSlot *const cell = hosted.hosted_pool.movable_buf_alloc(16u, &s_mover);

    TEST_ASSERT_NOT_NULL(cell);
    TEST_ASSERT_TRUE(hosted.hosted_pool.movable_buf_release(cell, &s_mover));
    hosted_freed();
    TEST_ASSERT_TRUE(hosted_cellblock() == NULL);
#endif
}

// A writer's cell filled with count
static void crowd_filled(embed_word *words, embed_word count)
{
    for (unsigned int at = 0u; at < CROWD_CELL_WORDS; at += 1u)
    {
        words[at] = count;
    }
}

// Whether every word of a writer's cell still holds count
static embed_bool crowd_holds(const embed_word *words, embed_word count)
{
    embed_bool whole = EMBED_TRUE;

    for (unsigned int at = 0u; at < CROWD_CELL_WORDS; at += 1u)
    {
        whole = (embed_bool)(whole && (words[at] == count));
    }
    return whole;
}

// A writer takes a cell, then each round reads the count it last wrote and writes the next. Every
// tenth round it gives the cell back and takes another, the hole it leaves what the mover fills
static void crowd_written(void)
{
    const CarcerMovableSlot *cell = NULL;
    embed_word count = 0u;
    embed_word words[CROWD_CELL_WORDS];

    for (unsigned int round = 0u; round < CROWD_ROUNDS; round += 1u)
    {
        if (cell == NULL)
        {
            cell = crowded.crowd.movable_buf_alloc(sizeof(words), &s_mover);
            if (cell == NULL)
            {
                (void)embed_atomic_usize_fetch_add(&s_refused, 1u);
                continue;
            }
            count = 0u;
            crowd_filled(words, count);
            (void)crowded.crowd.movable_buf_write(
                &(CarcerMovableCfg){.cell = cell, .src = words, .bytes = sizeof(words)});
        }
        (void)crowded.crowd.movable_buf_read(&(CarcerMovableCfg){.cell = cell, .dst = words, .bytes = sizeof(words)});
        if (!crowd_holds(words, count))
        {
            (void)embed_atomic_usize_fetch_add(&s_lost, 1u);
        }
        count += 1u;
        crowd_filled(words, count);
        (void)crowded.crowd.movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = words, .bytes = sizeof(words)});
        if ((round % 10u) == 9u)
        {
            (void)crowded.crowd.movable_buf_release(cell, &s_mover);
            cell = NULL;
        }
    }
    if (cell != NULL)
    {
        (void)crowded.crowd.movable_buf_release(cell, &s_mover);
    }
    (void)embed_atomic_usize_fetch_add(&s_writers_done, 1u);
}

// The first thread moves cells until every writer has finished, the others write
static void crowd_each(void)
{
    if (embed_atomic_u32_fetch_add(&s_next_index, 1u) != 0u)
    {
        crowd_written();
        return;
    }
    while (embed_atomic_usize_load(&s_writers_done) < CROWD_WRITERS)
    {
        if (mmgr_movable_buf_move(&crowded_crowd_ctx))
        {
            (void)embed_atomic_usize_fetch_add(&s_moves, 1u);
        }
    }
}

void test_no_write_is_lost_while_another_thread_moves_the_cells(void)
{
    memset(mmgr_pars_storage_crowd_option, 0, crowd_option_bytes);
    TEST_ASSERT_TRUE(crowded.crowd.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_crowd_option,
                                                                        .storage_bytes = crowd_option_bytes,
                                                                        .cells = CROWD_WRITERS + 1u,
                                                                        .width = CROWD_WIDTH}));
    TEST_ASSERT_TRUE(mmgr_host_threads_run(crowd_each, CROWD_WRITERS + 1u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_lost),
                                     "a write landed in a copy left behind");
    TEST_ASSERT_TRUE_MESSAGE((size_t)embed_atomic_usize_load(&s_moves) > 0u,
                             "the case has to move cells to mean anything");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&s_mover.in_use),
                                     "every cell is counted back");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(&crowded_crowd_ctx.in_use),
                                     "by the cellblock too");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(CROWD_BYTES, crowded.crowd.buf_available(), "and the cellblock stands whole");
}
