#include "locus_carcerum/locus_carcerum.h"
#include "spatium/spatium.h"

#include "unity.h"

#define A_BYTES 1024u
#define B_BYTES 1024u

LocusCarcerum(ram, MMGR_MINIMUM_SECURITY(a, A_BYTES), MMGR_MAXIMUM_SECURITY(b, B_BYTES));

#define QUAD_BYTES 256u

LocusCarcerum(quad, MMGR_MINIMUM_SECURITY(q0, QUAD_BYTES), MMGR_MINIMUM_SECURITY(q1, QUAD_BYTES),
              MMGR_MINIMUM_SECURITY(q2, QUAD_BYTES), MMGR_MINIMUM_SECURITY(q3, QUAD_BYTES));

#define OCTO_BYTES 128u

LocusCarcerum(octo, MMGR_MINIMUM_SECURITY(o0, OCTO_BYTES), MMGR_MINIMUM_SECURITY(o1, OCTO_BYTES),
              MMGR_MINIMUM_SECURITY(o2, OCTO_BYTES), MMGR_MINIMUM_SECURITY(o3, OCTO_BYTES),
              MMGR_MINIMUM_SECURITY(o4, OCTO_BYTES), MMGR_MINIMUM_SECURITY(o5, OCTO_BYTES),
              MMGR_MINIMUM_SECURITY(o6, OCTO_BYTES), MMGR_MINIMUM_SECURITY(o7, OCTO_BYTES));

void setUp(void)
{
    ram_a_ctx.persistent_end = 0u;
    ram_b_ctx.persistent_end = 0u;
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
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ram_a_bytes, ram_a_ctx.base, "a cellblock starts at its own storage");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ram_b_bytes, ram_b_ctx.base, "and so does its neighbor");
}

void test_a_site_does_not_pay_for_the_ceiling(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(A_BYTES, ram_a_ctx.size, "a cellblock is the size its declaration gave it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(OCTO_BYTES, octo_o0_ctx.size,
                                     "and another site's cellblocks cost this one nothing");
}

void test_a_four_cellblock_site_builds_all_four(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(QUAD_BYTES, quad_q0_ctx.size, "the count is not capped at two");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(QUAD_BYTES, quad_q3_ctx.size, "and the fourth is a whole cellblock");
}

void test_a_four_cellblock_site_lays_them_end_to_end(void)
{
    TEST_ASSERT_EQUAL_PTR(quad_q0_bytes, quad_q0_ctx.base);
    TEST_ASSERT_EQUAL_PTR(quad_q1_bytes, quad_q1_ctx.base);
    TEST_ASSERT_EQUAL_PTR(quad_q2_bytes, quad_q2_ctx.base);
    TEST_ASSERT_EQUAL_PTR(quad_q3_bytes, quad_q3_ctx.base);
}

void test_every_cellblock_of_a_four_cellblock_site_allocates_from_its_own_bytes(void)
{
    void *const g0 = quad.q0.persistent_buf_alloc(32u);
    void *const g1 = quad.q1.persistent_buf_alloc(32u);
    void *const g2 = quad.q2.persistent_buf_alloc(32u);
    void *const g3 = quad.q3.persistent_buf_alloc(32u);

    TEST_ASSERT_NOT_NULL_MESSAGE(g0, "an allocation from a declared cellblock must succeed");
    TEST_ASSERT_TRUE_MESSAGE(quad.q0.who_owns_buf(g0), "and must land inside that cellblock");
    TEST_ASSERT_TRUE_MESSAGE(quad.q1.who_owns_buf(g1), "and must land inside that cellblock");
    TEST_ASSERT_TRUE_MESSAGE(quad.q2.who_owns_buf(g2), "and must land inside that cellblock");
    TEST_ASSERT_TRUE_MESSAGE(quad.q3.who_owns_buf(g3), "and must land inside that cellblock");
    TEST_ASSERT_FALSE_MESSAGE(quad.q0.who_owns_buf(g3), "a neighbor's bytes are not ours");
}

