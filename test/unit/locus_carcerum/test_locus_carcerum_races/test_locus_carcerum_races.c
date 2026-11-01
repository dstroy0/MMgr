/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_locus_carcerum_races.c
 * @brief What a cellblock does when another context reaches a word first, on demand and not by luck.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-01
 *
 * @note Compiles the translation unit in, and so file-local entries are callable and the atomic swaps
 *       are the suite's own. A case names the swap that loses, or the swap after which another context
 *       acts, and each runs the same way every time.
 * @note A lost swap reads the word back into what the caller expected, as a real one does.
 * @note MMGR_FATAL jumps back to the case here. A prisoner from another cellblock has nothing to carry on
 *       from, and the jump is what lets a case read that it stopped.
 */
#include "locus_carcerum/locus_carcerum.h"

#include "embed_atomic.h"
#include "unity.h"

#include <setjmp.h>
#include <string.h>

static jmp_buf s_fatal;
static size_t s_swaps;
static size_t s_lost_at;
static size_t s_after_at;
static void (*s_after)(void);

/** @brief The real swaps, named ahead of the two names the translation unit is about to see. */
static embed_bool real_usize_swap(embed_atomic_usize *word, embed_atomic_usize_value *expected,
                                  embed_atomic_usize_value desired)
{
    return embed_atomic_usize_swap_if(word, expected, desired);
}

static embed_bool real_uptr_swap(embed_atomic_uptr *word, embed_atomic_uptr_value *expected,
                                 embed_atomic_uptr_value desired)
{
    return embed_atomic_uptr_swap_if(word, expected, desired);
}

/** @brief Counts one swap and says whether the case named it to lose. */
static embed_bool swap_lost(void)
{
    s_swaps += 1u;
    return (embed_bool)(s_swaps == s_lost_at);
}

/** @brief Runs what the case named to happen once this swap has landed. */
static void swap_landed(void)
{
    if ((s_after != NULL) && (s_swaps == s_after_at))
    {
        void (*const after)(void) = s_after;

        s_after = NULL;
        after();
    }
}

static embed_bool scripted_usize_swap(embed_atomic_usize *word, embed_atomic_usize_value *expected,
                                      embed_atomic_usize_value desired)
{
    if (swap_lost())
    {
        *expected = embed_atomic_usize_load(word);
        return EMBED_FALSE;
    }

    const embed_bool landed = real_usize_swap(word, expected, desired);

    swap_landed();
    return landed;
}

static embed_bool scripted_uptr_swap(embed_atomic_uptr *word, embed_atomic_uptr_value *expected,
                                     embed_atomic_uptr_value desired)
{
    if (swap_lost())
    {
        *expected = embed_atomic_uptr_load(word);
        return EMBED_FALSE;
    }

    const embed_bool landed = real_uptr_swap(word, expected, desired);

    swap_landed();
    return landed;
}

#undef MMGR_FATAL
#define MMGR_FATAL(msg_) longjmp(s_fatal, 1)
#undef embed_atomic_usize_swap_if
#define embed_atomic_usize_swap_if scripted_usize_swap
#undef embed_atomic_uptr_swap_if
#define embed_atomic_uptr_swap_if scripted_uptr_swap

#include "locus_carcerum/locus_carcerum.c"

#define RACED_BYTES 4096u
#define RACED_CELLS 8u
#define RACED_WIDTH 2u
#define RACED_ROWS 3u
#define PAYLOAD (MMGR_CARCER_ALIGN * 4u)

ParsMemoriaeInternae(raced, RACED_BYTES);
ParsMemoriaeInternae(raced_option, MMGR_MOVABLE_STORAGE_BYTES(RACED_CELLS, RACED_WIDTH, RACED_ROWS));
LocusCarcerum(cells, MMGR_MINIMUM_SECURITY(raced));

#define BLOCK (&cells_raced_ctx)

static CellOwner s_owner = MMGR_CELL_OWNER("raced");
static CellOwner s_other = MMGR_CELL_OWNER("other");
static CellOwner s_late = MMGR_CELL_OWNER("late");
static uint8_t s_foreign[16];

