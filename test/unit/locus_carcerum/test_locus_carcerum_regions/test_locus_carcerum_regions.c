/*
 * MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_locus_carcerum_regions.c
 * @brief The temporary tier as regions: a take in the region the thread's place selects, a take over a region's room
 *        in the large region, a give that trims the region that holds the cell from any thread, the sweep inside a
 *        region, the large region's turn at a take a full region refuses, the refusal when the large region is
 *        full as well, the growth of a region's newest cell, and the gap below the regions.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-18
 *
 * @note Compiles the translation unit in with two regions of 1024 bytes and a large region of 2048, so the knobs are
 *       the suite's and the file-local helpers are callable.
 */
#define MMGR_CARCER_REGIONS 2u
#define MMGR_CARCER_REGION_BYTES 1024u
#define MMGR_CARCER_LARGE_REGION_BYTES 2048u
#include "locus_carcerum/locus_carcerum.h"

#include "embed_atomic.h"
#include "host_threads.h"
#include "unity.h"

#include <string.h>

// the owners and the misnamed hook have their definitions in this unit, below: the source leaves its table of none and
// its weak hook out
#define MMGR_OWNERS_IN_THIS_UNIT 1
#define MMGR_MISNAMED_IN_THIS_UNIT 1
#include "locus_carcerum/locus_carcerum.c"

#define REGIONED_BYTES 8192u
#define PAYLOAD (MMGR_CARCER_ALIGN * 4u)
#define CELL (CARCER_HDR + PAYLOAD)
#define LARGE ((size_t)MMGR_CARCER_REGIONS)
#define ROUNDS 200u

ParsMemoriaeInternae(regioned, REGIONED_BYTES);
LocusCarcerum(cells, MMGR_MINIMUM_SECURITY(regioned));

#define BLOCK (&cells_regioned_ctx)
#define GUARD (&cells.regioned)

// A cellblock over a hospes pool, and the storage a case places it on
ParsMemoriaeHospes(given);
LocusCarcerum(given_site, MMGR_MINIMUM_SECURITY_HOSPES(given));
EMBED_ALIGN(MMGR_ALIGN_BYTES) static uint8_t s_given[REGIONED_BYTES];

// The owners every case takes for, declared with the cellblock as the program's table
#define REGIONED_OWNERS(owner_) owner_(s_one, "one") owner_(s_two, "two")
MMGR_OWNERS(REGIONED_OWNERS)

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
    memset(mmgr_pars_storage_regioned, 0, REGIONED_BYTES);
    s_misnamed = 0u;
}

void tearDown(void)
{
}

// The region that holds a prisoner of the temporary tier
static size_t region_of(const void *prisoner)
{
    return carcer_region_of_off(BLOCK, carcer_off_of(BLOCK, prisoner), carcer_all(1));
}

// The region this thread's small takes go to
static size_t own_region(void)
{
    return carcer_region_of_place(PAYLOAD);
}

// The depth of one region
static size_t depth_of(size_t region)
{
    return mmgr_series_cellularum_region_depth(BLOCK, region);
}

// This thread's region filled: one cell of PAYLOAD at its end, and one over the rest. The first cell is answered.
static void *region_filled(void **rest)
{
    void *const first = GUARD->temporary_buf_alloc(PAYLOAD, s_one);

    *rest = GUARD->temporary_buf_alloc(MMGR_CARCER_REGION_BYTES - CELL - CARCER_HDR, s_one);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(*rest);
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_REGION_BYTES, depth_of(own_region()));
    return first;
}

void test_a_temporary_take_lands_in_the_region_of_the_thread_and_deepens_it_by_the_cell(void)
{
    void *const prisoner = GUARD->temporary_buf_alloc(PAYLOAD, s_one);
    const size_t region = own_region();

    TEST_ASSERT_NOT_NULL(prisoner);
    TEST_ASSERT_TRUE(region < MMGR_CARCER_REGIONS);
    TEST_ASSERT_EQUAL_size_t(region, region_of(prisoner));
    TEST_ASSERT_EQUAL_size_t(CELL, depth_of(region));
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(1u - region));
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(LARGE));
    TEST_ASSERT_EQUAL_size_t(REGIONED_BYTES - MMGR_CARCER_REGION_AREA, mmgr_series_cellularum_temporary_top(BLOCK));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_one));
}

