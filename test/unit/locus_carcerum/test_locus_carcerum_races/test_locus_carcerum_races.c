/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_locus_carcerum_races.c
 * @brief What LocusCarcerum's branch-free paths do at each mask's edge, on demand and not by luck: the masks and
 *        picks, a bump the gap cannot meet, a pass that finds the cellblock closed, the sweep's merging and
 *        trimming, the owner word's completion bit, the slot table's claim bits and the option's none record.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-05
 *
 * @note Compiles the translation unit in, and so file-local entries are callable and the sink, the closed word
 *       and the claim bits are the suite's to set.
 * @note MMGR_FATAL jumps back to the case here. A prisoner from another cellblock has nothing to carry on from,
 *       and the jump is what lets a case read that it stopped.
 */
#include "locus_carcerum/locus_carcerum.h"

#include "embed_atomic.h"
#include "unity.h"

#include <setjmp.h>
#include <string.h>

static jmp_buf s_fatal;

#undef MMGR_FATAL
#define MMGR_FATAL(msg_) longjmp(s_fatal, 1)

// the owners and the misnamed hook are defined in this unit, below: the source leaves its table of none and its
// weak hook out
#define MMGR_OWNERS_IN_THIS_UNIT 1
#define MMGR_MISNAMED_IN_THIS_UNIT 1
#include "locus_carcerum/locus_carcerum.c"

#define RACED_BYTES 4096u
#define RACED_CELLS 70u
#define RACED_WIDTH 2u
#define RACED_ROWS 3u
#define PAYLOAD (MMGR_CARCER_ALIGN * 4u)

ParsMemoriaeInternae(raced, RACED_BYTES);
ParsMemoriaeInternae(raced_option, MMGR_MOVABLE_STORAGE_BYTES(RACED_CELLS, RACED_WIDTH, RACED_ROWS));
LocusCarcerum(cells, MMGR_MINIMUM_SECURITY(raced));

#define BLOCK (&cells_raced_ctx)

// The owners every case takes for, declared with the cellblocks as the program's table
#define RACED_OWNERS(owner_) owner_(s_one, "one") owner_(s_two, "two")
MMGR_OWNERS(RACED_OWNERS)

static size_t s_misnamed;

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    (void)named;
    (void)cell_owner;
    (void)bytes;
    s_misnamed += 1u;
}

void setUp(void)
{
    mmgr_owner_bytes_given(s_one, mmgr_owner_in_use(s_one));
    mmgr_owner_bytes_given(s_two, mmgr_owner_in_use(s_two));
    mmgr_series_cellularum_emptied(BLOCK);
    memset(mmgr_pars_storage_raced, 0, RACED_BYTES);
    s_misnamed = 0u;
}

void tearDown(void)
{
}

// The option assigned over its storage, zeroed
static void option_assigned(void)
{
    memset(mmgr_pars_storage_raced_option, 0, raced_option_bytes);
    TEST_ASSERT_TRUE(cells.raced.movable_init(&(CarcerMovableInitCfg){.storage = mmgr_pars_storage_raced_option,
                                                                      .storage_bytes = raced_option_bytes,
                                                                      .cells = RACED_CELLS,
                                                                      .width = RACED_WIDTH,
                                                                      .rows = RACED_ROWS}));
}

void test_a_mask_is_every_bit_or_none_and_a_pick_reads_one_side(void)
{
    TEST_ASSERT_EQUAL_size_t(SIZE_MAX, carcer_all(1));
    TEST_ASSERT_EQUAL_size_t(SIZE_MAX, carcer_all(7));
    TEST_ASSERT_EQUAL_size_t(0u, carcer_all(0));
    TEST_ASSERT_EQUAL_size_t(3u, carcer_pick(carcer_all(1), 3u, 5u));
    TEST_ASSERT_EQUAL_size_t(5u, carcer_pick(carcer_all(0), 3u, 5u));
    TEST_ASSERT_EQUAL_PTR(&s_misnamed, carcer_pick_at(carcer_all(1), &s_misnamed, NULL));
    TEST_ASSERT_NULL(carcer_pick_at(carcer_all(0), &s_misnamed, NULL));
}

void test_a_request_of_nothing_rounds_to_one_word(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, carcer_want(0u));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, carcer_want(1u));
    TEST_ASSERT_EQUAL_size_t(2u * MMGR_CARCER_ALIGN, carcer_want(MMGR_CARCER_ALIGN + 1u));
}