static void script_cleared(void)
{
    s_swaps = 0u;
    s_lost_at = 0u;
    s_after_at = 0u;
    s_after = NULL;
}

/** @brief Names the swap that loses, counted from here. */
static void lose_swap(size_t swap)
{
    script_cleared();
    s_lost_at = swap;
}

/** @brief Names the swap after which another context acts, counted from here. */
static void after_swap(size_t swap, void (*after)(void))
{
    script_cleared();
    s_after_at = swap;
    s_after = after;
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

/** @brief The cellblock as a fresh one reads, every byte of it zero. */
static void block_fresh(void)
{
    script_cleared();
    mmgr_cellblock_emptied(BLOCK);
    memset(BLOCK->base, 0, BLOCK->size);
    embed_atomic_usize_store(&BLOCK->persistent_moving, 0u);
    embed_atomic_usize_store(&BLOCK->temporary_moving, 0u);
}

void setUp(void)
{
    block_fresh();
    owner_ready(&s_owner);
    owner_ready(&s_other);
}

void tearDown(void)
{
}

static embed_atomic_usize_value owner_word(const CellOwner *owner)
{
    // Explicit casts carry the owner's address as the integer the owner's word holds
    return (embed_atomic_usize_value)(uintptr_t)owner;
}

static void word_put(size_t off, embed_atomic_usize_value word)
{
    embed_atomic_usize_store(carcer_word_at(BLOCK, off), word);
}

static embed_atomic_usize_value word_read(size_t off)
{
    return embed_atomic_usize_load(carcer_word_at(BLOCK, off));
}

/** @brief Lays a cell by hand: its word, its owner's word and its tail. */
static void cell_laid(size_t off, size_t size, embed_atomic_usize_value state, embed_atomic_usize_value named)
{
    word_put(off, carcer_word(size, state));
    word_put(carcer_foot_of(off, size), named);
    word_put(carcer_tail_of(off, size), carcer_tail_word(off));
}

static void tiers_put(size_t end, size_t top)
{
    embed_atomic_usize_store(&BLOCK->tiers, MMGR_CARCER_TIERS(end, top));
}

/** @brief Another context's rewind, past every cell the tier holds. */
static void rewound_past_all(void)
{
    tiers_put(mmgr_cellblock_persistent_end(BLOCK), BLOCK->size);
}

static embed_bool wait_steady(uint32_t milliseconds)
{
    (void)milliseconds;
    return EMBED_TRUE;
}

/** @brief The movable option over its zeroed storage. With no thread_started, no mover runs on its own. */
static CarcerMovable *option_assigned(void)
{
    memset(mmgr_pars_storage_raced_option, 0, raced_option_bytes);
    TEST_ASSERT_TRUE(cells.raced.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_raced_option,
                                                                      .storage_bytes = raced_option_bytes,
                                                                      .cells = RACED_CELLS,
                                                                      .width = RACED_WIDTH,
                                                                      .rows = RACED_ROWS,
                                                                      .thread_wait = wait_steady}));
    return carcer_movable_of(BLOCK);
}

void test_an_address_no_owner_registered_at_finds_no_owner(void)
{
    TEST_ASSERT_NULL(carcer_owner_found(owner_word(&s_late)));
}

void test_a_register_that_finds_another_registering_waits_its_turn(void)
{
    lose_swap(1u);
    TEST_ASSERT_TRUE(mmgr_owner_register(&s_late));
    TEST_ASSERT_EQUAL_PTR(&s_late, carcer_owner_found(owner_word(&s_late)));
}

void test_a_give_naming_another_owner_changes_nothing(void)
{
    void *const prisoner = cells.raced.persistent_buf_alloc(PAYLOAD, &s_owner);

    TEST_ASSERT_NOT_NULL(prisoner);
    cells.raced.persistent_buf_release(prisoner, &s_other);
    TEST_ASSERT_EQUAL_PTR(&s_owner, mmgr_owner_of_buf(BLOCK, prisoner));
}

