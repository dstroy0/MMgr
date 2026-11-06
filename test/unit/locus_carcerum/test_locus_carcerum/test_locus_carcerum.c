#include "locus_carcerum/locus_carcerum.h"
#include "spatium/spatium.h"

#include "unity.h"

#define A_BYTES 1024u
#define B_BYTES 1024u

ParsMemoriaeInternae(a, A_BYTES);
ParsMemoriaeInternae(b, B_BYTES);

LocusCarcerum(ram, MMGR_MINIMUM_SECURITY(a), MMGR_MAXIMUM_SECURITY(b));

#define QUAD_BYTES 256u

ParsMemoriaeInternae(q0, QUAD_BYTES);
ParsMemoriaeInternae(q1, QUAD_BYTES);
ParsMemoriaeInternae(q2, QUAD_BYTES);
ParsMemoriaeInternae(q3, QUAD_BYTES);

LocusCarcerum(quad, MMGR_MINIMUM_SECURITY(q0), MMGR_MINIMUM_SECURITY(q1), MMGR_MINIMUM_SECURITY(q2),
              MMGR_MINIMUM_SECURITY(q3));

#define OCTO_BYTES 128u

ParsMemoriaeInternae(o0, OCTO_BYTES);
ParsMemoriaeInternae(o1, OCTO_BYTES);
ParsMemoriaeInternae(o2, OCTO_BYTES);
ParsMemoriaeInternae(o3, OCTO_BYTES);
ParsMemoriaeInternae(o4, OCTO_BYTES);
ParsMemoriaeInternae(o5, OCTO_BYTES);
ParsMemoriaeInternae(o6, OCTO_BYTES);
ParsMemoriaeInternae(o7, OCTO_BYTES);

LocusCarcerum(octo, MMGR_MINIMUM_SECURITY(o0), MMGR_MINIMUM_SECURITY(o1), MMGR_MINIMUM_SECURITY(o2),
              MMGR_MINIMUM_SECURITY(o3), MMGR_MINIMUM_SECURITY(o4), MMGR_MINIMUM_SECURITY(o5),
              MMGR_MINIMUM_SECURITY(o6), MMGR_MINIMUM_SECURITY(o7));

// The owners the cases take for, handed out by the queue to the first setUp: the owner queue lives as long
// as the program does
// The owners every case takes for, declared with the cellblocks as the program's table
#define RAM_OWNERS(owner_) owner_(cells, "cells") owner_(grower, "grower")
MMGR_OWNERS(RAM_OWNERS)

void setUp(void)
{
    mmgr_owner_bytes_given(grower, mmgr_owner_in_use(grower));
    mmgr_series_cellularum_emptied(&ram_a_ctx);
    mmgr_series_cellularum_emptied(&ram_b_ctx);
    ram.a.temporary_buf_reset();
    ram.b.temporary_buf_reset();
}

void tearDown(void)
{
}

void test_locus_carcerum_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("locus_carcerum.h compiled with no header before it");
}

void test_the_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(MinimumSecurityGuard), sizeof ram.a,
                                     "a minimum security cellblock is not its own type");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(MaximumSecurityGuard), sizeof ram.b,
                                     "a maximum security cellblock is not its own type");
}

void test_the_machinery_sits_below_the_arena(void)
{
    TEST_ASSERT_EQUAL_PTR_MESSAGE(mmgr_pars_storage_a, ram_a_ctx.base, "a cellblock starts at the pool it was given");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(mmgr_pars_storage_b, ram_b_ctx.base, "and so does its neighbor");
}

void test_a_site_does_not_pay_for_the_ceiling(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(A_BYTES, ram_a_ctx.size, "a cellblock is the size its declaration gave it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(OCTO_BYTES, octo_o0_ctx.size,
                                     "and another site's cellblocks cost this one nothing");
}

void test_a_four_series_cellularum_site_builds_all_four(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(QUAD_BYTES, quad_q0_ctx.size, "the count is not capped at two");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(QUAD_BYTES, quad_q3_ctx.size, "and the fourth is a whole cellblock");
}

void test_a_four_series_cellularum_site_lays_them_end_to_end(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_q0, quad_q0_ctx.base);
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_q1, quad_q1_ctx.base);
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_q2, quad_q2_ctx.base);
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_q3, quad_q3_ctx.base);
}

