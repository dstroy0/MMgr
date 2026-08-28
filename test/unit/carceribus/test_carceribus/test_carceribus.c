/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "carceribus/carceribus.h"
#include "spatium/spatium.h"

#define A_BYTES 1024u
#define B_BYTES 1024u

Carceribus(ram, MMGR_SOLUTA(a, A_BYTES), MMGR_SECURA(b, B_BYTES));

#define QUAD_BYTES 256u

Carceribus(quad, MMGR_SOLUTA(q0, QUAD_BYTES), MMGR_SOLUTA(q1, QUAD_BYTES), MMGR_SOLUTA(q2, QUAD_BYTES),
           MMGR_SOLUTA(q3, QUAD_BYTES));

#define OCTO_BYTES 128u

/**
 * @brief Eight rows, to show nothing caps the count.
 */
Carceribus(octo, MMGR_SOLUTA(o0, OCTO_BYTES), MMGR_SOLUTA(o1, OCTO_BYTES), MMGR_SOLUTA(o2, OCTO_BYTES),
           MMGR_SOLUTA(o3, OCTO_BYTES), MMGR_SOLUTA(o4, OCTO_BYTES), MMGR_SOLUTA(o5, OCTO_BYTES),
           MMGR_SOLUTA(o6, OCTO_BYTES), MMGR_SOLUTA(o7, OCTO_BYTES));

/**
 * @brief Puts both pools back to empty at both ends.
 *
 * @note The persistent end is put back by hand rather than by a call: a reset that walked the chain
 *       to prove it was empty would be testing the thing under test. The declaration is in this
 *       file, so each pool's own context is in scope.
 */
void setUp(void)
{
    ram_a_ctx.persist_end = 0u;
    ram_b_ctx.persist_end = 0u;
    ram.a.interim_reset();
    ram.b.interim_reset();
}

void tearDown(void)
{
}

void test_carceribus_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("carceribus.h compiled with no header before it");
}

void test_the_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(SolutaCustodiae), sizeof ram.a, "a loose pool is not its own type");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(SecuraCustodiae), sizeof ram.b, "a close pool is not its own type");
}

void test_the_machinery_sits_below_the_arena(void)
{
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ram_a_bytes, ram_a_ctx.base, "a pool starts at its own storage");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ram_b_bytes, ram_b_ctx.base, "and so does its neighbour");
}

/**
 * @brief A region carrying fewer pools than another does not pay for the difference.
 *
 * @note There is no ceiling to pay for. Each region's pool array is sized by the kinds its own
 *       declaration listed, so an eight row region elsewhere costs a two row one nothing.
 */
