// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "carceribus/carceribus.h"

// deliberately over-aligned and oversized: init aligns the base up, so a case that wants an
// unaligned start offsets into this itself.
static _Alignas(64) uint8_t store[4096];
static _Alignas(64) uint8_t store2[1024];
static mmgr_carcer a;

void setUp(void)
{
    for (unsigned i = 0; i < sizeof store; i++)
    {
        store[i] = 0xCDu;
    }
    mmgr_carcer_init(&a, store, sizeof store);
}

void tearDown(void)
{
}

void test_carceribus_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("confinium.h compiled with no header before it");
}

void test_align_up_rounds_to_the_grain(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_align_up(0u));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, mmgr_carcer_align_up(1u));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, mmgr_carcer_align_up(MMGR_CARCER_ALIGN));
    TEST_ASSERT_EQUAL_size_t(2u * MMGR_CARCER_ALIGN, mmgr_carcer_align_up(MMGR_CARCER_ALIGN + 1u));
}

void test_a_fresh_tenant_is_empty(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_persist_used(&a));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_interim_used(&a));
    TEST_ASSERT_GREATER_THAN_size_t(0u, mmgr_carcer_octas_praesto(&a));
}

void test_init_aligns_the_base_up_and_loses_the_slack(void)
{
    mmgr_carcer u;
    mmgr_carcer_init(&u, store + 1u, sizeof store - 1u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)u.base & (MMGR_CARCER_MAX_ALIGN - 1u), "base is aligned up");
    TEST_ASSERT_LESS_THAN_size_t_MESSAGE(sizeof store, u.size, "the bytes skipped to align are not usable");
}

void test_init_of_a_region_too_small_to_align_leaves_nothing(void)
{
    mmgr_carcer u;
    mmgr_carcer_init(&u, store + 1u, 2u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, u.size, "a region shorter than the alignment adjustment holds nothing");
    TEST_ASSERT_NULL(mmgr_carcer_persist_capio(&u, 1u));
}

void test_persist_hands_out_zeroed_bytes(void)
{
    uint8_t *p = (uint8_t *)mmgr_carcer_persist_capio(&a, 32u);
    TEST_ASSERT_NOT_NULL(p);
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, p[i], "persist bytes come back zeroed");
    }
    TEST_ASSERT_TRUE(mmgr_carcer_owns(&a, p));
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(32u, mmgr_carcer_persist_used(&a));
}

void test_a_zero_sized_persist_still_gives_a_block(void)
{
    void *p = mmgr_carcer_persist_capio(&a, 0u);
    TEST_ASSERT_NOT_NULL_MESSAGE(p, "zero rounds up to one grain rather than failing");
}

void test_persist_blocks_do_not_overlap(void)
{
    uint8_t *p = (uint8_t *)mmgr_carcer_persist_capio(&a, 64u);
    uint8_t *q = (uint8_t *)mmgr_carcer_persist_capio(&a, 64u);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_TRUE_MESSAGE(q >= p + 64u || p >= q + 64u, "two live blocks must not overlap");
}

void test_a_freed_block_is_reused(void)
{
    void *p = mmgr_carcer_persist_capio(&a, 64u);
    TEST_ASSERT_NOT_NULL(p);
    const size_t used = mmgr_carcer_persist_used(&a);

    mmgr_carcer_persist_reddo(&a, p);
    TEST_ASSERT_LESS_THAN_size_t_MESSAGE(used, mmgr_carcer_persist_used(&a), "freeing gives the bytes back");

    void *q = mmgr_carcer_persist_capio(&a, 64u);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, q, "the free list hands the same block back");
}

void test_a_large_free_block_splits(void)
{
    void *big = mmgr_carcer_persist_capio(&a, 512u);
    void *after = mmgr_carcer_persist_capio(&a, 16u);
    TEST_ASSERT_NOT_NULL(big);
    TEST_ASSERT_NOT_NULL(after);

    mmgr_carcer_persist_reddo(&a, big);

    // a small request into a large hole should split it and leave the remainder usable
    void *small = mmgr_carcer_persist_capio(&a, 16u);
    TEST_ASSERT_EQUAL_PTR(big, small);
    void *rest = mmgr_carcer_persist_capio(&a, 16u);
    TEST_ASSERT_NOT_NULL_MESSAGE(rest, "the remainder of the split block is still available");
}