void test_a_foot_reading_none_takes_nothing_off_any_owner(void)
{
    cell_laid(0u, PAYLOAD, CARCER_BUSY, 0u);
    embed_atomic_usize_store(&BLOCK->in_use, PAYLOAD);

    carcer_foot_claimed(BLOCK, 0u, PAYLOAD, EMBED_FALSE);
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, (size_t)embed_atomic_usize_load(&BLOCK->in_use));
}

void test_a_foot_another_context_claimed_first_takes_nothing_off_any_owner(void)
{
    cell_laid(0u, PAYLOAD, CARCER_BUSY, owner_word(&s_owner));
    embed_atomic_usize_store(&BLOCK->in_use, PAYLOAD);
    embed_atomic_usize_store(&s_owner.in_use, PAYLOAD);

    lose_swap(1u);
    carcer_foot_claimed(BLOCK, 0u, PAYLOAD, EMBED_FALSE);
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, (size_t)embed_atomic_usize_load(&BLOCK->in_use));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(&s_owner));
}

void test_a_drop_ends_at_a_word_already_dead(void)
{
    const size_t above = carcer_end_of(0u, PAYLOAD);

    cell_laid(0u, PAYLOAD, CARCER_DEAD, 0u);
    cell_laid(above, PAYLOAD, CARCER_BUSY, 0u);

    carcer_dropped(BLOCK, 0u, BLOCK->size);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)carcer_word(PAYLOAD, CARCER_BUSY), (uint64_t)word_read(above));
}

void test_a_drop_ends_at_a_word_too_small_to_be_a_cell(void)
{
    const embed_atomic_usize_value small = carcer_word(MMGR_CARCER_ALIGN - 1u, CARCER_BUSY);

    word_put(0u, small);
    carcer_dropped(BLOCK, 0u, BLOCK->size);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)small, (uint64_t)word_read(0u));
}

void test_a_drop_ends_at_a_cell_reaching_past_its_bound(void)
{
    cell_laid(0u, PAYLOAD, CARCER_BUSY, 0u);

    carcer_dropped(BLOCK, 0u, PAYLOAD);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)carcer_word(PAYLOAD, CARCER_BUSY), (uint64_t)word_read(0u));
}

void test_a_drop_that_loses_its_swap_leaves_the_cell_to_the_winner(void)
{
    cell_laid(0u, PAYLOAD, CARCER_BUSY, 0u);

    lose_swap(1u);
    carcer_dropped(BLOCK, 0u, BLOCK->size);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)carcer_word(PAYLOAD, CARCER_BUSY), (uint64_t)word_read(0u));
}

void test_a_take_that_loses_a_swap_on_any_of_its_words_answers_null(void)
{
    // The first swap moves the boundary, and the three after it lay the tail, the owner's word and the word
    for (size_t swap = 2u; swap <= 4u; swap += 1u)
    {
        block_fresh();
        lose_swap(swap);
        TEST_ASSERT_NULL(carcer_grow(BLOCK, PAYLOAD, EMBED_FALSE, &s_owner));
    }
}

void test_a_take_a_rewind_passed_drops_its_own_cell_and_answers_null(void)
{
    after_swap(4u, rewound_past_all);
    TEST_ASSERT_NULL(carcer_grow(BLOCK, PAYLOAD, EMBED_TRUE, &s_owner));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(&s_owner));
}

void test_a_request_of_nothing_rounds_to_one_word(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, carcer_want(0u));
}

void test_a_word_that_is_no_free_cell_does_not_read_as_one(void)
{
    embed_atomic_usize_value word = 0u;

    word_put(0u, carcer_word(PAYLOAD + 1u, CARCER_FREE));
    TEST_ASSERT_FALSE_MESSAGE(carcer_free_read(BLOCK, 0u, BLOCK->size, &word),
                              "a size that is no whole number of words");

    word_put(0u, carcer_word(PAYLOAD, CARCER_FREE));
    TEST_ASSERT_FALSE_MESSAGE(carcer_free_read(BLOCK, 0u, PAYLOAD, &word), "a cell that ends past the bound");
    TEST_ASSERT_FALSE_MESSAGE(carcer_free_read(BLOCK, 0u, BLOCK->size, &word), "a tail that names another cell");
}