void test_a_store_a_pass_must_not_land_lands_on_the_sink(void)
{
    embed_atomic_usize *const live = carcer_word_picked(BLOCK, carcer_all(1), 0u);
    embed_atomic_usize *const sunk = carcer_word_picked(BLOCK, carcer_all(0), 0u);

    TEST_ASSERT_EQUAL_PTR(carcer_word_at(BLOCK, 0u), live);
    TEST_ASSERT_EQUAL_PTR(&BLOCK->sink, sunk);
    embed_atomic_usize_store(sunk, 77u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(carcer_word_at(BLOCK, 0u)),
                                     "the cellblock's bytes stand as they were");
}

void test_a_word_whose_place_is_past_the_table_finds_no_owner(void)
{
    const embed_atomic_usize_value past = (embed_atomic_usize_value)(mmgr_owner_table_count + 1u)
                                          << MMGR_OWNER_PLACE_SHIFT;

    TEST_ASSERT_NULL(carcer_owner_found(past));
    TEST_ASSERT_NULL(carcer_owner_found(0u));
    TEST_ASSERT_EQUAL_PTR(&carcer_owner_none, carcer_owner_at(0u));
    TEST_ASSERT_EQUAL_PTR(&carcer_owner_none, carcer_owner_at(mmgr_owner_table_count + 1u));
    TEST_ASSERT_EQUAL_PTR(s_one, carcer_owner_at(s_one->place));
}

void test_an_owner_word_is_read_back_as_its_record(void)
{
    const embed_atomic_usize_value named = carcer_owner_word(s_two);

    TEST_ASSERT_EQUAL_PTR(s_two, carcer_owner_found(named));
    TEST_ASSERT_EQUAL_size_t(SIZE_MAX, carcer_owner_is(named, s_two));
    TEST_ASSERT_EQUAL_size_t(0u, carcer_owner_is(named, s_one));
    TEST_ASSERT_FALSE(mmgr_owner_listed(NULL));
    TEST_ASSERT_FALSE(mmgr_owner_listed(&carcer_owner_none));
    TEST_ASSERT_TRUE(mmgr_owner_listed(s_one));
}

void test_a_foot_is_counted_off_once_and_a_claim_not_permitted_touches_nothing(void)
{
    uint8_t *const prisoner = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    const size_t off = carcer_off_of(BLOCK, prisoner);

    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_one));
    carcer_foot_claimed(BLOCK, off, PAYLOAD, carcer_all(1), carcer_all(0));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(PAYLOAD, mmgr_owner_in_use(s_one), "a claim not permitted counts nothing");
    carcer_foot_claimed(BLOCK, off, PAYLOAD, carcer_all(1), carcer_all(1));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_owner_in_use(s_one), "the first claim counts the cell off");
    carcer_foot_claimed(BLOCK, off, PAYLOAD, carcer_all(1), carcer_all(1));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_owner_in_use(s_one), "a second claim finds the completion bit set");
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&BLOCK->in_use));
}

void test_a_bump_the_gap_cannot_meet_leaves_the_boundary_moved_records_the_overrun_and_lays_its_words_on_the_sink(void)
{
    const CarcerTakeCtx take = {RACED_BYTES, s_one, 0u, carcer_all(1)};
    const uintptr_t answer = carcer_bumped(BLOCK, &take, carcer_all(1));

    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)answer);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(CARCER_HDR + RACED_BYTES, mmgr_series_cellularum_persistent_end(BLOCK),
                                     "the end stands where the bump moved it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(CARCER_HDR + RACED_BYTES, (size_t)embed_atomic_usize_load(&BLOCK->persistent_overrun),
                                     "the overrun records the bump's bytes");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_usize_load(carcer_word_at(BLOCK, 0u)),
                                     "no header was laid");
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(s_one));
    const CarcerTakeCtx down = {RACED_BYTES, s_one, carcer_all(1), carcer_all(1)};

    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)carcer_bumped(BLOCK, &down, carcer_all(1)));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(((size_t)RACED_BYTES - (CARCER_HDR + RACED_BYTES)) & (size_t)MMGR_CARCER_HALF_MASK,
                                     mmgr_series_cellularum_temporary_top(BLOCK),
                                     "the top stands where the bump moved it, below zero within its half");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(CARCER_HDR + RACED_BYTES, (size_t)embed_atomic_usize_load(&BLOCK->temporary_overrun),
                                     "the overrun records the bump's bytes");
    carcer_settled(BLOCK);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_series_cellularum_persistent_end(BLOCK), "the settle takes the overrun back");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(RACED_BYTES, mmgr_series_cellularum_temporary_top(BLOCK), "the settle takes the overrun back");
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&BLOCK->persistent_overrun));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&BLOCK->temporary_overrun));
}