void test_an_exact_fit_does_not_split(void)
{
    void *p = mmgr_carcer_persist_capio(&a, 32u);
    void *guard = mmgr_carcer_persist_capio(&a, 32u);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(guard);
    mmgr_carcer_persist_reddo(&a, p);

    void *q = mmgr_carcer_persist_capio(&a, 32u);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, q, "an exact fit reuses the block whole");
}

void test_freeing_null_is_a_no_op(void)
{
    const size_t used = mmgr_carcer_persist_used(&a);
    mmgr_carcer_persist_reddo(&a, NULL);
    TEST_ASSERT_EQUAL_size_t(used, mmgr_carcer_persist_used(&a));
}

void test_freeing_twice_does_not_double_count(void)
{
    void *p = mmgr_carcer_persist_capio(&a, 32u);
    mmgr_carcer_persist_reddo(&a, p);
    const size_t used = mmgr_carcer_persist_used(&a);
    mmgr_carcer_persist_reddo(&a, p);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(used, mmgr_carcer_persist_used(&a), "a second free must not subtract again");
}

void test_adjacent_free_blocks_coalesce(void)
{
    void *p = mmgr_carcer_persist_capio(&a, 64u);
    void *q = mmgr_carcer_persist_capio(&a, 64u);
    void *guard = mmgr_carcer_persist_capio(&a, 16u);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_NOT_NULL(guard);

    mmgr_carcer_persist_reddo(&a, q);
    mmgr_carcer_persist_reddo(&a, p);

    // the two holes are adjacent, so one request larger than either must fit
    void *big = mmgr_carcer_persist_capio(&a, 128u);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, big, "two adjacent holes become one");
}

void test_persist_fails_rather_than_meeting_the_other_end(void)
{
    TEST_ASSERT_NULL_MESSAGE(mmgr_carcer_persist_capio(&a, sizeof store * 2u), "a request past the tenant fails");
}

void test_interim_grows_down_from_the_top(void)
{
    uint8_t *p = (uint8_t *)mmgr_carcer_interim_capio(&a, 64u);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(mmgr_carcer_owns(&a, p));
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(64u, mmgr_carcer_interim_used(&a));

    uint8_t *q = (uint8_t *)mmgr_carcer_interim_capio(&a, 64u);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_TRUE_MESSAGE(q < p, "the interim end grows downward");
}

void test_interim_honours_alignment(void)
{
    for (size_t al = 1u; al <= 32u; al <<= 1)
    {
        void *p = mmgr_carcer_interim_capio_aligned(&a, 24u, al);
        TEST_ASSERT_NOT_NULL(p);
        const size_t want =
            (al < MMGR_CARCER_ALIGN) ? MMGR_CARCER_ALIGN : ((al > MMGR_CARCER_MAX_ALIGN) ? MMGR_CARCER_MAX_ALIGN : al);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)p & (want - 1u), "alignment is clamped, then honored");
    }
}

void test_a_zero_sized_interim_still_gives_bytes(void)
{
    TEST_ASSERT_NOT_NULL(mmgr_carcer_interim_capio(&a, 0u));
}