void test_a_merge_that_loses_the_cell_above_keeps_its_own_size(void)
{
    const size_t above = carcer_end_of(0u, PAYLOAD);
    size_t at = 0u;
    size_t size = PAYLOAD;

    cell_laid(0u, PAYLOAD, CARCER_BUSY, 0u);
    cell_laid(above, PAYLOAD, CARCER_FREE, 0u);

    lose_swap(1u);
    carcer_coalesce(BLOCK, (CarcerTier){.lo = 0u, .hi = carcer_end_of(above, PAYLOAD)}, &at, &size);
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, size);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)carcer_word(PAYLOAD, CARCER_FREE), (uint64_t)word_read(above));
}

void test_a_merge_leaves_a_cell_below_it_cannot_take(void)
{
    const size_t mine = carcer_end_of(0u, PAYLOAD);
    const CarcerTier tier = {.lo = 0u, .hi = carcer_end_of(mine, PAYLOAD)};
    size_t at = mine;
    size_t size = PAYLOAD;

    cell_laid(0u, PAYLOAD, CARCER_FREE, 0u);
    cell_laid(mine, PAYLOAD, CARCER_BUSY, 0u);

    carcer_coalesce(BLOCK, (CarcerTier){.lo = CARCER_WORD, .hi = tier.hi}, &at, &size);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mine, at, "a tail naming a cell below the tier");

    lose_swap(1u);
    carcer_coalesce(BLOCK, tier, &at, &size);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mine, at, "the cell below went to another context");
    script_cleared();

    word_put(mine - CARCER_WORD, carcer_word(1u, CARCER_DEAD));
    carcer_coalesce(BLOCK, tier, &at, &size);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mine, at, "a tail that is no whole number of words");

    word_put(mine - CARCER_WORD, carcer_word(mine, CARCER_DEAD));
    carcer_coalesce(BLOCK, tier, &at, &size);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mine, at, "a tail naming this cell or one above it");

    const size_t apart = mine + (MMGR_CARCER_ALIGN * 2u);

    word_put(mine - CARCER_WORD, carcer_tail_word(0u));
    word_put(apart - CARCER_WORD, carcer_tail_word(0u));
    at = apart;
    carcer_coalesce(BLOCK, (CarcerTier){.lo = 0u, .hi = apart}, &at, &size);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(apart, at, "a cell below that does not end at this one");
}

void test_a_return_stops_at_a_next_cell_it_cannot_take(void)
{
    const size_t mine = carcer_end_of(0u, PAYLOAD);
    size_t at = mine;
    size_t held = PAYLOAD;

    word_put(mine - CARCER_WORD, carcer_word(1u, CARCER_DEAD));
    TEST_ASSERT_FALSE_MESSAGE(carcer_next_taken(BLOCK, EMBED_TRUE, carcer_end_of(mine, PAYLOAD), &at, &held),
                              "a tail that is no whole number of words");

    cell_laid(0u, PAYLOAD, CARCER_FREE, 0u);
    lose_swap(1u);
    TEST_ASSERT_FALSE_MESSAGE(carcer_next_taken(BLOCK, EMBED_TRUE, carcer_end_of(mine, PAYLOAD), &at, &held),
                              "the cell below went to another context");
    TEST_ASSERT_EQUAL_size_t(mine, at);
    script_cleared();

    word_put(mine - CARCER_WORD, carcer_word(mine, CARCER_DEAD));
    TEST_ASSERT_FALSE_MESSAGE(carcer_next_taken(BLOCK, EMBED_TRUE, carcer_end_of(mine, PAYLOAD), &at, &held),
                              "a tail naming this cell or one above it");

    const size_t apart = mine + (MMGR_CARCER_ALIGN * 2u);

    word_put(mine - CARCER_WORD, carcer_tail_word(0u));
    word_put(apart - CARCER_WORD, carcer_tail_word(0u));
    at = apart;
    TEST_ASSERT_FALSE_MESSAGE(carcer_next_taken(BLOCK, EMBED_TRUE, carcer_end_of(apart, PAYLOAD), &at, &held),
                              "a cell below that does not end at this one");
}