void test_after_a_refused_bump_every_bump_of_the_tier_is_refused_until_a_writer_settles(void)
{
    const CarcerTakeCtx wide = {RACED_BYTES, s_one, 0u, carcer_all(1)};
    const CarcerTakeCtx narrow = {PAYLOAD, s_two, 0u, carcer_all(1)};

    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)carcer_bumped(BLOCK, &wide, carcer_all(1)));
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, (uint64_t)carcer_bumped(BLOCK, &narrow, carcer_all(1)),
                                     "a bump that would fit the gap is refused behind a refused one");
    TEST_ASSERT_EQUAL_size_t(2u * CARCER_HDR + RACED_BYTES + PAYLOAD, mmgr_series_cellularum_persistent_end(BLOCK));
    carcer_settled(BLOCK);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_series_cellularum_persistent_end(BLOCK));
    TEST_ASSERT_NOT_EQUAL(0u, (uint64_t)carcer_bumped(BLOCK, &narrow, carcer_all(1)));
    TEST_ASSERT_EQUAL_size_t(CARCER_HDR + PAYLOAD, mmgr_series_cellularum_persistent_end(BLOCK));
}

void test_a_bump_not_permitted_moves_nothing(void)
{
    const CarcerTakeCtx take = {PAYLOAD, s_one, 0u, carcer_all(1)};

    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)carcer_bumped(BLOCK, &take, carcer_all(0)));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_series_cellularum_persistent_end(BLOCK));
    const CarcerTakeCtx unlisted = {PAYLOAD, s_one, 0u, carcer_all(0)};

    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)carcer_bumped(BLOCK, &unlisted, carcer_all(1)));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_series_cellularum_persistent_end(BLOCK));
}

void test_a_bump_that_lands_lays_its_header_and_owner_word_and_counts(void)
{
    const CarcerTakeCtx take = {PAYLOAD, s_two, 0u, carcer_all(1)};
    const uintptr_t answer = carcer_bumped(BLOCK, &take, carcer_all(1));

    TEST_ASSERT_EQUAL_PTR(BLOCK->base + CARCER_WORD, (void *)answer);
    TEST_ASSERT_EQUAL_size_t(carcer_end_of(0u, PAYLOAD), mmgr_series_cellularum_persistent_end(BLOCK));
    TEST_ASSERT_EQUAL_size_t(carcer_word(PAYLOAD, CARCER_BUSY), (size_t)embed_atomic_usize_load(carcer_word_at(BLOCK, 0u)));
    TEST_ASSERT_EQUAL_PTR(s_two, mmgr_owner_of_buf(BLOCK, (void *)answer));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_two));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, (size_t)embed_atomic_usize_load(&BLOCK->persistent_in_use));
}

void test_a_pass_that_finds_the_series_cellularum_closed_misses_and_touches_nothing(void)
{
    const CarcerTakeCtx take = {PAYLOAD, s_one, 0u, carcer_all(1)};
    size_t missed = 0u;

    (void)embed_atomic_word_fetch_or(&BLOCK->closed, CARCER_CLOSED);
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)carcer_attempted(BLOCK, s_one->place, carcer_bumped, &take, &missed));
    TEST_ASSERT_EQUAL_size_t(SIZE_MAX, missed);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_series_cellularum_persistent_end(BLOCK));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)embed_atomic_word_load(&BLOCK->turnstiles.busy) & EMBED_TURNSTILE_LANE_BITS,
                                     "the lane is let go");
    (void)embed_atomic_word_fetch_and(&BLOCK->closed, ~CARCER_CLOSED);
    TEST_ASSERT_NOT_EQUAL(0u, (uint64_t)carcer_attempted(BLOCK, s_one->place, carcer_bumped, &take, &missed));
    TEST_ASSERT_EQUAL_size_t(0u, missed);
}