void test_every_series_cellularum_of_a_four_series_cellularum_site_allocates_from_its_own_bytes(void)
{
    void *const g0 = quad.q0.persistent_buf_alloc(32u, cells);
    void *const g1 = quad.q1.persistent_buf_alloc(32u, cells);
    void *const g2 = quad.q2.persistent_buf_alloc(32u, cells);
    void *const g3 = quad.q3.persistent_buf_alloc(32u, cells);

    TEST_ASSERT_NOT_NULL_MESSAGE(g0, "an allocation from a declared cellblock must succeed");
    TEST_ASSERT_TRUE_MESSAGE(quad.q0.who_owns_buf(g0), "and must land inside that cellblock");
    TEST_ASSERT_TRUE_MESSAGE(quad.q1.who_owns_buf(g1), "and must land inside that cellblock");
    TEST_ASSERT_TRUE_MESSAGE(quad.q2.who_owns_buf(g2), "and must land inside that cellblock");
    TEST_ASSERT_TRUE_MESSAGE(quad.q3.who_owns_buf(g3), "and must land inside that cellblock");
    TEST_ASSERT_FALSE_MESSAGE(quad.q0.who_owns_buf(g3), "a neighbor's bytes are not ours");
}

void test_an_eight_series_cellularum_site_builds_and_allocates(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_o0, octo_o0_ctx.base);
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_o7, octo_o7_ctx.base);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(OCTO_BYTES, octo_o7_ctx.size, "eight cellblocks is a legal count");

    void *const first = octo.o0.persistent_buf_alloc(16u, cells);
    void *const last = octo.o7.persistent_buf_alloc(16u, cells);

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_TRUE_MESSAGE(octo.o0.who_owns_buf(first), "the first cellblock hands out storage inside itself");
    TEST_ASSERT_TRUE_MESSAGE(octo.o7.who_owns_buf(last), "and so does the eighth");
    TEST_ASSERT_FALSE_MESSAGE(octo.o0.who_owns_buf(last), "which is not the first cellblock's");
}

void test_two_sites_hold_separate_pools(void)
{
    TEST_ASSERT_TRUE_MESSAGE((uintptr_t)mmgr_pars_storage_a != (uintptr_t)mmgr_pars_storage_q0,
                             "two sites' pools are separate objects");
}

void test_init_records_the_site_it_was_given(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_a, ram_a_ctx.base);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram_a_ctx.size);
    TEST_ASSERT_EQUAL_size_t(B_BYTES, ram_b_ctx.size);
}

void test_the_build_lays_the_series_cellularum_end_to_end(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_pars_storage_a, ram_a_ctx.base);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram_a_ctx.size);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(mmgr_pars_storage_b, ram_b_ctx.base, "and the second on its own");
    TEST_ASSERT_EQUAL_size_t(B_BYTES, ram_b_ctx.size);
}

void test_a_fresh_series_cellularum_is_empty_and_whole(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_series_cellularum_persistent_end(&ram_a_ctx));
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram.a.buf_available());
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram.a.temporary_buf_mark());
}

void test_the_owner_readers_answer_what_an_owner_counts(void)
{
    static CellOwner s_unlisted = {"unlisted", 0u, EMBED_ATOMIC_INIT(0u), EMBED_ATOMIC_INIT(0u)};
    const size_t before = mmgr_owner_in_use(cells);

    TEST_ASSERT_FALSE_MESSAGE(mmgr_owner_listed(NULL), "no owner stands in no queue");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_owner_listed(&s_unlisted), "a record of the caller's own is none the queue holds");
    TEST_ASSERT_TRUE(mmgr_owner_listed(cells));
    void *const cell = ram.a.persistent_buf_alloc(128u, cells);

    TEST_ASSERT_EQUAL_size_t(before + 128u, mmgr_owner_in_use(cells));
    TEST_ASSERT_TRUE(mmgr_owner_hw(cells) >= (before + 128u));
    ram.a.persistent_buf_release(cell, cells);
    TEST_ASSERT_EQUAL_size_t(before, mmgr_owner_in_use(cells));
    TEST_ASSERT_TRUE_MESSAGE(mmgr_owner_hw(cells) >= (before + 128u), "the peak stands");
}