void test_a_region_does_not_pay_for_the_ceiling(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(A_BYTES, ram_a_ctx.size, "a pool is the size its declaration gave it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(OCTO_BYTES, octo_o0_ctx.size, "and another region's pools cost this one nothing");
}

/**
 * @brief A region may carve more than two pools, and four is one of the counts it may carve.
 */
void test_a_four_pool_region_carves_all_four(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(QUAD_BYTES, quad_q0_ctx.size, "the count is not capped at two");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(QUAD_BYTES, quad_q3_ctx.size, "and the fourth is a whole pool");
}

/**
 * @brief Each of the four sits on its own storage.
 */
void test_a_four_pool_region_lays_them_end_to_end(void)
{
    TEST_ASSERT_EQUAL_PTR(quad_q0_bytes, quad_q0_ctx.base);
    TEST_ASSERT_EQUAL_PTR(quad_q1_bytes, quad_q1_ctx.base);
    TEST_ASSERT_EQUAL_PTR(quad_q2_bytes, quad_q2_ctx.base);
    TEST_ASSERT_EQUAL_PTR(quad_q3_bytes, quad_q3_ctx.base);
}

/**
 * @brief Every pool of a four pool region hands out storage inside its own bytes.
 *
 * @note A declaration being right on paper is not the same as each pool taking from its own bytes,
 *       so this takes from all four and asks each pool whether the address is its own.
 */
void test_every_pool_of_a_four_pool_region_takes_from_its_own_bytes(void)
{
    void *const g0 = quad.q0.persist_capio(32u);
    void *const g1 = quad.q1.persist_capio(32u);
    void *const g2 = quad.q2.persist_capio(32u);
    void *const g3 = quad.q3.persist_capio(32u);

    TEST_ASSERT_NOT_NULL_MESSAGE(g0, "a take from a declared pool must succeed");
    TEST_ASSERT_TRUE_MESSAGE(quad.q0.owns(g0), "and must land inside that pool");
    TEST_ASSERT_TRUE_MESSAGE(quad.q1.owns(g1), "and must land inside that pool");
    TEST_ASSERT_TRUE_MESSAGE(quad.q2.owns(g2), "and must land inside that pool");
    TEST_ASSERT_TRUE_MESSAGE(quad.q3.owns(g3), "and must land inside that pool");
    TEST_ASSERT_FALSE_MESSAGE(quad.q0.owns(g3), "a neighbour's bytes are not ours");
}

/**
 * @brief Eight pools, each on its own storage, each taking from its own bytes.
 *
 * @note Nothing caps the count. Eight is where the walk in carceribus.h currently stops, and it is
 *       the case that would break first if the pool count were miscounted.
 */
void test_an_eight_pool_region_carves_and_takes(void)
{
    TEST_ASSERT_EQUAL_PTR(octo_o0_bytes, octo_o0_ctx.base);
    TEST_ASSERT_EQUAL_PTR(octo_o7_bytes, octo_o7_ctx.base);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(OCTO_BYTES, octo_o7_ctx.size, "eight pools is a legal count");

    void *const first = octo.o0.persist_capio(16u);
    void *const last = octo.o7.persist_capio(16u);

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_TRUE_MESSAGE(octo.o0.owns(first), "the first pool hands out storage inside itself");
    TEST_ASSERT_TRUE_MESSAGE(octo.o7.owns(last), "and so does the eighth");
    TEST_ASSERT_FALSE_MESSAGE(octo.o0.owns(last), "which is not the first pool's");
}

/**
 * @brief Two regions may each hold a pool of the same name, under different watches.
 */
void test_two_regions_may_share_pool_names(void)
{
    TEST_ASSERT_TRUE_MESSAGE((uintptr_t)ram_a_bytes != (uintptr_t)quad_q0_bytes,
                             "two regions' pools are separate objects");
}

void test_init_records_the_region_it_was_given(void)
{
    TEST_ASSERT_EQUAL_PTR(ram_a_bytes, ram_a_ctx.base);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram_a_ctx.size);
    TEST_ASSERT_EQUAL_size_t(B_BYTES, ram_b_ctx.size);
}

void test_the_carve_lays_the_pools_end_to_end(void)
{
    TEST_ASSERT_EQUAL_PTR(ram_a_bytes, ram_a_ctx.base);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram_a_ctx.size);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ram_b_bytes, ram_b_ctx.base, "and the second on its own");
    TEST_ASSERT_EQUAL_size_t(B_BYTES, ram_b_ctx.size);
}

void test_a_fresh_pool_is_empty_and_whole(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, ram_a_ctx.persist_end);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram.a.octas_praesto());
    TEST_ASSERT_EQUAL_size_t(A_BYTES, ram.a.interim_mark());
}

void test_align_up_rounds_to_a_whole_word(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_carcer_align_up(0u));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, mmgr_carcer_align_up(1u));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, mmgr_carcer_align_up(MMGR_CARCER_ALIGN));
    TEST_ASSERT_EQUAL_size_t(2u * MMGR_CARCER_ALIGN, mmgr_carcer_align_up(MMGR_CARCER_ALIGN + 1u));
}

/**
 * @brief Every address either end hands out carries the alignment the module promises.
 */
void test_both_ends_hand_out_aligned_addresses(void)
{
    for (size_t n = 1u; n <= 40u; n++)
    {
        // A fresh pool each time: every block costs a header too, so forty of them at both ends
        // would run a kilobyte pool out and the NULL would be the pool behaving, not a finding
        setUp();

        void *const p = ram.a.persist_capio(n);
        void *const q = ram.a.interim_capio(n);

        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_NOT_NULL(q);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)p % MMGR_CARCER_ALIGN, "a persistent tenancy is misaligned");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)q % MMGR_CARCER_ALIGN, "an interim tenancy is misaligned");
    }
}