void test_a_take_over_a_regions_room_lands_in_the_large_region(void)
{
    void *const prisoner = GUARD->temporary_buf_alloc(MMGR_CARCER_REGION_BYTES, s_one);

    TEST_ASSERT_NOT_NULL(prisoner);
    TEST_ASSERT_EQUAL_size_t(LARGE, region_of(prisoner));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_REGION_BYTES + CARCER_HDR, depth_of(LARGE));
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(own_region()));
    GUARD->persistent_buf_release(prisoner, s_one);
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(LARGE));
    TEST_ASSERT_TRUE(mmgr_series_cellularum_idle(BLOCK));
}

void test_a_give_of_the_newest_cell_returns_the_region_to_empty_and_the_cellblock_reads_idle(void)
{
    void *const prisoner = GUARD->temporary_buf_alloc(PAYLOAD, s_one);

    GUARD->persistent_buf_release(prisoner, s_one);
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(own_region()));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(s_one));
    TEST_ASSERT_TRUE(mmgr_series_cellularum_idle(BLOCK));
}

void test_a_take_the_region_cannot_hold_sweeps_the_region_and_takes_a_freed_run(void)
{
    void *rest;
    void *const first = region_filled(&rest);

    // the first cell is at the region's end, under the rest: its give leaves a free cell below no top
    GUARD->persistent_buf_release(first, s_one);
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_REGION_BYTES, depth_of(own_region()));
    void *const again = GUARD->temporary_buf_alloc(PAYLOAD, s_two);

    TEST_ASSERT_EQUAL_PTR(first, again);
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_REGION_BYTES, depth_of(own_region()));
    TEST_ASSERT_EQUAL_PTR(s_two, mmgr_owner_of_buf(BLOCK, again));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_two));
}

void test_a_take_no_path_of_the_thread_region_holds_lands_in_the_large_region(void)
{
    void *rest;
    void *const first = region_filled(&rest);
    void *const spilled = GUARD->temporary_buf_alloc(PAYLOAD, s_two);

    TEST_ASSERT_NOT_NULL(spilled);
    TEST_ASSERT_EQUAL_size_t(LARGE, region_of(spilled));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_REGION_BYTES, depth_of(own_region()));
    TEST_ASSERT_EQUAL_size_t(CELL, depth_of(LARGE));
    TEST_ASSERT_EQUAL_PTR(s_two, mmgr_owner_of_buf(BLOCK, spilled));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_two));
    TEST_ASSERT_EQUAL_PTR(s_one, mmgr_owner_of_buf(BLOCK, first));
    TEST_ASSERT_EQUAL_PTR(s_one, mmgr_owner_of_buf(BLOCK, rest));
}

void test_a_take_neither_the_thread_region_nor_the_large_region_holds_is_refused_and_changes_no_cell(void)
{
    void *rest;
    void *const first = region_filled(&rest);
    void *const whole = GUARD->temporary_buf_alloc(MMGR_CARCER_LARGE_REGION_BYTES - CARCER_HDR, s_one);
    const size_t in_use = mmgr_owner_in_use(s_one);

    TEST_ASSERT_NOT_NULL(whole);
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_LARGE_REGION_BYTES, depth_of(LARGE));
    TEST_ASSERT_NULL(GUARD->temporary_buf_alloc(PAYLOAD, s_two));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_REGION_BYTES, depth_of(own_region()));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_LARGE_REGION_BYTES, depth_of(LARGE));
    TEST_ASSERT_EQUAL_size_t(in_use, mmgr_owner_in_use(s_one));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(s_two));
    TEST_ASSERT_EQUAL_PTR(s_one, mmgr_owner_of_buf(BLOCK, first));
    TEST_ASSERT_EQUAL_PTR(s_one, mmgr_owner_of_buf(BLOCK, rest));
    TEST_ASSERT_EQUAL_PTR(s_one, mmgr_owner_of_buf(BLOCK, whole));
}