void test_the_series_cellularum_readers_answer_what_its_tiers_hold(void)
{
    mmgr_series_cellularum_emptied(&ram_a_ctx);
    TEST_ASSERT_TRUE(mmgr_series_cellularum_idle(&ram_a_ctx));
    void *const low = ram.a.persistent_buf_alloc(64u, cells);

    TEST_ASSERT_FALSE_MESSAGE(mmgr_series_cellularum_idle(&ram_a_ctx), "a persistent cell is taken");
    ram.a.persistent_buf_release(low, cells);
    TEST_ASSERT_TRUE(mmgr_series_cellularum_idle(&ram_a_ctx));
    void *const high = ram.a.temporary_buf_alloc(64u, cells);

    TEST_ASSERT_FALSE_MESSAGE(mmgr_series_cellularum_idle(&ram_a_ctx), "a temporary cell is taken");
    ram.a.persistent_buf_release(high, cells);
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    TEST_ASSERT_TRUE(mmgr_series_cellularum_persistent_hw(&ram_a_ctx) >= 64u);
    TEST_ASSERT_TRUE(mmgr_series_cellularum_temporary_hw(&ram_a_ctx) >= 64u);
#else
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_series_cellularum_persistent_hw(&ram_a_ctx));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_series_cellularum_temporary_hw(&ram_a_ctx));
#endif
}

void test_align_up_rounds_to_a_whole_word(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_align_up_buf(0u));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, mmgr_align_up_buf(1u));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, mmgr_align_up_buf(MMGR_CARCER_ALIGN));
    TEST_ASSERT_EQUAL_size_t(2u * MMGR_CARCER_ALIGN, mmgr_align_up_buf(MMGR_CARCER_ALIGN + 1u));
}

void test_both_tiers_hand_out_aligned_addresses(void)
{
    for (size_t n = 1u; n <= 40u; n++)
    {

        setUp();

        void *const p = ram.a.persistent_buf_alloc(n, cells);
        void *const q = ram.a.temporary_buf_alloc(n, cells);

        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_NOT_NULL(q);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)p % MMGR_CARCER_ALIGN, "a persistent cell is misaligned");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)q % MMGR_CARCER_ALIGN, "a temporary cell is misaligned");
    }
}

void test_the_persistent_tier_hands_out_the_bottom_and_walks_up(void)
{
    uint8_t *p = (uint8_t *)ram.a.persistent_buf_alloc(64u, cells);
    uint8_t *q = (uint8_t *)ram.a.persistent_buf_alloc(64u, cells);

    TEST_ASSERT_TRUE_MESSAGE(p < q, "the next cell sits above the first");
    TEST_ASSERT_TRUE(ram.a.who_owns_buf(p));
}

void test_the_temporary_tier_hands_out_the_top_and_walks_down(void)
{
    uint8_t *p = (uint8_t *)ram.a.temporary_buf_alloc(64u, cells);
    uint8_t *q = (uint8_t *)ram.a.temporary_buf_alloc(64u, cells);

    TEST_ASSERT_TRUE_MESSAGE(q < p, "the next cell sits below the first");
    TEST_ASSERT_TRUE(ram.a.who_owns_buf(p));
}

void test_the_two_tiers_allocate_from_the_same_gap(void)
{
    const size_t room = ram.a.buf_available();

    (void)ram.a.persistent_buf_alloc(64u, cells);
    (void)ram.a.temporary_buf_alloc(64u, cells);

    TEST_ASSERT_TRUE_MESSAGE(ram.a.buf_available() < room, "both tiers allocate from the same gap");
}

void test_both_tiers_fail_closed(void)
{
    TEST_ASSERT_NULL(ram.a.persistent_buf_alloc(A_BYTES * 4u, cells));
    TEST_ASSERT_NULL(ram.a.temporary_buf_alloc(A_BYTES * 4u, cells));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(A_BYTES, ram.a.buf_available(),
                                     "a refused request must not have moved a boundary");
}