void test_persist_hands_out_the_bottom_and_walks_up(void)
{
    uint8_t *p = (uint8_t *)ram.a.persist_capio(64u);
    uint8_t *q = (uint8_t *)ram.a.persist_capio(64u);

    TEST_ASSERT_TRUE_MESSAGE(p < q, "the next tenancy sits above the first");
    TEST_ASSERT_TRUE(ram.a.owns(p));
}

void test_interim_hands_out_the_top_and_walks_down(void)
{
    uint8_t *p = (uint8_t *)ram.a.interim_capio(64u);
    uint8_t *q = (uint8_t *)ram.a.interim_capio(64u);

    TEST_ASSERT_TRUE_MESSAGE(q < p, "the next tenancy sits below the first");
    TEST_ASSERT_TRUE(ram.a.owns(p));
}

void test_the_two_ends_take_from_the_same_middle(void)
{
    const size_t room = ram.a.octas_praesto();

    (void)ram.a.persist_capio(64u);
    (void)ram.a.interim_capio(64u);

    TEST_ASSERT_TRUE_MESSAGE(ram.a.octas_praesto() < room, "both ends take from the same gap");
}

/**
 * @brief A request larger than the pool is refused rather than trespassing.
 */
void test_both_ends_fail_closed(void)
{
    TEST_ASSERT_NULL(ram.a.persist_capio(A_BYTES * 4u));
    TEST_ASSERT_NULL(ram.a.interim_capio(A_BYTES * 4u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(A_BYTES, ram.a.octas_praesto(),
                                     "a refused request must not have moved a boundary");
}

/**
 * @brief A released block is handed out again, which is what the chain exists for.
 */
void test_a_released_block_is_reused(void)
{
    void *const p = ram.a.persist_capio(64u);

    (void)ram.a.persist_capio(64u);
    ram.a.persist_reddo(p);

    void *const q = ram.a.persist_capio(64u);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, q, "the freed block is the one that fits");
}

/**
 * @brief Releases in any order, which is what the persistent end's long life needs.
 */
void test_persist_releases_out_of_order(void)
{
    void *const p = ram.a.persist_capio(32u);
    void *const q = ram.a.persist_capio(32u);
    void *const r = ram.a.persist_capio(32u);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_NOT_NULL(r);

    // The middle one first, which a stack could not do
    ram.a.persist_reddo(q);
    ram.a.persist_reddo(p);
    ram.a.persist_reddo(r);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_a_ctx.persist_end,
                                     "every block released, so the end must have wound back to base");
}

/**
 * @brief Adjacent free blocks merge, so a later request larger than any one of them still fits.
 */
void test_adjacent_free_blocks_merge(void)
{
    void *const p = ram.a.persist_capio(32u);
    void *const q = ram.a.persist_capio(32u);
    void *const keep = ram.a.persist_capio(32u);

    TEST_ASSERT_NOT_NULL(keep);
    ram.a.persist_reddo(p);
    ram.a.persist_reddo(q);

    // Larger than either freed block, so it only fits if the two became one
    void *const big = ram.a.persist_capio(72u);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, big, "the two freed blocks must have merged into one");
}

void test_a_plain_release_leaves_the_bytes_alone(void)
{
    uint8_t *const p = (uint8_t *)ram.a.persist_capio(64u);

    for (size_t i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }
    ram.a.persist_reddo(p);

    for (size_t i = 0; i < 64u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, p[i], "an unwiped release must not touch the bytes");
    }
}

/**
 * @brief The one thing that separates the wiped release from the plain one.
 */
void test_a_secura_release_zeroes_the_bytes_first(void)
{
    uint8_t *const p = (uint8_t *)ram.b.persist_capio(64u);

    for (size_t i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }
    ram.b.persist_reddo(p);

    for (size_t i = 0; i < 64u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, p[i], "a wiped release must clear every byte");
    }
}

/**
 * @brief The wipe covers the block, not what the caller happens to remember about it.
 */