void test_an_eight_cellblock_site_builds_and_allocates(void)
{
    TEST_ASSERT_EQUAL_PTR(octo_o0_bytes, octo_o0_ctx.base);
    TEST_ASSERT_EQUAL_PTR(octo_o7_bytes, octo_o7_ctx.base);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(OCTO_BYTES, octo_o7_ctx.size, "eight cellblocks is a legal count");

    void *const first = octo.o0.persistent_buf_alloc(16u);
    void *const last = octo.o7.persistent_buf_alloc(16u);

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_TRUE_MESSAGE(octo.o0.who_owns_buf(first), "the first cellblock hands out storage inside itself");
    TEST_ASSERT_TRUE_MESSAGE(octo.o7.who_owns_buf(last), "and so does the eighth");
    TEST_ASSERT_FALSE_MESSAGE(octo.o0.who_owns_buf(last), "which is not the first cellblock's");
}

void test_two_sites_may_share_cellblock_names(void)
{
    TEST_ASSERT_TRUE_MESSAGE((uintptr_t)ram_a_bytes != (uintptr_t)quad_q0_bytes,
                             "two sites' cellblocks are separate objects");
}

void test_init_records_the_site_it_was_given(void)
{
    TEST_ASSERT_EQUAL_PTR(ram_a_bytes, ram_a_ctx.base);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram_a_ctx.size);
    TEST_ASSERT_EQUAL_size_t(B_BYTES, ram_b_ctx.size);
}

void test_the_build_lays_the_cellblocks_end_to_end(void)
{
    TEST_ASSERT_EQUAL_PTR(ram_a_bytes, ram_a_ctx.base);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram_a_ctx.size);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ram_b_bytes, ram_b_ctx.base, "and the second on its own");
    TEST_ASSERT_EQUAL_size_t(B_BYTES, ram_b_ctx.size);
}

void test_a_fresh_cellblock_is_empty_and_whole(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, ram_a_ctx.persistent_end);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram.a.buf_available());
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram.a.temporary_buf_mark());
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

        void *const p = ram.a.persistent_buf_alloc(n);
        void *const q = ram.a.temporary_buf_alloc(n);

        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_NOT_NULL(q);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)p % MMGR_CARCER_ALIGN, "a persistent cell is misaligned");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)q % MMGR_CARCER_ALIGN, "a temporary cell is misaligned");
    }
}

void test_the_persistent_tier_hands_out_the_bottom_and_walks_up(void)
{
    uint8_t *p = (uint8_t *)ram.a.persistent_buf_alloc(64u);
    uint8_t *q = (uint8_t *)ram.a.persistent_buf_alloc(64u);

    TEST_ASSERT_TRUE_MESSAGE(p < q, "the next cell sits above the first");
    TEST_ASSERT_TRUE(ram.a.who_owns_buf(p));
}

void test_the_temporary_tier_hands_out_the_top_and_walks_down(void)
{
    uint8_t *p = (uint8_t *)ram.a.temporary_buf_alloc(64u);
    uint8_t *q = (uint8_t *)ram.a.temporary_buf_alloc(64u);

    TEST_ASSERT_TRUE_MESSAGE(q < p, "the next cell sits below the first");
    TEST_ASSERT_TRUE(ram.a.who_owns_buf(p));
}

void test_the_two_tiers_allocate_from_the_same_gap(void)
{
    const size_t room = ram.a.buf_available();

    (void)ram.a.persistent_buf_alloc(64u);
    (void)ram.a.temporary_buf_alloc(64u);

    TEST_ASSERT_TRUE_MESSAGE(ram.a.buf_available() < room, "both tiers allocate from the same gap");
}

void test_both_tiers_fail_closed(void)
{
    TEST_ASSERT_NULL(ram.a.persistent_buf_alloc(A_BYTES * 4u));
    TEST_ASSERT_NULL(ram.a.temporary_buf_alloc(A_BYTES * 4u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(A_BYTES, ram.a.buf_available(),
                                     "a refused request must not have moved a boundary");
}

void test_a_released_cell_is_reused(void)
{
    void *const p = ram.a.persistent_buf_alloc(64u);

    (void)ram.a.persistent_buf_alloc(64u);
    ram.a.persistent_buf_release(p);

    void *const q = ram.a.persistent_buf_alloc(64u);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, q, "the released cell is the one that fits");
}

void test_the_persistent_tier_releases_out_of_order(void)
{
    void *const p = ram.a.persistent_buf_alloc(32u);
    void *const q = ram.a.persistent_buf_alloc(32u);
    void *const r = ram.a.persistent_buf_alloc(32u);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_NOT_NULL(r);

    ram.a.persistent_buf_release(q);
    ram.a.persistent_buf_release(p);
    ram.a.persistent_buf_release(r);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_a_ctx.persistent_end,
                                     "every cell released, so the tier must have wound back to base");
}