void test_a_sweep_leaves_a_boundary_it_cannot_return(void)
{
    const size_t hi = carcer_end_of(0u, PAYLOAD);

    tiers_put(hi, BLOCK->size);
    word_put(hi - CARCER_WORD, carcer_word(1u, CARCER_DEAD));
    carcer_boundary_swept(BLOCK, EMBED_TRUE);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(hi, mmgr_cellblock_persistent_end(BLOCK),
                                     "a tail that is no whole number of words");

    cell_laid(0u, PAYLOAD, CARCER_FREE, 0u);
    lose_swap(1u);
    carcer_boundary_swept(BLOCK, EMBED_TRUE);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(hi, mmgr_cellblock_persistent_end(BLOCK), "the cell went to another context");
    script_cleared();

    const size_t apart = hi + (MMGR_CARCER_ALIGN * 2u);

    tiers_put(apart, BLOCK->size);
    word_put(apart - CARCER_WORD, carcer_tail_word(0u));
    carcer_boundary_swept(BLOCK, EMBED_TRUE);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(apart, mmgr_cellblock_persistent_end(BLOCK),
                                     "a cell that does not end at the boundary");
}

void test_a_fit_ends_at_a_word_that_is_no_cell(void)
{
    const size_t hi = carcer_end_of(0u, PAYLOAD);

    tiers_put(hi, BLOCK->size);
    word_put(0u, carcer_word(PAYLOAD + 1u, CARCER_FREE));
    TEST_ASSERT_TRUE(carcer_moving_taken(&BLOCK->persistent_moving));
    TEST_ASSERT_NULL_MESSAGE(carcer_fit_held(BLOCK, MMGR_CARCER_ALIGN, &s_owner, EMBED_TRUE, BLOCK->size),
                             "a size that is no whole number of words");

    tiers_put(PAYLOAD + CARCER_WORD, BLOCK->size);
    word_put(0u, carcer_word(PAYLOAD, CARCER_FREE));
    TEST_ASSERT_TRUE(carcer_moving_taken(&BLOCK->persistent_moving));
    TEST_ASSERT_NULL_MESSAGE(carcer_fit_held(BLOCK, MMGR_CARCER_ALIGN, &s_owner, EMBED_TRUE, BLOCK->size),
                             "a cell that ends past the tier");
}

void test_a_fit_that_loses_its_cell_answers_null(void)
{
    tiers_put(carcer_end_of(0u, PAYLOAD), BLOCK->size);
    cell_laid(0u, PAYLOAD, CARCER_FREE, 0u);

    TEST_ASSERT_TRUE(carcer_moving_taken(&BLOCK->persistent_moving));
    lose_swap(1u);
    TEST_ASSERT_NULL(carcer_fit_held(BLOCK, PAYLOAD, &s_owner, EMBED_TRUE, BLOCK->size));
}

void test_a_fit_a_rewind_passed_drops_its_own_cell_and_answers_null(void)
{
    const size_t top = BLOCK->size - carcer_end_of(0u, PAYLOAD);

    tiers_put(0u, top);
    cell_laid(top, PAYLOAD, CARCER_FREE, 0u);

    TEST_ASSERT_TRUE(carcer_moving_taken(&BLOCK->temporary_moving));
    // The first swap takes the cell, and the second lets the moving bit go
    after_swap(2u, rewound_past_all);
    TEST_ASSERT_NULL(carcer_fit_held(BLOCK, PAYLOAD, &s_owner, EMBED_FALSE, BLOCK->size));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(&s_owner));
}

void test_a_growth_that_loses_the_tiers_word_reads_it_again(void)
{
    void *const prisoner = carcer_grow(BLOCK, PAYLOAD, EMBED_TRUE, &s_owner);

    TEST_ASSERT_NOT_NULL(prisoner);
    lose_swap(1u);
    TEST_ASSERT_NOT_NULL(
        carcer_grown(BLOCK, carcer_off_of(BLOCK, prisoner), PAYLOAD, PAYLOAD * 2u, &s_owner, EMBED_FALSE));
}