void test_a_pass_through_a_closed_series_cellularum_opened_meanwhile_lands_on_its_second_try(void)
{
    const CarcerTakeCtx take = {PAYLOAD, s_one, 0u, carcer_all(1)};

    // the first attempt reads the closed bit; the writer opens before the backoff's wait, with no sleeper to wake,
    // and the second attempt lands. The weak wait answers at once, which is the host's answer where nothing sleeps
    (void)embed_atomic_word_fetch_or(&BLOCK->closed, CARCER_CLOSED);
    (void)embed_atomic_word_fetch_and(&BLOCK->closed, ~CARCER_CLOSED);
    TEST_ASSERT_NOT_EQUAL(0u, (uint64_t)carcer_passed(BLOCK, s_one->place, carcer_bumped, &take));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_one));
}

void test_a_closed_series_cellularum_opened_wakes_the_sleepers_and_clears_both_bits(void)
{
    (void)embed_atomic_word_fetch_or(&BLOCK->closed, CARCER_CLOSED | CARCER_CLOSED_SLEEPERS);
    carcer_opened(BLOCK);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_word_load(&BLOCK->closed));
    carcer_shut(BLOCK);
    TEST_ASSERT_EQUAL_size_t(CARCER_CLOSED, (size_t)embed_atomic_word_load(&BLOCK->closed));
    carcer_opened(BLOCK);
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_word_load(&BLOCK->closed));
}

void test_the_sweep_merges_free_runs_takes_the_first_that_fits_and_trims_the_end(void)
{
    uint8_t *const one = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    uint8_t *const two = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    uint8_t *const held = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    uint8_t *const three = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    const size_t end_with_four = mmgr_series_cellularum_persistent_end(BLOCK);

    // the two below the held cell and the one above it set free by their headers alone: no trim runs
    (void)carcer_given(BLOCK, one, s_one, 0u, carcer_all(1));
    (void)carcer_given(BLOCK, two, s_one, 0u, carcer_all(1));
    (void)carcer_given(BLOCK, three, s_one, 0u, carcer_all(1));
    TEST_ASSERT_EQUAL_size_t(end_with_four, mmgr_series_cellularum_persistent_end(BLOCK));
    const CarcerSweep sweep = carcer_swept(BLOCK, (2u * PAYLOAD) + CARCER_HDR, RACED_BYTES, carcer_all(1));

    TEST_ASSERT_EQUAL_size_t_MESSAGE(SIZE_MAX, sweep.found, "the merged run below the held cell holds the take");
    TEST_ASSERT_EQUAL_size_t(carcer_off_of(BLOCK, one), sweep.at);
    TEST_ASSERT_EQUAL_size_t((2u * PAYLOAD) + CARCER_HDR, sweep.size);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(carcer_off_of(BLOCK, three), sweep.end, "the run reaching the end goes to the gap");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(carcer_word((2u * PAYLOAD) + CARCER_HDR, CARCER_FREE),
                                     (size_t)embed_atomic_usize_load(carcer_word_at(BLOCK, sweep.at)),
                                     "the run is laid as one free cell");
    TEST_ASSERT_NOT_NULL(held);
}

void test_a_sweep_not_permitted_reads_nothing_and_keeps_the_end(void)
{
    uint8_t *const one = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    const size_t end = mmgr_series_cellularum_persistent_end(BLOCK);
    const CarcerSweep sweep = carcer_swept(BLOCK, PAYLOAD, RACED_BYTES, carcer_all(0));

    TEST_ASSERT_EQUAL_size_t(0u, sweep.found);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(end, sweep.end, "a walk of nothing leaves the end where it stands");
    TEST_ASSERT_NOT_NULL(one);
}

void test_a_sweep_takes_no_run_ending_above_its_bound(void)
{
    uint8_t *const low = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    uint8_t *const held = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    uint8_t *const high = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    uint8_t *const top = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);

    (void)carcer_given(BLOCK, low, s_one, 0u, carcer_all(1));
    (void)carcer_given(BLOCK, high, s_one, 0u, carcer_all(1));
    const CarcerSweep below_held = carcer_swept(BLOCK, PAYLOAD, carcer_off_of(BLOCK, held), carcer_all(1));

    TEST_ASSERT_EQUAL_size_t(carcer_off_of(BLOCK, low), below_held.at);
    const CarcerSweep below_low = carcer_swept(BLOCK, PAYLOAD, 0u, carcer_all(1));

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, below_low.found, "no run ends at or below the lowest cell");
    TEST_ASSERT_NOT_NULL(top);
}