void test_adjacent_empty_cells_merge(void)
{
    void *const p = ram.a.persistent_buf_alloc(32u);
    void *const q = ram.a.persistent_buf_alloc(32u);
    void *const keep = ram.a.persistent_buf_alloc(32u);

    TEST_ASSERT_NOT_NULL(keep);
    ram.a.persistent_buf_release(p);
    ram.a.persistent_buf_release(q);

    void *const big = ram.a.persistent_buf_alloc(72u);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, big, "the two released cells must have merged into one");
}

void test_a_minimum_security_release_leaves_the_bytes_alone(void)
{
    uint8_t *const p = (uint8_t *)ram.a.persistent_buf_alloc(64u);

    for (size_t i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }
    ram.a.persistent_buf_release(p);

    for (size_t i = 0; i < 64u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, p[i], "an unzeroed release must not touch the bytes");
    }
}

void test_a_maximum_security_release_zeroes_the_bytes_first(void)
{
    uint8_t *const p = (uint8_t *)ram.b.persistent_buf_alloc(64u);

    for (size_t i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }
    ram.b.persistent_buf_release(p);

    for (size_t i = 0; i < 64u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, p[i], "a zeroing release must clear every byte");
    }
}

void test_a_maximum_security_release_zeroes_the_whole_cell(void)
{

    uint8_t *const p = (uint8_t *)ram.b.persistent_buf_alloc(33u);
    const size_t held = mmgr_align_up_buf(33u);

    for (size_t i = 0; i < held; i++)
    {
        p[i] = 0xA5u;
    }
    ram.b.persistent_buf_release(p);

    for (size_t i = 0; i < held; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, p[i], "the rounded slack was left holding key material");
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

    (void)ram.a.temporary_buf_alloc(64u);
    (void)ram.a.temporary_buf_alloc(128u);
    TEST_ASSERT_TRUE(ram_a_ctx.temporary_top < before);

    ram.a.temporary_buf_release(before);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, ram_a_ctx.temporary_top, "the top must come back to where it was marked");
}

void test_marks_nest_because_the_caller_holds_them(void)
{
    const size_t outer = ram.a.temporary_buf_mark();

    (void)ram.a.temporary_buf_alloc(64u);

    const size_t inner = ram.a.temporary_buf_mark();

    (void)ram.a.temporary_buf_alloc(32u);

    ram.a.temporary_buf_release(inner);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(inner, ram_a_ctx.temporary_top, "the inner savepoint comes back");

    ram.a.temporary_buf_release(outer);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(outer, ram_a_ctx.temporary_top, "and the outer one still stands behind it");
}

void test_reset_releases_the_whole_temporary_tier(void)
{
    (void)ram.a.temporary_buf_alloc(128u);
    ram.a.temporary_buf_reset();

    TEST_ASSERT_EQUAL_size_t(ram_a_ctx.size, ram_a_ctx.temporary_top);
}

void test_who_owns_buf_tells_a_cellblock_from_its_neighbor(void)
{
    void *p = ram.a.persistent_buf_alloc(32u);
    void *q = ram.b.persistent_buf_alloc(32u);

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

void test_the_cellblocks_do_not_share_a_fill_point(void)
{
    (void)ram.a.persistent_buf_alloc(64u);

    TEST_ASSERT_TRUE(ram_a_ctx.persistent_end > 0u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_b_ctx.persistent_end, "one cellblock filling must not move the other");
}

void test_a_span_over_cellblock_bytes_carries_the_cellblock_address(void)
{
    uint8_t *const p = (uint8_t *)ram.a.persistent_buf_alloc(64u);
    const mmgr_span s = EMBED_CALL(spat.from, SpatiumCfg, .buf = p, .cap = 64u);

    TEST_ASSERT_TRUE_MESSAGE(ram.a.who_owns_buf(s.buf),
                             "the cellblock the span was allocated from still owns its bytes");
}