void test_a_cell_given_back_below_the_end_waits_for_the_cells_above_it(void)
{
    void *const p = ram.a.persistent_buf_alloc(64u, cells);
    void *const q = ram.a.persistent_buf_alloc(64u, cells);
    const size_t end_with_both = mmgr_series_cellularum_persistent_end(&ram_a_ctx);

    ram.a.persistent_buf_release(p, cells);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(end_with_both, mmgr_series_cellularum_persistent_end(&ram_a_ctx),
                                     "a cell below a held one stays in the tier");

    void *const r = ram.a.persistent_buf_alloc(64u, cells);

    TEST_ASSERT_TRUE_MESSAGE(r > q, "a take with room in the gap bumps the end past the held cell");
    TEST_ASSERT_TRUE_MESSAGE(mmgr_series_cellularum_persistent_end(&ram_a_ctx) > end_with_both, "and the end moves");
    ram.a.persistent_buf_release(r, cells);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(end_with_both, mmgr_series_cellularum_persistent_end(&ram_a_ctx),
                                     "the cell at the end given back trims the end to the held cell");
    ram.a.persistent_buf_release(q, cells);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_series_cellularum_persistent_end(&ram_a_ctx),
                                     "the held cell given back trims every free cell below it");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, ram.a.persistent_buf_alloc(64u, cells),
                                  "the next take lands where the first did");
}

void test_the_persistent_tier_releases_out_of_order(void)
{
    void *const p = ram.a.persistent_buf_alloc(32u, cells);
    void *const q = ram.a.persistent_buf_alloc(32u, cells);
    void *const r = ram.a.persistent_buf_alloc(32u, cells);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_NOT_NULL(r);

    ram.a.persistent_buf_release(q, cells);
    ram.a.persistent_buf_release(p, cells);
    ram.a.persistent_buf_release(r, cells);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_series_cellularum_persistent_end(&ram_a_ctx),
                                     "every cell released, so the tier must have wound back to base");
}

void test_cells_given_back_below_a_held_cell_come_off_with_it(void)
{
    void *const p = ram.a.persistent_buf_alloc(32u, cells);
    void *const q = ram.a.persistent_buf_alloc(32u, cells);
    void *const keep = ram.a.persistent_buf_alloc(32u, cells);
    const size_t end_with_three = mmgr_series_cellularum_persistent_end(&ram_a_ctx);

    TEST_ASSERT_NOT_NULL(keep);
    ram.a.persistent_buf_release(p, cells);
    ram.a.persistent_buf_release(q, cells);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(end_with_three, mmgr_series_cellularum_persistent_end(&ram_a_ctx),
                                     "two free cells under a held one stay in the tier");
    ram.a.persistent_buf_release(keep, cells);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_series_cellularum_persistent_end(&ram_a_ctx),
                                     "the give at the end trims every free cell below it in turn");

    void *const big = ram.a.persistent_buf_alloc(72u, cells);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, big, "the bytes of all three are the gap's again");
}

void test_a_minimum_security_release_leaves_the_bytes_alone(void)
{
    uint8_t *const p = (uint8_t *)ram.a.persistent_buf_alloc(64u, cells);

    for (size_t i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }
    ram.a.persistent_buf_release(p, cells);

    for (size_t i = 0; i < 64u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, p[i], "an unzeroed release must not touch the bytes");
    }
}

void test_a_maximum_security_release_zeroes_the_bytes_first(void)
{
    uint8_t *const p = (uint8_t *)ram.b.persistent_buf_alloc(64u, cells);

    for (size_t i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }
    ram.b.persistent_buf_release(p, cells);

    for (size_t i = 0; i < 64u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, p[i], "a zeroing release must clear every byte");
    }
}

void test_a_maximum_security_release_zeroes_the_whole_cell(void)
{
    uint8_t *const p = (uint8_t *)ram.b.persistent_buf_alloc(33u, cells);
    const size_t held = mmgr_align_up_buf(33u);
    // The take's alloc_size bounds p at the 33 bytes asked for: the slack past them is reached
    // through the cellblock's storage, which holds it: the same address, derived from the base
    uint8_t *const cell = ram_b_ctx.base + (size_t)(p - ram_b_ctx.base);

    for (size_t i = 0; i < held; i++)
    {
        cell[i] = 0xA5u;
    }
    ram.b.persistent_buf_release(p, cells);

    for (size_t i = 0; i < held; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, cell[i], "the rounded slack was left holding key material");
    }
}

void test_the_zeroing_clears_exactly_what_it_was_given(void)
{
    static uint8_t probe[128];

    for (size_t n = 1u; n <= 64u; n++)
    {
        for (size_t i = 0; i < sizeof probe; i++)
        {
            probe[i] = 0xA5u;
        }
        mmgr_zero_buf(probe, n);

        for (size_t i = 0; i < n; i++)
        {
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, probe[i], "a byte inside the request survived the zeroing");
        }
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, probe[n], "the zeroing cleared past what it was given");
    }
}