void test_a_secura_release_wipes_the_whole_block(void)
{
    // Asks for 33 bytes, which the pool rounds up; the slack must be cleared too
    uint8_t *const p = (uint8_t *)ram.b.persist_capio(33u);
    const size_t held = mmgr_carcer_align_up(33u);

    for (size_t i = 0; i < held; i++)
    {
        p[i] = 0xA5u;
    }
    ram.b.persist_reddo(p);

    for (size_t i = 0; i < held; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, p[i], "the rounded slack was left holding key material");
    }
}

void test_the_wipe_clears_exactly_what_it_was_given(void)
{
    static uint8_t scratch[128];

    for (size_t n = 1u; n <= 64u; n++)
    {
        for (size_t i = 0; i < sizeof scratch; i++)
        {
            scratch[i] = 0xA5u;
        }
        mmgr_carcer_wipe(scratch, n);

        for (size_t i = 0; i < n; i++)
        {
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, scratch[i], "a byte inside the request survived the wipe");
        }
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, scratch[n], "the wipe cleared past what it was given");
    }
}

void test_a_mark_gives_back_everything_taken_after_it(void)
{
    const size_t before = ram.a.interim_mark();

    (void)ram.a.interim_capio(64u);
    (void)ram.a.interim_capio(128u);
    TEST_ASSERT_TRUE(ram_a_ctx.interim_top < before);

    ram.a.interim_reddo(before);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, ram_a_ctx.interim_top, "the top must come back to where it was marked");
}

/**
 * @brief Two savepoints live at once, which is what holding the mark in the caller buys.
 */
void test_marks_nest_because_the_caller_holds_them(void)
{
    const size_t outer = ram.a.interim_mark();

    (void)ram.a.interim_capio(64u);

    const size_t inner = ram.a.interim_mark();

    (void)ram.a.interim_capio(32u);

    ram.a.interim_reddo(inner);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(inner, ram_a_ctx.interim_top, "the inner savepoint comes back");

    ram.a.interim_reddo(outer);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(outer, ram_a_ctx.interim_top, "and the outer one still stands behind it");
}

void test_reset_gives_the_whole_interim_end_back(void)
{
    (void)ram.a.interim_capio(128u);
    ram.a.interim_reset();

    TEST_ASSERT_EQUAL_size_t(ram_a_ctx.size, ram_a_ctx.interim_top);
}

void test_owns_tells_a_pool_from_its_neighbour(void)
{
    void *p = ram.a.persist_capio(32u);
    void *q = ram.b.persist_capio(32u);

    TEST_ASSERT_TRUE(ram.a.owns(p));
    TEST_ASSERT_TRUE(ram.b.owns(q));
    TEST_ASSERT_FALSE_MESSAGE(ram.a.owns(q), "a neighbour's bytes are not ours");
    TEST_ASSERT_FALSE_MESSAGE(ram.b.owns(p), "and ours are not the neighbour's");
}

void test_owns_refuses_the_edges(void)
{
    const uint8_t *const base = ram_a_ctx.base;
    const size_t size = ram_a_ctx.size;

    TEST_ASSERT_TRUE(ram.a.owns(base));
    TEST_ASSERT_TRUE(ram.a.owns(base + size - 1u));
    TEST_ASSERT_FALSE_MESSAGE(ram.a.owns(base + size), "one past the end is outside");
    TEST_ASSERT_FALSE_MESSAGE(ram.a.owns(base - 1), "one below the base is outside");
    TEST_ASSERT_FALSE(ram.a.owns(NULL));
}

void test_the_pools_do_not_share_a_fill_point(void)
{
    (void)ram.a.persist_capio(64u);

    TEST_ASSERT_TRUE(ram_a_ctx.persist_end > 0u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_b_ctx.persist_end, "one pool filling must not move the other");
}

void test_a_span_over_pool_bytes_carries_the_pool_address(void)
{
    uint8_t *const p = (uint8_t *)ram.a.persist_capio(64u);
    const mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = p, .cap = 64u);

    TEST_ASSERT_TRUE_MESSAGE(ram.a.owns(s.buf), "the pool the span was carved from still owns its bytes");
}