// The spans one walk of the idle storage passed: how many, and each one's start and bytes
typedef struct
{
    size_t count;
    const unsigned char *at[MMGR_CARCER_REGION_COUNT + 1u];
    size_t bytes[MMGR_CARCER_REGION_COUNT + 1u];
} IdleSpans;

static void idle_span_each(void *context, void *at, size_t bytes)
{
    IdleSpans *const spans = (IdleSpans *)context;

    if (spans->count < (MMGR_CARCER_REGION_COUNT + 1u))
    {
        spans->at[spans->count] = (const unsigned char *)at;
        spans->bytes[spans->count] = bytes;
    }
    spans->count += 1u;
}

void test_the_idle_spans_are_the_gap_and_each_regions_room_above_its_depth(void)
{
    void *const persistent = GUARD->persistent_buf_alloc(PAYLOAD, s_one);
    void *const temporary = GUARD->temporary_buf_alloc(PAYLOAD, s_two);
    IdleSpans spans = {0u, {NULL}, {0u}};

    TEST_ASSERT_NOT_NULL(persistent);
    TEST_ASSERT_NOT_NULL(temporary);
    mmgr_series_cellularum_idle_each(BLOCK, idle_span_each, &spans);
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_REGION_COUNT + 1u, spans.count);
    // the gap: from the persistent end, past the one cell, to the regions' area
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_regioned + CELL, spans.at[0]);
    TEST_ASSERT_EQUAL_size_t(REGIONED_BYTES - MMGR_CARCER_REGION_AREA - CELL, spans.bytes[0]);
    // each thread's region from its start to its depth, this thread's one cell less
    for (size_t region = 0u; region < MMGR_CARCER_REGIONS; region += 1u)
    {
        const size_t held = (region == own_region()) ? CELL : 0u;
        const size_t end = REGIONED_BYTES - MMGR_CARCER_LARGE_REGION_BYTES - (region * MMGR_CARCER_REGION_BYTES);

        TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_regioned + end - MMGR_CARCER_REGION_BYTES, spans.at[1u + region]);
        TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_REGION_BYTES - held, spans.bytes[1u + region]);
    }
    // the large region, whole
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_regioned + REGIONED_BYTES - MMGR_CARCER_LARGE_REGION_BYTES,
                          spans.at[1u + MMGR_CARCER_REGIONS]);
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_LARGE_REGION_BYTES, spans.bytes[1u + MMGR_CARCER_REGIONS]);
    // the cellblock is open again: a take goes through
    TEST_ASSERT_NOT_NULL(GUARD->temporary_buf_alloc(PAYLOAD, s_two));
}

void test_a_cellblock_over_a_hospes_pool_refuses_every_take_until_placed_and_then_takes_from_the_storage_given(void)
{
    mmgr_series_cellularum_emptied(&given_site_given_ctx);
    given_site_given_ctx.base = NULL;
    given_site_given_ctx.size = 0u;
    TEST_ASSERT_NULL(given_site.given.persistent_buf_alloc(PAYLOAD, s_one));
    TEST_ASSERT_NULL(given_site.given.temporary_buf_alloc(PAYLOAD, s_one));
    // a give, a grow and an owner read on the unplaced cellblock read no byte and change nothing
    given_site.given.persistent_buf_release(NULL, s_one);
    given_site.given.persistent_buf_release(s_given, s_one);
    TEST_ASSERT_NULL(given_site.given.buf_grow(NULL, PAYLOAD, s_one));
    TEST_ASSERT_NULL(mmgr_owner_of_buf(&given_site_given_ctx, NULL));
    TEST_ASSERT_NULL(mmgr_owner_of_buf(&given_site_given_ctx, s_given));
    TEST_ASSERT_FALSE(mmgr_who_owns_buf(&given_site_given_ctx, s_given));
    TEST_ASSERT_FALSE(mmgr_series_cellularum_placed(&given_site_given_ctx, NULL, REGIONED_BYTES));
    TEST_ASSERT_FALSE(mmgr_series_cellularum_placed(&given_site_given_ctx, s_given, MMGR_CARCER_REGION_AREA));
    TEST_ASSERT_NULL(given_site.given.persistent_buf_alloc(PAYLOAD, s_one));
    TEST_ASSERT_TRUE(mmgr_series_cellularum_placed(&given_site_given_ctx, s_given, REGIONED_BYTES));
    void *const persistent = given_site.given.persistent_buf_alloc(PAYLOAD, s_one);
    void *const temporary = given_site.given.temporary_buf_alloc(PAYLOAD, s_two);

    TEST_ASSERT_NOT_NULL(persistent);
    TEST_ASSERT_NOT_NULL(temporary);
    TEST_ASSERT_TRUE(((const uint8_t *)persistent >= s_given) && ((const uint8_t *)persistent < s_given + REGIONED_BYTES));
    TEST_ASSERT_TRUE(((const uint8_t *)temporary >= s_given) && ((const uint8_t *)temporary < s_given + REGIONED_BYTES));
    given_site.given.persistent_buf_release(persistent, s_one);
    given_site.given.persistent_buf_release(temporary, s_two);
    TEST_ASSERT_TRUE(mmgr_series_cellularum_idle(&given_site_given_ctx));
}