void test_a_split_lays_the_rest_as_a_free_cell_and_keeps_a_run_too_small_to_split(void)
{
    uint8_t *const large = cells.raced.persistent_buf_alloc(4u * PAYLOAD, s_one);
    uint8_t *const held = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);

    (void)carcer_given(BLOCK, large, s_one, 0u, carcer_all(1));
    const CarcerSweep sweep = carcer_swept(BLOCK, PAYLOAD, RACED_BYTES, carcer_all(1));

    TEST_ASSERT_EQUAL_size_t(PAYLOAD, carcer_split(BLOCK, sweep, PAYLOAD));
    const size_t rest_at = carcer_end_of(sweep.at, PAYLOAD);

    TEST_ASSERT_EQUAL_size_t(carcer_word((3u * PAYLOAD) - CARCER_HDR, CARCER_FREE),
                             (size_t)embed_atomic_usize_load(carcer_word_at(BLOCK, rest_at)));
    const CarcerSweep whole = {carcer_all(1), sweep.at, PAYLOAD + MMGR_CARCER_ALIGN, sweep.end};

    TEST_ASSERT_EQUAL_size_t_MESSAGE(PAYLOAD + MMGR_CARCER_ALIGN, carcer_split(BLOCK, whole, PAYLOAD),
                                     "a rest too small for a cell of its own stays with the take");
    TEST_ASSERT_NOT_NULL(held);
}

void test_a_give_naming_another_owner_changes_nothing_and_is_told(void)
{
    uint8_t *const prisoner = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);

    TEST_ASSERT_EQUAL_size_t(0u, carcer_given(BLOCK, prisoner, s_two, carcer_all(1), carcer_all(1)));
    TEST_ASSERT_EQUAL_size_t(1u, s_misnamed);
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_one));
    TEST_ASSERT_EQUAL_PTR(s_one, mmgr_owner_of_buf(BLOCK, prisoner));
}

void test_a_give_answers_the_boundary_and_the_tier_the_cell_stood_at(void)
{
    uint8_t *const low = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    uint8_t *const high = cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    uint8_t *const temporary = cells.raced.temporary_buf_alloc(PAYLOAD, s_one);

    TEST_ASSERT_EQUAL_size_t(CARCER_GIVEN_BACK | CARCER_GIVEN_PERSISTENT, carcer_given(BLOCK, low, s_one, 0u, carcer_all(1)));
    TEST_ASSERT_EQUAL_size_t(CARCER_GIVEN_BACK | CARCER_GIVEN_PERSISTENT | CARCER_GIVEN_AT_BOUNDARY,
                             carcer_given(BLOCK, high, s_one, 0u, carcer_all(1)));
    TEST_ASSERT_EQUAL_size_t(CARCER_GIVEN_BACK | CARCER_GIVEN_AT_BOUNDARY,
                             carcer_given(BLOCK, temporary, s_one, 0u, carcer_all(1)));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, carcer_given(BLOCK, temporary, s_one, 0u, carcer_all(1)),
                                     "a cell given back already goes back no second time");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, carcer_given(BLOCK, NULL, s_one, 0u, carcer_all(1)), "nothing to give");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, carcer_given(BLOCK, low, NULL, 0u, carcer_all(1)), "no owner named");
}

void test_a_prisoner_from_another_series_cellularum_stops_the_program(void)
{
    static uint8_t s_elsewhere[64];

    if (setjmp(s_fatal) == 0)
    {
        cells.raced.persistent_buf_release(s_elsewhere + CARCER_WORD, s_one);
        TEST_FAIL_MESSAGE("a prisoner from another cellblock was released");
    }
    TEST_PASS_MESSAGE("the release stopped in place");
}

void test_the_trim_walks_the_top_up_over_free_cells_and_stops_at_a_busy_one(void)
{
    uint8_t *const upper = cells.raced.temporary_buf_alloc(PAYLOAD, s_one);
    uint8_t *const held = cells.raced.temporary_buf_alloc(PAYLOAD, s_one);
    uint8_t *const lower = cells.raced.temporary_buf_alloc(PAYLOAD, s_one);

    (void)carcer_given(BLOCK, upper, s_one, 0u, carcer_all(1));
    (void)carcer_given(BLOCK, lower, s_one, 0u, carcer_all(1));
    carcer_trimmed(BLOCK, 0u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(carcer_off_of(BLOCK, held), mmgr_series_cellularum_temporary_top(BLOCK),
                                     "the top walks up to the held cell");
    (void)carcer_given(BLOCK, held, s_one, 0u, carcer_all(1));
    carcer_trimmed(BLOCK, 0u);
    TEST_ASSERT_EQUAL_size_t(RACED_BYTES, mmgr_series_cellularum_temporary_top(BLOCK));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_word_load(&BLOCK->closed));
}