void test_a_mark_releases_everything_taken_after_it(void)
{
    const size_t before = ram.a.temporary_buf_mark();

    (void)ram.a.temporary_buf_alloc(64u, cells);
    (void)ram.a.temporary_buf_alloc(128u, cells);
    TEST_ASSERT_TRUE(mmgr_series_cellularum_temporary_top(&ram_a_ctx) < before);

    ram.a.temporary_buf_release(before);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, mmgr_series_cellularum_temporary_top(&ram_a_ctx),
                                     "the top must come back to where it was marked");
}

void test_marks_nest_because_the_caller_holds_them(void)
{
    const size_t outer = ram.a.temporary_buf_mark();

    (void)ram.a.temporary_buf_alloc(64u, cells);

    const size_t inner = ram.a.temporary_buf_mark();

    (void)ram.a.temporary_buf_alloc(32u, cells);

    ram.a.temporary_buf_release(inner);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(inner, mmgr_series_cellularum_temporary_top(&ram_a_ctx), "the inner savepoint comes back");

    ram.a.temporary_buf_release(outer);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(outer, mmgr_series_cellularum_temporary_top(&ram_a_ctx),
                                     "and the outer one still stands behind it");
}

void test_reset_releases_the_whole_temporary_tier(void)
{
    (void)ram.a.temporary_buf_alloc(128u, cells);
    ram.a.temporary_buf_reset();

    TEST_ASSERT_EQUAL_size_t(ram_a_ctx.size, mmgr_series_cellularum_temporary_top(&ram_a_ctx));
}

void test_who_owns_buf_tells_a_series_cellularum_from_its_neighbor(void)
{
    void *p = ram.a.persistent_buf_alloc(32u, cells);
    void *q = ram.b.persistent_buf_alloc(32u, cells);

    TEST_ASSERT_TRUE(ram.a.who_owns_buf(p));
    TEST_ASSERT_TRUE(ram.b.who_owns_buf(q));
    TEST_ASSERT_FALSE_MESSAGE(ram.a.who_owns_buf(q), "a neighbor's bytes are not ours");
    TEST_ASSERT_FALSE_MESSAGE(ram.b.who_owns_buf(p), "and ours are not the neighbor's");
}

void test_who_owns_buf_refuses_the_edges(void)
{
    const uint8_t *const base = ram_a_ctx.base;
    const size_t size = ram_a_ctx.size;

    TEST_ASSERT_TRUE(ram.a.who_owns_buf(base));
    TEST_ASSERT_TRUE(ram.a.who_owns_buf(base + size - 1u));
    TEST_ASSERT_FALSE_MESSAGE(ram.a.who_owns_buf(base + size), "one past the end is outside");
    TEST_ASSERT_FALSE_MESSAGE(ram.a.who_owns_buf(base - 1), "one below the base is outside");
    TEST_ASSERT_FALSE(ram.a.who_owns_buf(NULL));
}

void test_the_series_cellularum_do_not_share_a_fill_point(void)
{
    (void)ram.a.persistent_buf_alloc(64u, cells);

    TEST_ASSERT_TRUE(mmgr_series_cellularum_persistent_end(&ram_a_ctx) > 0u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_series_cellularum_persistent_end(&ram_b_ctx),
                                     "one cellblock filling must not move the other");
}

void test_a_span_over_series_cellularum_bytes_carries_the_series_cellularum_address(void)
{
    uint8_t *const p = (uint8_t *)ram.a.persistent_buf_alloc(64u, cells);
    const mmgr_span s = EMBED_CALL(spat.from, SpatiumCfg, .buf = p, .cap = 64u);

    TEST_ASSERT_TRUE_MESSAGE(ram.a.who_owns_buf(s.buf),
                             "the cellblock the span was allocated from still owns its bytes");
}