void test_a_temporary_take_over_the_large_regions_room_is_refused_ahead_of_the_pass(void)
{
    TEST_ASSERT_NULL(GUARD->temporary_buf_alloc(MMGR_CARCER_LARGE_REGION_BYTES, s_one));
    TEST_ASSERT_NULL(GUARD->temporary_buf_alloc(MMGR_CARCER_LARGE_REGION_BYTES - CARCER_HDR + 1u, s_one));
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(LARGE));
    TEST_ASSERT_TRUE(mmgr_series_cellularum_idle(BLOCK));
}

void test_two_owners_that_give_under_each_other_sink_the_region_to_its_size_and_the_sweep_holds_it_there(void)
{
    void *one = GUARD->temporary_buf_alloc(PAYLOAD, s_one);
    void *two = GUARD->temporary_buf_alloc(PAYLOAD, s_two);
    const size_t region = own_region();
    size_t deepest = 0u;
    size_t round;

    for (round = 0u; round < ROUNDS; round++)
    {
        GUARD->persistent_buf_release(one, s_one);
        one = GUARD->temporary_buf_alloc(PAYLOAD, s_one);
        GUARD->persistent_buf_release(two, s_two);
        two = GUARD->temporary_buf_alloc(PAYLOAD, s_two);
        TEST_ASSERT_NOT_NULL(one);
        TEST_ASSERT_NOT_NULL(two);
        TEST_ASSERT_TRUE(depth_of(region) <= MMGR_CARCER_REGION_BYTES);
        deepest = (depth_of(region) > deepest) ? depth_of(region) : deepest;
    }
    // the region sank past the two live cells, as Stacks.lean says a shared stack does, and no deeper than its size
    TEST_ASSERT_TRUE(deepest > (2u * CELL));
    TEST_ASSERT_TRUE(deepest <= MMGR_CARCER_REGION_BYTES);
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_one));
    TEST_ASSERT_EQUAL_size_t(PAYLOAD, mmgr_owner_in_use(s_two));
    TEST_ASSERT_EQUAL_PTR(s_one, mmgr_owner_of_buf(BLOCK, one));
    TEST_ASSERT_EQUAL_PTR(s_two, mmgr_owner_of_buf(BLOCK, two));
}

// What each thread of the two-thread case records: its region and the deepest it read that region
static embed_atomic_u32 s_thread_next = EMBED_ATOMIC_INIT(0u);
static size_t s_thread_region[2];
static size_t s_thread_deepest[2];