void test_a_rewind_to_a_mark_past_the_series_cellularum_or_below_the_top_stops_at_the_top(void)
{
    uint8_t *const prisoner = cells.raced.temporary_buf_alloc(PAYLOAD, s_one);
    const size_t top = mmgr_series_cellularum_temporary_top(BLOCK);

    cells.raced.temporary_buf_release(RACED_BYTES + MMGR_CARCER_ALIGN);
    TEST_ASSERT_EQUAL_size_t(top, mmgr_series_cellularum_temporary_top(BLOCK));
    cells.raced.temporary_buf_release(top - MMGR_CARCER_ALIGN);
    TEST_ASSERT_EQUAL_size_t(top, mmgr_series_cellularum_temporary_top(BLOCK));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_one));
    cells.raced.temporary_buf_release(RACED_BYTES);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(s_one));
    TEST_ASSERT_NOT_NULL(prisoner);
}

void test_zeroing_starts_at_an_address_off_a_word_boundary(void)
{
    static uint8_t s_bytes[4u * MMGR_CARCER_ALIGN];

    memset(s_bytes, 0xA5, sizeof(s_bytes));
    mmgr_zero_buf(s_bytes + 1u, (2u * MMGR_CARCER_ALIGN) + 1u);
    TEST_ASSERT_EQUAL_HEX8(0xA5u, s_bytes[0]);
    for (size_t at = 1u; at < (2u * MMGR_CARCER_ALIGN) + 2u; at += 1u)
    {
        TEST_ASSERT_EQUAL_HEX8(0x00u, s_bytes[at]);
    }
    TEST_ASSERT_EQUAL_HEX8(0xA5u, s_bytes[(2u * MMGR_CARCER_ALIGN) + 2u]);
}

void test_a_claim_sets_the_lowest_free_bit_and_a_full_table_claims_nothing(void)
{
    option_assigned();
    CarcerMovable *const movable = carcer_movable_of(BLOCK);

    const size_t words = (RACED_CELLS + MMGR_MOVABLE_CLAIM_BITS - 1u) / MMGR_MOVABLE_CLAIM_BITS;

    TEST_ASSERT_EQUAL_size_t(words, movable->claimed_words);
    // the bits past the cells in the last word are set for good
    const embed_atomic_usize_value past = embed_atomic_usize_load(&movable->claimed[words - 1u]);

    TEST_ASSERT_EQUAL_size_t(RACED_CELLS - ((words - 1u) * MMGR_MOVABLE_CLAIM_BITS), (size_t)embed_turnstile_lowest(past));
    for (size_t at = 0u; at < RACED_CELLS; at += 1u)
    {
        TEST_ASSERT_EQUAL_PTR(&movable->slot[at], carcer_slot_claimed(movable, CARCER_WORD, carcer_all(1)));
    }
    TEST_ASSERT_NULL_MESSAGE(carcer_slot_claimed(movable, CARCER_WORD, carcer_all(1)), "every slot is named");
    TEST_ASSERT_EQUAL_size_t(RACED_CELLS, (size_t)embed_atomic_usize_load(&movable->named));
}

void test_a_claim_not_permitted_names_nothing(void)
{
    option_assigned();
    CarcerMovable *const movable = carcer_movable_of(BLOCK);

    TEST_ASSERT_NULL(carcer_slot_claimed(movable, CARCER_WORD, carcer_all(0)));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&movable->claimed[0]));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_usize_load(&movable->named));
}

void test_a_claim_lost_to_another_context_names_nothing_and_the_take_tries_once_more(void)
{
    option_assigned();
    CarcerMovable *const movable = carcer_movable_of(BLOCK);

    // every bit of the first word set as another context's claims would set them: the search finds the second word
    embed_atomic_usize_store(&movable->claimed[0], ~(embed_atomic_usize_value)0u);
    CarcerMovableSlot *const slot = carcer_slot_claimed(movable, CARCER_WORD, carcer_all(1));

    TEST_ASSERT_EQUAL_PTR(&movable->slot[MMGR_MOVABLE_CLAIM_BITS], slot);
    TEST_ASSERT_EQUAL_size_t(1u, (size_t)embed_atomic_usize_load(&movable->lowest));
}