void test_the_newest_persistent_cell_grows_in_place(void)
{
    uint8_t *const cell = ram.b.persistent_buf_alloc(32u, grower);
    const size_t end_before = mmgr_series_cellularum_persistent_end(&ram_b_ctx);

    TEST_ASSERT_NOT_NULL(cell);
    for (size_t at = 0u; at < 32u; at += 1u)
    {
        cell[at] = 0xA5u;
    }
    uint8_t *const grown = ram.b.buf_grow(cell, 96u, grower);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(cell, grown, "a persistent cell keeps its address as it grows");
    TEST_ASSERT_EQUAL_size_t(end_before + mmgr_align_up_buf(96u) - mmgr_align_up_buf(32u),
                             mmgr_series_cellularum_persistent_end(&ram_b_ctx));
    for (size_t at = 0u; at < 32u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, grown[at], "the payload keeps its bytes");
    }
    for (size_t at = 32u; at < 96u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, grown[at], "the bytes the growth uncovers are zero");
    }
    TEST_ASSERT_EQUAL_size_t(mmgr_align_up_buf(96u), mmgr_owner_in_use(grower));
    uint8_t *const next = ram.b.persistent_buf_alloc(8u, cells);

    TEST_ASSERT_TRUE_MESSAGE(next >= grown + mmgr_align_up_buf(96u), "the next take lands past the grown cell");
    ram.b.persistent_buf_release(next, cells);
    ram.b.persistent_buf_release(grown, grower);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(grower));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_series_cellularum_persistent_end(&ram_b_ctx));
}

void test_a_persistent_cell_with_another_above_it_does_not_grow(void)
{
    void *const first = ram.a.persistent_buf_alloc(32u, cells);
    void *const second = ram.a.persistent_buf_alloc(32u, cells);
    const size_t end_before = mmgr_series_cellularum_persistent_end(&ram_a_ctx);

    TEST_ASSERT_NULL_MESSAGE(ram.a.buf_grow(first, 96u, cells), "a cell under another is not the newest");
    TEST_ASSERT_EQUAL_size_t(end_before, mmgr_series_cellularum_persistent_end(&ram_a_ctx));
    ram.a.persistent_buf_release(second, cells);
    ram.a.persistent_buf_release(first, cells);
}

// the gap spent on the persistent tier, every cell of it counted to the grower
static void gap_spent(void)
{
    while (ram.a.persistent_buf_alloc(64u, grower) != NULL)
    {
    }
}

void test_a_take_the_gap_cannot_meet_reuses_a_free_cell(void)
{
    void *const first = ram.a.persistent_buf_alloc(64u, grower);
    void *const held = ram.a.persistent_buf_alloc(64u, grower);

    gap_spent();
    TEST_ASSERT_NULL_MESSAGE(ram.a.persistent_buf_alloc(64u, grower), "the gap is spent and no cell is free");
    ram.a.persistent_buf_release(first, grower);
    TEST_ASSERT_TRUE_MESSAGE(ram.a.persistent_buf_alloc(64u, grower) == first, "the free cell is reused");
    TEST_ASSERT_NOT_NULL(held);
}

void test_a_reused_cell_splits_what_it_does_not_need(void)
{
    void *const large = ram.a.persistent_buf_alloc(256u, grower);
    void *const held = ram.a.persistent_buf_alloc(64u, grower);

    gap_spent();
    ram.a.persistent_buf_release(large, grower);

    void *const first = ram.a.persistent_buf_alloc(64u, grower);
    void *const second = ram.a.persistent_buf_alloc(64u, grower);

    TEST_ASSERT_TRUE_MESSAGE(first == large, "the first take lands on the free cell");
    TEST_ASSERT_TRUE_MESSAGE((char *)second > (char *)first, "the second lands on what the split left");
    TEST_ASSERT_TRUE_MESSAGE((char *)second < (char *)held, "inside the free cell");
}

void test_a_cell_given_back_merges_with_the_free_cell_above_it(void)
{
    void *const one = ram.a.persistent_buf_alloc(64u, grower);
    void *const two = ram.a.persistent_buf_alloc(64u, grower);
    void *const held = ram.a.persistent_buf_alloc(64u, grower);

    gap_spent();
    ram.a.persistent_buf_release(two, grower);
    ram.a.persistent_buf_release(one, grower);
    TEST_ASSERT_TRUE_MESSAGE(ram.a.persistent_buf_alloc(128u, grower) == one, "two neighbors carry what neither does");
    TEST_ASSERT_NOT_NULL(held);
}