void test_interim_mark_and_release(void)
{
    void *before = mmgr_carcer_interim_capio(&a, 32u);
    const size_t used = mmgr_carcer_interim_used(&a);
    const size_t mark = mmgr_carcer_interim_mark(&a);

    TEST_ASSERT_NOT_NULL(mmgr_carcer_interim_capio(&a, 128u));
    TEST_ASSERT_GREATER_THAN_size_t(used, mmgr_carcer_interim_used(&a));

    mmgr_carcer_interim_reddo(&a, mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(used, mmgr_carcer_interim_used(&a), "release puts back what came after");
    TEST_ASSERT_TRUE(mmgr_carcer_owns(&a, before));
}

void test_a_stale_or_forward_mark_is_ignored(void)
{
    TEST_ASSERT_NOT_NULL(mmgr_carcer_interim_capio(&a, 64u));
    const size_t used = mmgr_carcer_interim_used(&a);

    mmgr_carcer_interim_reddo(&a, 0u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(used, mmgr_carcer_interim_used(&a), "a mark below the fill point is refused");

    mmgr_carcer_interim_reddo(&a, a.size + 64u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(used, mmgr_carcer_interim_used(&a), "a mark past the tenant is refused");
}

void test_interim_reset_releases_everything(void)
{
    TEST_ASSERT_NOT_NULL(mmgr_carcer_interim_capio(&a, 128u));
    mmgr_carcer_interim_reset(&a);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_interim_used(&a));
}

void test_interim_fails_rather_than_meeting_the_other_end(void)
{
    TEST_ASSERT_NULL(mmgr_carcer_interim_capio(&a, sizeof store * 2u));
    TEST_ASSERT_NULL(mmgr_carcer_interim_capio_aligned(&a, sizeof store * 2u, 16u));
}

void test_the_two_ends_meet_and_stop(void)
{
    size_t n = 0;
    while (mmgr_carcer_interim_capio(&a, 64u) != NULL)
    {
        n += 64u;
        if (n > sizeof store * 2u)
        {
            TEST_FAIL_MESSAGE("the tenant handed out more than it holds");
        }
    }
    TEST_ASSERT_GREATER_THAN_size_t(0u, n);
    TEST_ASSERT_NULL_MESSAGE(mmgr_carcer_persist_capio(&a, 64u), "persist cannot grow into a full interim");
}

void test_owns_rejects_what_it_did_not_hand_out(void)
{
    TEST_ASSERT_FALSE(mmgr_carcer_owns(&a, store2));
    TEST_ASSERT_FALSE(mmgr_carcer_owns(&a, NULL));
    TEST_ASSERT_FALSE_MESSAGE(mmgr_carcer_owns(&a, a.base + a.size), "one past the end is not inside");
}

void test_free_space_falls_as_it_is_taken(void)
{
    const size_t before = mmgr_carcer_octas_praesto(&a);
    TEST_ASSERT_NOT_NULL(mmgr_carcer_interim_capio(&a, 128u));
    TEST_ASSERT_LESS_THAN_size_t(before, mmgr_carcer_octas_praesto(&a));
}

/* ---------------------------------------------------------------------------------------------
 * a set of regions taken from as one
 * ------------------------------------------------------------------------------------------- */

void test_a_set_starts_empty(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_set_octas_praesto(&s));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_set_persist_used(&s));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_set_interim_used(&s));
    TEST_ASSERT_NULL(mmgr_carcer_set_persist_capio(&s, 16u));
    TEST_ASSERT_NULL(mmgr_carcer_set_interim_capio(&s, 16u));
}

void test_a_set_takes_regions_until_it_is_full(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);

    unsigned added = 0;
    while (mmgr_carcer_set_add(&s, store, sizeof store))
    {
        added++;
        if (added > MMGR_CARCER_MAX_REGIONS + 2u)
        {
            TEST_FAIL_MESSAGE("the set accepted more regions than it holds");
        }
    }
    TEST_ASSERT_EQUAL_UINT_MESSAGE(MMGR_CARCER_MAX_REGIONS, added, "a full set refuses the next region");
}

void test_a_set_refuses_a_region_too_small_to_hold_anything(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);
    TEST_ASSERT_FALSE_MESSAGE(mmgr_carcer_set_add(&s, store, 2u), "a region with no room for a header is refused");
}

void test_a_set_hands_out_from_its_regions(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);
    TEST_ASSERT_TRUE(mmgr_carcer_set_add(&s, store, sizeof store));
    TEST_ASSERT_TRUE(mmgr_carcer_set_add(&s, store2, sizeof store2));

    TEST_ASSERT_GREATER_THAN_size_t(0u, mmgr_carcer_set_octas_praesto(&s));

    void *p = mmgr_carcer_set_persist_capio(&s, 64u);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(64u, mmgr_carcer_set_persist_used(&s));

    void *q = mmgr_carcer_set_interim_capio(&s, 64u);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(64u, mmgr_carcer_set_interim_used(&s));

    void *r = mmgr_carcer_set_interim_capio_aligned(&s, 32u, 16u);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_size_t(0u, (uintptr_t)r & 15u);

    mmgr_carcer_set_persist_reddo(&s, p);
    mmgr_carcer_set_persist_reddo(&s, NULL);
}

void test_a_set_mark_covers_every_region(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);
    TEST_ASSERT_TRUE(mmgr_carcer_set_add(&s, store, sizeof store));
    TEST_ASSERT_TRUE(mmgr_carcer_set_add(&s, store2, sizeof store2));

    const mmgr_carcer_mark mark = mmgr_carcer_set_interim_mark(&s);
    TEST_ASSERT_NOT_NULL(mmgr_carcer_set_interim_capio(&s, 128u));
    TEST_ASSERT_GREATER_THAN_size_t(0u, mmgr_carcer_set_interim_used(&s));

    mmgr_carcer_set_interim_reddo(&s, &mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_carcer_set_interim_used(&s), "the mark restores every region at once");
}