void test_a_growth_of_a_cell_a_rewind_took_the_owner_off_counts_nothing(void)
{
    void *const prisoner = carcer_grow(BLOCK, PAYLOAD, EMBED_TRUE, &s_owner);
    const size_t off = carcer_off_of(BLOCK, prisoner);

    word_put(carcer_foot_of(off, PAYLOAD), 0u);
    TEST_ASSERT_NOT_NULL(carcer_grown(BLOCK, off, PAYLOAD, PAYLOAD * 2u, &s_owner, EMBED_FALSE));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(&s_owner));
}

void test_a_growth_a_rewind_passed_answers_null(void)
{
    void *const prisoner = carcer_grow(BLOCK, PAYLOAD, EMBED_TRUE, &s_owner);

    after_swap(1u, rewound_past_all);
    TEST_ASSERT_NULL(carcer_grown(BLOCK, carcer_off_of(BLOCK, prisoner), PAYLOAD, PAYLOAD * 2u, &s_owner, EMBED_FALSE));
}

void test_touches_with_no_rows_to_sum_into_are_dropped(void)
{
    const CarcerMovable bare = {.rows = 0u};
    CarcerTurnstile turnstile = {.touches = 1u};

    carcer_touches_summed(&bare, &turnstile);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)turnstile.touches);
}

void test_a_search_for_a_turnstile_wraps_past_the_last(void)
{
    const CarcerMovable *const movable = option_assigned();

    embed_atomic_uptr_store(&movable->turnstile[1].inside, CARCER_TURNSTILE_PASSING);
    TEST_ASSERT_EQUAL_PTR(&movable->turnstile[0], carcer_turnstile_tried(movable, CARCER_TURNSTILE_PASSING, 1u));
}

void test_zeroing_starts_at_an_address_off_a_word_boundary(void)
{
    static EMBED_ALIGN(16) uint8_t bytes[32];

    memset(bytes, 0xFF, sizeof bytes);
    mmgr_zero_buf(bytes + 1, MMGR_CARCER_ALIGN * 3u);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0u, bytes[1]);
    TEST_ASSERT_EQUAL_HEX8(0u, bytes[MMGR_CARCER_ALIGN * 3u]);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, bytes[(MMGR_CARCER_ALIGN * 3u) + 1u]);
}

void test_a_prisoner_from_another_cellblock_stops_the_program(void)
{
    if (setjmp(s_fatal) == 0)
    {
        carcer_given(BLOCK, s_foreign, &s_owner, EMBED_FALSE);
        TEST_FAIL_MESSAGE("a give went on past it");
    }
    if (setjmp(s_fatal) == 0)
    {
        (void)carcer_grow_asked(BLOCK, s_foreign, PAYLOAD, &s_owner, EMBED_FALSE);
        TEST_FAIL_MESSAGE("a growth went on past it");
    }
}

void test_a_give_of_a_cell_no_longer_busy_ends_there(void)
{
    cell_laid(0u, PAYLOAD, CARCER_FREE, owner_word(&s_owner));
    embed_atomic_usize_store(&BLOCK->in_use, PAYLOAD);
    embed_atomic_usize_store(&s_owner.in_use, PAYLOAD);

    carcer_given(BLOCK, BLOCK->base + CARCER_WORD, &s_owner, EMBED_FALSE);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)carcer_word(PAYLOAD, CARCER_FREE), (uint64_t)word_read(0u));
}

void test_a_growth_while_another_context_moves_the_tier_is_refused(void)
{
    void *const prisoner = cells.raced.persistent_buf_alloc(PAYLOAD, &s_owner);

    TEST_ASSERT_NOT_NULL(prisoner);
    TEST_ASSERT_TRUE(carcer_moving_taken(&BLOCK->persistent_moving));
    TEST_ASSERT_NULL(carcer_grow_asked(BLOCK, prisoner, PAYLOAD * 2u, &s_owner, EMBED_FALSE));
}