void test_a_cell_given_back_merges_with_the_free_cell_below_it(void)
{
    void *const one = ram.a.persistent_buf_alloc(64u, grower);
    void *const two = ram.a.persistent_buf_alloc(64u, grower);
    void *const held = ram.a.persistent_buf_alloc(64u, grower);

    gap_spent();
    ram.a.persistent_buf_release(one, grower);
    ram.a.persistent_buf_release(two, grower);
    TEST_ASSERT_TRUE_MESSAGE(ram.a.persistent_buf_alloc(128u, grower) == one, "two neighbors carry what neither does");
    TEST_ASSERT_NOT_NULL(held);
}

void test_temporary_cells_given_back_in_any_order_return_to_the_gap(void)
{
    void *const first = ram.a.temporary_buf_alloc(64u, grower);
    void *const second = ram.a.temporary_buf_alloc(64u, grower);
    void *const third = ram.a.temporary_buf_alloc(64u, grower);

    ram.a.persistent_buf_release(second, grower);
    ram.a.persistent_buf_release(first, grower);
    TEST_ASSERT_TRUE_MESSAGE(ram.a.buf_available() < A_BYTES, "the cell at the top still holds the tier");
    ram.a.persistent_buf_release(third, grower);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(A_BYTES, ram.a.buf_available(),
                                     "the last give trims the top over every free cell above it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_owner_in_use(grower), "and each came off its owner once");
}

// the gap spent on the temporary tier, every cell of it counted to the grower
static void temporary_gap_spent(void)
{
    while (ram.a.temporary_buf_alloc(64u, grower) != NULL)
    {
    }
}

void test_a_temporary_take_the_gap_cannot_meet_is_refused_though_a_free_cell_of_its_tier_stands(void)
{
    void *const above = ram.a.temporary_buf_alloc(64u, grower);
    void *const freed = ram.a.temporary_buf_alloc(64u, grower);
    void *const below = ram.a.temporary_buf_alloc(64u, grower);

    temporary_gap_spent();
    TEST_ASSERT_NULL_MESSAGE(ram.a.temporary_buf_alloc(64u, grower), "the gap is spent");
    ram.a.persistent_buf_release(freed, grower);
    const size_t counted = mmgr_owner_in_use(grower);

    TEST_ASSERT_NULL_MESSAGE(ram.a.temporary_buf_alloc(64u, grower),
                             "the tier is a stack: a free cell inside it is a rewind's to return");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(counted, mmgr_owner_in_use(grower), "nothing is counted");
    TEST_ASSERT_NOT_NULL(above);
    TEST_ASSERT_NOT_NULL(below);
    ram.a.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_owner_in_use(grower), "the reset counts the rest off");
}

void test_each_take_stays_on_its_own_tier_when_the_gap_is_spent(void)
{
    void *const persistent = ram.a.persistent_buf_alloc(64u, grower);
    void *const persistent_held = ram.a.persistent_buf_alloc(64u, grower);
    void *const temporary_held = ram.a.temporary_buf_alloc(64u, grower);
    void *const temporary = ram.a.temporary_buf_alloc(64u, grower);
    void *const temporary_below = ram.a.temporary_buf_alloc(64u, grower);

    temporary_gap_spent();
    ram.a.persistent_buf_release(temporary, grower);
    TEST_ASSERT_NULL_MESSAGE(ram.a.persistent_buf_alloc(64u, grower), "a persistent take leaves a temporary cell");
    ram.a.persistent_buf_release(persistent, grower);
    TEST_ASSERT_NULL_MESSAGE(ram.a.temporary_buf_alloc(64u, grower), "a temporary take leaves a persistent cell");
    TEST_ASSERT_TRUE_MESSAGE(ram.a.persistent_buf_alloc(64u, grower) == persistent,
                             "the persistent tier's own take reuses its free cell");
    TEST_ASSERT_NOT_NULL(persistent_held);
    TEST_ASSERT_NOT_NULL(temporary_held);
    TEST_ASSERT_NOT_NULL(temporary_below);
}

void test_a_temporary_cell_given_back_alone_goes_back_with_the_rest_at_a_rewind(void)
{
    void *const above = ram.a.temporary_buf_alloc(64u, grower);
    void *const freed = ram.a.temporary_buf_alloc(64u, grower);
    void *const below = ram.a.temporary_buf_alloc(64u, grower);

    ram.a.persistent_buf_release(freed, grower);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(128u, mmgr_owner_in_use(grower), "the give takes its bytes off");
    ram.a.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(A_BYTES, ram.a.buf_available(), "the rewind returns the whole tier");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_owner_in_use(grower), "and counts the given cell off no second time");
    TEST_ASSERT_NOT_NULL(above);
    TEST_ASSERT_NOT_NULL(below);
}