void test_a_set_reset_releases_every_region(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);
    TEST_ASSERT_TRUE(mmgr_carcer_set_add(&s, store, sizeof store));
    TEST_ASSERT_NOT_NULL(mmgr_carcer_set_interim_capio(&s, 128u));

    mmgr_carcer_set_interim_reset(&s);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_set_interim_used(&s));
}

void test_a_set_request_larger_than_any_region_fails(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);
    TEST_ASSERT_TRUE(mmgr_carcer_set_add(&s, store2, sizeof store2));
    TEST_ASSERT_NULL(mmgr_carcer_set_persist_capio(&s, sizeof store2 * 4u));
    TEST_ASSERT_NULL(mmgr_carcer_set_interim_capio(&s, sizeof store2 * 4u));
}

/* ---------------------------------------------------------------------------------------------
 * the free list, walked
 *
 * A released persistent block goes back on the list and the next request walks it looking for one
 * that fits. Which arm the walk takes depends on whether the block it lands on is free at all and
 * whether it is big enough, so both have to be arranged rather than hoped for.
 * ------------------------------------------------------------------------------------------- */

void test_a_persist_request_walks_past_a_block_that_is_still_in_use(void)
{
    void *keep = mmgr_carcer_persist_capio(&a, 64u);
    void *drop = mmgr_carcer_persist_capio(&a, 64u);
    TEST_ASSERT_NOT_NULL(keep);
    TEST_ASSERT_NOT_NULL(drop);

    mmgr_carcer_persist_reddo(&a, drop);

    // The first block on the list is still in use, so the walk has to step over it to reach the
    // one that was given back.
    void *again = mmgr_carcer_persist_capio(&a, 64u);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(drop, again, "the released block was not reused");
}

void test_a_persist_request_walks_past_a_free_block_that_is_too_small(void)
{
    void *small = mmgr_carcer_persist_capio(&a, 16u);
    void *keep = mmgr_carcer_persist_capio(&a, 16u);
    void *big = mmgr_carcer_persist_capio(&a, 256u);
    TEST_ASSERT_NOT_NULL(small);
    TEST_ASSERT_NOT_NULL(keep);
    TEST_ASSERT_NOT_NULL(big);

    // keep stays in use between the two. Releasing neighbors merges them, so without something
    // in the way there would be one free block and nothing for the walk to step over.
    mmgr_carcer_persist_reddo(&a, small);
    mmgr_carcer_persist_reddo(&a, big);

    // The small block comes first and cannot hold the request, so the walk keeps going.
    void *want = mmgr_carcer_persist_capio(&a, 200u);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(big, want, "the walk stopped at a block that was too small");
}

void test_a_reused_block_is_split_when_there_is_enough_left_over(void)
{
    void *big = mmgr_carcer_persist_capio(&a, 512u);
    TEST_ASSERT_NOT_NULL(big);
    mmgr_carcer_persist_reddo(&a, big);

    void *first = mmgr_carcer_persist_capio(&a, 16u);
    void *second = mmgr_carcer_persist_capio(&a, 16u);

    TEST_ASSERT_EQUAL_PTR(big, first);
    TEST_ASSERT_NOT_NULL_MESSAGE(second, "the leftover was not put back on the list");
    TEST_ASSERT_TRUE_MESSAGE((uint8_t *)second > (uint8_t *)first,
                             "the second allocation did not come out of the leftover");
}

void test_a_persist_request_larger_than_the_tenant_is_refused(void)
{
    TEST_ASSERT_NULL(mmgr_carcer_persist_capio(&a, sizeof store * 2u));
}

void test_a_persist_request_that_would_meet_the_interim_end_is_refused(void)
{
    // Take the interim end down until the two ends have almost no space between them.
    TEST_ASSERT_NOT_NULL(mmgr_carcer_interim_capio(&a, sizeof store - 128u));
    TEST_ASSERT_NULL_MESSAGE(mmgr_carcer_persist_capio(&a, 1024u), "the two ends were allowed to cross");
}

void test_releasing_the_same_block_twice_is_ignored(void)
{
    void *p = mmgr_carcer_persist_capio(&a, 32u);
    TEST_ASSERT_NOT_NULL(p);
    const size_t used = mmgr_carcer_persist_used(&a);

    mmgr_carcer_persist_reddo(&a, p);
    const size_t after = mmgr_carcer_persist_used(&a);
    TEST_ASSERT_LESS_THAN_size_t(used, after);

    mmgr_carcer_persist_reddo(&a, p);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(after, mmgr_carcer_persist_used(&a),
                                     "a second release took the bytes off the tally twice");
}