void test_a_rewind_to_a_mark_past_the_cellblock_is_refused(void)
{
    TEST_ASSERT_NOT_NULL(carcer_grow(BLOCK, PAYLOAD, EMBED_TRUE, &s_owner));

    const size_t top = mmgr_cellblock_temporary_top(BLOCK);

    carcer_rewound(BLOCK, BLOCK->size + MMGR_CARCER_ALIGN, EMBED_FALSE);
    TEST_ASSERT_EQUAL_size_t(top, mmgr_cellblock_temporary_top(BLOCK));
}

void test_a_rewind_that_loses_the_tiers_word_runs_again(void)
{
    TEST_ASSERT_NOT_NULL(carcer_grow(BLOCK, PAYLOAD, EMBED_TRUE, &s_owner));

    // The first swap drops the cell, the second takes its owner's word, and the third moves the top
    lose_swap(3u);
    carcer_rewound(BLOCK, BLOCK->size, EMBED_FALSE);
    TEST_ASSERT_EQUAL_size_t(BLOCK->size, mmgr_cellblock_temporary_top(BLOCK));
}

void test_a_reset_rewinds_the_whole_temporary_tier(void)
{
    TEST_ASSERT_NOT_NULL(carcer_grow(BLOCK, PAYLOAD, EMBED_TRUE, &s_owner));
    mmgr_temporary_buf_reset(BLOCK);
    TEST_ASSERT_EQUAL_size_t(BLOCK->size, mmgr_cellblock_temporary_top(BLOCK));
}

void test_a_word_reaching_past_the_cellblock_names_no_owner(void)
{
    word_put(0u, carcer_word(BLOCK->size, CARCER_BUSY));
    TEST_ASSERT_NULL(mmgr_owner_of_buf(BLOCK, BLOCK->base + CARCER_WORD));
}

void test_a_slot_named_on_either_word_of_a_turnstile_is_occupied(void)
{
    const CarcerMovable *const movable = option_assigned();
    const CarcerMovableSlot *const slot = &movable->slot[0];
    // Explicit cast carries the slot's address as the integer a turnstile's words hold
    const embed_atomic_uptr_value named = (embed_atomic_uptr_value)(uintptr_t)slot;

    TEST_ASSERT_FALSE(carcer_slot_named(movable, slot));
    embed_atomic_uptr_store(&movable->turnstile[1].inside, named);
    TEST_ASSERT_TRUE(carcer_slot_named(movable, slot));
    embed_atomic_uptr_store(&movable->turnstile[1].inside, 0u);
    embed_atomic_uptr_store(&movable->turnstile[1].other, named);
    TEST_ASSERT_TRUE(carcer_slot_named(movable, slot));
}

void test_a_move_of_a_cell_whose_owner_word_names_no_owner_is_refused(void)
{
    CarcerMovable *const movable = option_assigned();
    const CarcerMovableSlot *const cell = cells.raced.movable_buf_alloc(PAYLOAD, &s_owner);

    TEST_ASSERT_NOT_NULL(cell);

    const size_t off = carcer_off_of(BLOCK, BLOCK->base + (size_t)embed_atomic_usize_load(&cell->word));

    word_put(carcer_foot_of(off, PAYLOAD), owner_word(&s_late) + MMGR_CARCER_ALIGN);
    TEST_ASSERT_FALSE(mmgr_movable_buf_move(BLOCK));
    TEST_ASSERT_EQUAL_size_t(0u, carcer_queue_depth(movable));
}

void test_a_locked_turnstile_is_not_occupied(void)
{
    const CarcerMovable *const movable = option_assigned();

    embed_atomic_uptr_store(&movable->turnstile[0].inside, CARCER_TURNSTILE_LOCKED);
    embed_atomic_uptr_store(&movable->turnstile[1].inside, CARCER_TURNSTILE_PASSING);
    TEST_ASSERT_EQUAL_size_t(1u, carcer_turnstiles_occupied(movable));
}