void test_a_rewind_passes_a_temporary_cell_given_back_alone_and_counts_it_once(void)
{
    void *const above = ram.a.temporary_buf_alloc(64u, grower);
    const size_t mark = ram.a.temporary_buf_mark();
    void *const freed = ram.a.temporary_buf_alloc(64u, grower);
    void *const below = ram.a.temporary_buf_alloc(64u, grower);

    ram.a.persistent_buf_release(freed, grower);
    TEST_ASSERT_EQUAL_size_t(128u, mmgr_owner_in_use(grower));
    ram.a.temporary_buf_release(mark);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(64u, mmgr_owner_in_use(grower), "the rewind drops the cell below the mark once");
    TEST_ASSERT_EQUAL_size_t(mark, mmgr_series_cellularum_temporary_top(&ram_a_ctx));
    TEST_ASSERT_NOT_NULL(above);
    TEST_ASSERT_NOT_NULL(below);
}

void test_the_top_temporary_cell_grows_by_moving_down(void)
{
    uint8_t *const cell = ram.b.temporary_buf_alloc(32u, grower);
    const size_t top_before = mmgr_series_cellularum_temporary_top(&ram_b_ctx);

    TEST_ASSERT_NOT_NULL(cell);
    for (size_t at = 0u; at < 32u; at += 1u)
    {
        cell[at] = 0x5Au;
    }
    uint8_t *const grown = ram.b.buf_grow(cell, 96u, grower);
    const size_t growth = mmgr_align_up_buf(96u) - mmgr_align_up_buf(32u);

    TEST_ASSERT_NOT_NULL(grown);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(cell - growth, grown, "a temporary cell moves down by its growth");
    TEST_ASSERT_EQUAL_size_t(top_before - growth, mmgr_series_cellularum_temporary_top(&ram_b_ctx));
    for (size_t at = 0u; at < 32u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x5Au, grown[at], "the payload keeps its bytes, moved");
    }
    for (size_t at = 32u; at < 96u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, grown[at], "the bytes the growth uncovers are zero");
    }
    TEST_ASSERT_EQUAL_size_t_MESSAGE(mmgr_align_up_buf(96u), mmgr_owner_in_use(grower),
                                     "the growth counts to the owner the cell names");
    ram.b.temporary_buf_reset();
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_owner_in_use(grower));
}

void test_a_temporary_cell_below_the_top_does_not_grow(void)
{
    void *const first = ram.a.temporary_buf_alloc(32u, cells);
    void *const second = ram.a.temporary_buf_alloc(32u, cells);
    const size_t top_before = mmgr_series_cellularum_temporary_top(&ram_a_ctx);

    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_NULL_MESSAGE(ram.a.buf_grow(first, 96u, cells), "a cell under the top is not the newest");
    TEST_ASSERT_EQUAL_size_t(top_before, mmgr_series_cellularum_temporary_top(&ram_a_ctx));
    ram.a.temporary_buf_reset();
}

void test_a_growth_the_gap_cannot_meet_is_refused(void)
{
    void *const cell = ram.a.persistent_buf_alloc(32u, cells);
    const size_t end_before = mmgr_series_cellularum_persistent_end(&ram_a_ctx);

    TEST_ASSERT_NULL(ram.a.buf_grow(cell, A_BYTES, cells));
    TEST_ASSERT_EQUAL_size_t(end_before, mmgr_series_cellularum_persistent_end(&ram_a_ctx));
    ram.a.persistent_buf_release(cell, cells);
}

void test_a_cell_carrying_the_size_asked_is_answered_as_it_stands(void)
{
    void *const cell = ram.a.persistent_buf_alloc(64u, cells);
    const size_t end_before = mmgr_series_cellularum_persistent_end(&ram_a_ctx);

    TEST_ASSERT_EQUAL_PTR(cell, ram.a.buf_grow(cell, 16u, cells));
    TEST_ASSERT_EQUAL_size_t(end_before, mmgr_series_cellularum_persistent_end(&ram_a_ctx));
    TEST_ASSERT_NULL(ram.a.buf_grow(NULL, 16u, cells));
    ram.a.persistent_buf_release(cell, cells);
}