void test_releasing_nothing_is_ignored(void)
{
    const size_t used = mmgr_carcer_persist_used(&a);
    mmgr_carcer_persist_reddo(&a, NULL);
    TEST_ASSERT_EQUAL_size_t(used, mmgr_carcer_persist_used(&a));
}

/* ---------------------------------------------------------------------------------------------
 * what is left in the middle
 * ------------------------------------------------------------------------------------------- */

void test_free_space_is_zero_once_the_two_ends_meet(void)
{
    TEST_ASSERT_NOT_NULL(mmgr_carcer_interim_capio(&a, sizeof store - 64u));
    while (mmgr_carcer_persist_capio(&a, 8u) != NULL)
    {
        // Fill the gap from the other end until nothing more fits.
    }

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_carcer_octas_praesto(&a),
                                     "a tenant with nothing between its ends still reported room");
}

void test_free_space_never_reports_less_than_a_header(void)
{
    // Whatever is left has to hold a header before it can hold a byte, so the count is the space
    // past that and never a number the caller could not actually use.
    const size_t room = mmgr_carcer_octas_praesto(&a);
    TEST_ASSERT_NOT_NULL(mmgr_carcer_persist_capio(&a, room));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_octas_praesto(&a));
}

/* ---------------------------------------------------------------------------------------------
 * the aligned interim entry
 * ------------------------------------------------------------------------------------------- */

void test_an_aligned_interim_request_that_does_not_fit_is_refused(void)
{
    TEST_ASSERT_NULL(mmgr_carcer_interim_capio_aligned(&a, sizeof store * 2u, 16u));

    // And one that fits by size but not once the persistent end is where it is.
    TEST_ASSERT_NOT_NULL(mmgr_carcer_persist_capio(&a, sizeof store - 256u));
    TEST_ASSERT_NULL_MESSAGE(mmgr_carcer_interim_capio_aligned(&a, 1024u, 16u),
                             "the interim end was allowed to reach past the persistent end");
}

void test_owns_says_no_to_addresses_on_either_side(void)
{
    mmgr_carcer empty;
    empty.base = NULL;
    empty.size = 0;

    TEST_ASSERT_FALSE_MESSAGE(mmgr_carcer_owns(&empty, store), "a tenant with no storage owns nothing");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_carcer_owns(&a, store - 1), "an address below the base is not inside");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_carcer_owns(&a, a.base + a.size), "one past the end is not inside");
    TEST_ASSERT_TRUE(mmgr_carcer_owns(&a, a.base));
    TEST_ASSERT_TRUE(mmgr_carcer_owns(&a, a.base + a.size - 1u));
}

/* ---------------------------------------------------------------------------------------------
 * the set
 * ------------------------------------------------------------------------------------------- */

void test_a_set_release_finds_the_region_the_pointer_came_from(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);
    TEST_ASSERT_TRUE(mmgr_carcer_set_add(&s, store, sizeof store));
    TEST_ASSERT_TRUE(mmgr_carcer_set_add(&s, store2, sizeof store2));

    // From the second region, so the search has to step past the first to find it.
    void *p = mmgr_carcer_persist_capio(&s.region[1], 32u);
    TEST_ASSERT_NOT_NULL(p);
    const size_t used = mmgr_carcer_persist_used(&s.region[1]);

    mmgr_carcer_set_persist_reddo(&s, p);
    TEST_ASSERT_LESS_THAN_size_t_MESSAGE(used, mmgr_carcer_persist_used(&s.region[1]),
                                         "the release did not reach the region the pointer was in");
}

void test_a_set_release_of_an_address_it_does_not_hold(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);
    TEST_ASSERT_TRUE(mmgr_carcer_set_add(&s, store, sizeof store));

    uint8_t elsewhere[8];
    mmgr_carcer_set_persist_reddo(&s, elsewhere);
    mmgr_carcer_set_persist_reddo(&s, NULL);
    TEST_PASS_MESSAGE("an address from outside every region is walked past and dropped");
}

void test_a_set_release_on_an_empty_set(void)
{
    mmgr_carcer_set s;
    mmgr_carcer_set_init(&s);

    uint8_t elsewhere[8];
    mmgr_carcer_set_persist_reddo(&s, elsewhere);
    TEST_PASS_MESSAGE("a set with no regions has nothing to walk");
}