void test_the_highest_named_cell_below_a_bound_and_under_a_size(void)
{
    option_assigned();
    const CarcerMovable *const movable = carcer_movable_of(BLOCK);
    const CarcerMovableSlot *const low = cells.raced.movable_buf_alloc(PAYLOAD, s_one);
    const CarcerMovableSlot *const high = cells.raced.movable_buf_alloc(2u * PAYLOAD, s_one);
    size_t payload = 0u;

    TEST_ASSERT_EQUAL_PTR(high, carcer_slot_highest(movable, BLOCK, RACED_BYTES, RACED_BYTES, &payload));
    TEST_ASSERT_EQUAL_size_t((size_t)embed_atomic_usize_load(&high->word), payload);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(low, carcer_slot_highest(movable, BLOCK, RACED_BYTES, 2u * PAYLOAD, &payload),
                                  "a cell at the size refused is passed over");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&carcer_slot_none, carcer_slot_highest(movable, BLOCK, 0u, RACED_BYTES, &payload),
                                  "nothing lies below the first byte");
    TEST_ASSERT_EQUAL_size_t(0u, payload);
}

void test_with_no_option_there_is_no_line_and_no_row(void)
{
    size_t id = 1u;
    size_t touches = 1u;

    TEST_ASSERT_EQUAL_size_t(0u, mmgr_movable_queue_depth(BLOCK));
    TEST_ASSERT_FALSE(mmgr_movable_touches(BLOCK, 0u, &id, &touches));
    TEST_ASSERT_EQUAL_size_t(0u, id);
    TEST_ASSERT_EQUAL_size_t(0u, touches);
    TEST_ASSERT_FALSE(mmgr_movable_buf_move(BLOCK));
    TEST_ASSERT_NULL(cells.raced.movable_buf_alloc(PAYLOAD, s_one));
    TEST_ASSERT_FALSE(cells.raced.movable_buf_release(&carcer_slot_none, s_one));
    TEST_ASSERT_EQUAL_INT(1, (int)cells.raced.movable_buf_compare(&(CarcerMovableCfg){.cell = NULL, .bytes = 1u}));
}

void test_a_span_past_a_cell_or_of_a_free_slot_does_not_fit(void)
{
    option_assigned();
    const CarcerMovableSlot *const cell = cells.raced.movable_buf_alloc(PAYLOAD, s_one);
    const CarcerSpan inside = carcer_span_read(BLOCK, cell, 0u, PAYLOAD, carcer_all(1));
    const CarcerSpan past = carcer_span_read(BLOCK, cell, 1u, PAYLOAD, carcer_all(1));
    const CarcerSpan none = carcer_span_read(BLOCK, &carcer_slot_none, 0u, 0u, carcer_all(1));
    const CarcerSpan refused = carcer_span_read(BLOCK, cell, 0u, PAYLOAD, carcer_all(0));

    TEST_ASSERT_EQUAL_size_t(SIZE_MAX, inside.fits);
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, inside.room);
    TEST_ASSERT_EQUAL_size_t(0u, past.fits);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(BLOCK->base, past.at, "a span that does not fit starts at the cellblock's first byte");
    TEST_ASSERT_EQUAL_size_t(0u, none.fits);
    TEST_ASSERT_EQUAL_size_t(0u, refused.fits);
}

void test_emptying_puts_the_record_back_at_rest(void)
{
    option_assigned();
    (void)cells.raced.persistent_buf_alloc(PAYLOAD, s_one);
    (void)cells.raced.temporary_buf_alloc(PAYLOAD, s_one);
    (void)embed_atomic_word_fetch_or(&BLOCK->closed, CARCER_CLOSED);
    mmgr_series_cellularum_emptied(BLOCK);
    TEST_ASSERT_TRUE(mmgr_series_cellularum_idle(BLOCK));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_word_load(&BLOCK->closed));
    TEST_ASSERT_EQUAL_size_t(0u, (size_t)embed_atomic_word_load(&BLOCK->turnstiles.busy));
    TEST_ASSERT_EQUAL_size_t(1u, (size_t)embed_turnstile_lanes(&BLOCK->turnstiles));
    TEST_ASSERT_NULL(carcer_movable_of(BLOCK));
}