void test_a_line_that_grew_is_not_steady(void)
{
    CarcerMovable *const movable = option_assigned();

    embed_atomic_usize_store(&movable->queue_next, embed_atomic_usize_load(&movable->queue_serving) + 1u);
    TEST_ASSERT_FALSE(carcer_pressure_steady(movable, 0u, 0u));
}

void test_a_line_with_no_one_waiting_has_no_depth(void)
{
    (void)option_assigned();
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_movable_queue_depth(BLOCK));
}

void test_opening_lets_go_only_what_reads_locked(void)
{
    const CarcerMovable *const movable = option_assigned();

    embed_atomic_uptr_store(&movable->turnstile[0].inside, CARCER_TURNSTILE_LOCKED);
    carcer_turnstiles_opened(movable, 1u);
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)embed_atomic_uptr_load(&movable->turnstile[0].inside));

    carcer_turnstiles_opened(movable, 1u);
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)embed_atomic_uptr_load(&movable->turnstile[1].inside));
}

void test_a_sort_passes_a_turnstile_in_use_and_runs_with_no_one_to_tell(void)
{
    CarcerMovable *const movable = option_assigned();

    embed_atomic_uptr_store(&movable->turnstile[1].inside, CARCER_TURNSTILE_PASSING);
    TEST_ASSERT_TRUE(carcer_mover_sorted(BLOCK, movable));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)CARCER_TURNSTILE_PASSING,
                             (uint64_t)embed_atomic_uptr_load(&movable->turnstile[1].inside));
}

void test_a_reading_no_higher_than_the_last_sort_left_waits_on(void)
{
    CarcerMovable *const movable = option_assigned();
    void *prisoners[12];

    for (size_t at = 0u; at < 12u; at += 1u)
    {
        prisoners[at] = cells.raced.persistent_buf_alloc(PAYLOAD, &s_owner);
        TEST_ASSERT_NOT_NULL(prisoners[at]);
    }
    for (size_t at = 0u; at < 11u; at += 2u)
    {
        cells.raced.persistent_buf_release(prisoners[at], &s_owner);
    }
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(MMGR_MOVABLE_SETTLE_PERCENT, mmgr_cellblock_fragmentation(BLOCK));

    uint32_t wait = 77u;
    size_t left = 100u;
    size_t quiet = 0u;

    carcer_mover_read(BLOCK, movable, &wait, &left, &quiet);
    TEST_ASSERT_EQUAL_UINT32(77u, wait);
    TEST_ASSERT_EQUAL_size_t(100u, left);
}

void test_with_no_option_there_is_no_line_and_no_row(void)
{
    size_t id = 0u;
    size_t touches = 0u;

    TEST_ASSERT_EQUAL_size_t(0u, mmgr_movable_queue_depth(BLOCK));
    TEST_ASSERT_FALSE(mmgr_movable_touches(BLOCK, 0u, &id, &touches));
}

void test_an_empty_row_reads_as_none_with_a_turnstile_in_use(void)
{
    const CarcerMovable *const movable = option_assigned();
    size_t id = 0u;
    size_t touches = 0u;

    embed_atomic_uptr_store(&movable->turnstile[0].inside, CARCER_TURNSTILE_PASSING);
    TEST_ASSERT_FALSE(mmgr_movable_touches(BLOCK, 0u, &id, &touches));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)CARCER_TURNSTILE_PASSING,
                             (uint64_t)embed_atomic_uptr_load(&movable->turnstile[0].inside));
}

void test_emptying_passes_a_temporary_cell_already_dead(void)
{
    const size_t top = BLOCK->size - carcer_end_of(0u, PAYLOAD);

    tiers_put(0u, top);
    cell_laid(top, PAYLOAD, CARCER_DEAD, 0u);

    mmgr_cellblock_emptied(BLOCK);
    TEST_ASSERT_EQUAL_size_t(BLOCK->size, mmgr_cellblock_temporary_top(BLOCK));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)carcer_word(PAYLOAD, CARCER_DEAD), (uint64_t)word_read(top));
}