// Each thread's work: rounds of a take and its give in the thread's own region
static void thread_took_and_gave(void)
{
    const unsigned int index = embed_atomic_u32_fetch_add(&s_thread_next, 1u);
    size_t round;

    s_thread_region[index] = own_region();
    s_thread_deepest[index] = 0u;
    for (round = 0u; round < ROUNDS; round++)
    {
        void *const prisoner = GUARD->temporary_buf_alloc(PAYLOAD, s_one);
        const size_t depth = depth_of(s_thread_region[index]);

        s_thread_deepest[index] = (depth > s_thread_deepest[index]) ? depth : s_thread_deepest[index];
        GUARD->persistent_buf_release(prisoner, s_one);
    }
}

void test_two_threads_that_take_and_give_in_regions_of_their_own_never_deepen_one_past_their_cell(void)
{
    embed_atomic_u32_store(&s_thread_next, 0u);
    TEST_ASSERT_TRUE(mmgr_host_threads_run(thread_took_and_gave, 2u));
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(0u));
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(1u));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(s_one));
    if (s_thread_region[0] != s_thread_region[1])
    {
        TEST_ASSERT_EQUAL_size_t(CELL, s_thread_deepest[0]);
        TEST_ASSERT_EQUAL_size_t(CELL, s_thread_deepest[1]);
    }
    else
    {
        TEST_ASSERT_TRUE(s_thread_deepest[0] <= MMGR_CARCER_REGION_BYTES);
        TEST_ASSERT_TRUE(s_thread_deepest[1] <= MMGR_CARCER_REGION_BYTES);
    }
}

// The cell the main thread took, which another thread gives
static void *s_handed;

static void thread_gave(void)
{
    GUARD->persistent_buf_release(s_handed, s_one);
}

void test_a_give_from_another_thread_trims_the_region_that_holds_the_cell(void)
{
    const size_t region = own_region();

    s_handed = GUARD->temporary_buf_alloc(PAYLOAD, s_one);
    TEST_ASSERT_EQUAL_size_t(CELL, depth_of(region));
    TEST_ASSERT_TRUE(mmgr_host_threads_run(thread_gave, 1u));
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(region));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(s_one));
}

void test_a_growth_of_a_regions_newest_cell_moves_it_down_and_deepens_the_region(void)
{
    void *const prisoner = GUARD->temporary_buf_alloc(PAYLOAD, s_one);
    const size_t region = own_region();
    void *const grown = GUARD->buf_grow(prisoner, 2u * PAYLOAD, s_one);

    TEST_ASSERT_NOT_NULL(grown);
    TEST_ASSERT_EQUAL_PTR((uint8_t *)prisoner - PAYLOAD, grown);
    TEST_ASSERT_EQUAL_size_t(CARCER_HDR + (2u * PAYLOAD), depth_of(region));
    TEST_ASSERT_EQUAL_size_t(2u * PAYLOAD, mmgr_owner_in_use(s_one));
    GUARD->persistent_buf_release(grown, s_one);
    TEST_ASSERT_EQUAL_size_t(0u, depth_of(region));
}

void test_a_growth_a_region_cannot_hold_changes_nothing(void)
{
    void *rest;
    void *const first = region_filled(&rest);

    TEST_ASSERT_NULL(GUARD->buf_grow(rest, MMGR_CARCER_REGION_BYTES, s_one));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_REGION_BYTES, depth_of(own_region()));
    TEST_ASSERT_EQUAL_PTR(s_one, mmgr_owner_of_buf(BLOCK, first));
    TEST_ASSERT_EQUAL_PTR(s_one, mmgr_owner_of_buf(BLOCK, rest));
}

void test_the_gap_lies_below_the_regions_and_a_persistent_take_lands_in_it(void)
{
    void *const prisoner = GUARD->persistent_buf_alloc(PAYLOAD, s_one);

    TEST_ASSERT_EQUAL_size_t(REGIONED_BYTES - MMGR_CARCER_REGION_AREA - CELL, GUARD->buf_available());
    TEST_ASSERT_NOT_NULL(prisoner);
    TEST_ASSERT_TRUE(carcer_off_of(BLOCK, prisoner) < (REGIONED_BYTES - MMGR_CARCER_REGION_AREA));
    TEST_ASSERT_EQUAL_size_t(REGIONED_BYTES - MMGR_CARCER_REGION_AREA, mmgr_series_cellularum_temporary_top(BLOCK));
}
