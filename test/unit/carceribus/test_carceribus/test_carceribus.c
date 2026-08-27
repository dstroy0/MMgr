/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "carceribus/carceribus.h"
#include "spatium/spatium.h"

#define A_BYTES 1024u
#define B_BYTES 2048u

mmgr_carcer_init(ram, A_BYTES + B_BYTES, MMGR_POOL(a, A_BYTES), MMGR_POOL(b, B_BYTES));

#define QUAD_BYTES 256u

mmgr_carcer_init(quad, 4u * QUAD_BYTES, MMGR_POOL(q0, QUAD_BYTES), MMGR_POOL(q1, QUAD_BYTES),
                 MMGR_POOL(q2, QUAD_BYTES), MMGR_POOL(q3, QUAD_BYTES));

#define OCTO_BYTES 128u

/**
 * @brief The largest carve there is, which is where MMGR_NARG's argument table runs out.
 */
mmgr_carcer_init(octo, 8u * OCTO_BYTES, MMGR_POOL(o0, OCTO_BYTES), MMGR_POOL(o1, OCTO_BYTES),
                 MMGR_POOL(o2, OCTO_BYTES), MMGR_POOL(o3, OCTO_BYTES), MMGR_POOL(o4, OCTO_BYTES),
                 MMGR_POOL(o5, OCTO_BYTES), MMGR_POOL(o6, OCTO_BYTES), MMGR_POOL(o7, OCTO_BYTES));

/**
 * @brief Puts both pools back to empty at both ends.
 *
 * @note Reaches the members rather than a call: the pool is the caller's own type, so a reset that
 *       walked the chains to prove they were empty would be testing the thing under test.
 * @note Bounded by the region's own count, not by MMGR_CARCER_MAX_REGIONS. The array is sized per
 *       region now, so the ceiling is not a bound on any particular one.
 */
void setUp(void)
{
    for (size_t i = 0; i < (size_t)ram_count; i++)
    {
        ram.pool[i].persist_end = 0u;
        ram.pool[i].interim_top = ram.pool[i].size;
    }
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
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(CarceribusNs), sizeof carcer, "the pool table is not its own type");
}

void test_the_machinery_sits_below_the_arena(void)
{
    TEST_ASSERT_TRUE_MESSAGE((uintptr_t)ram.bytes > (uintptr_t)&ram, "the arena must start above the machinery");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(CarcerCtx) * ram_count, sizeof ram.pool,
                                     "a region sizes its pool array from its own count");
}

/**
 * @brief A region carrying fewer pools than the build allows does not pay for the ceiling.
 *
 * @note This is the whole reason the pool array is sized per region. ram carves two, so raising
 *       MMGR_CARCER_MAX_REGIONS to admit an eight pool region elsewhere must cost ram nothing.
 */
void test_a_region_does_not_pay_for_the_ceiling(void)
{
    TEST_ASSERT_TRUE_MESSAGE(ram_count < MMGR_CARCER_MAX_REGIONS, "ram carves fewer pools than the ceiling allows");
    TEST_ASSERT_TRUE_MESSAGE(sizeof ram.pool < (sizeof(CarcerCtx) * MMGR_CARCER_MAX_REGIONS),
                             "so its pool array must be smaller than the ceiling would give");
}

/**
 * @brief A region may carve more than two pools, and four is one of the counts it may carve.
 */
void test_a_four_pool_region_carves_all_four(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, (size_t)quad_count, "the carve is not capped at two");
    TEST_ASSERT_EQUAL_size_t(sizeof(CarcerCtx) * 4u, sizeof quad.pool);
}

/**
 * @brief The four pools lie end to end, each at the sum of the sizes ahead of it.
 */
void test_a_four_pool_region_lays_them_end_to_end(void)
{
    TEST_ASSERT_EQUAL_PTR(quad.bytes, MMGR_CARCER_POOL(quad, q0)->base);
    TEST_ASSERT_EQUAL_PTR(quad.bytes + QUAD_BYTES, MMGR_CARCER_POOL(quad, q1)->base);
    TEST_ASSERT_EQUAL_PTR(quad.bytes + (2u * QUAD_BYTES), MMGR_CARCER_POOL(quad, q2)->base);
    TEST_ASSERT_EQUAL_PTR(quad.bytes + (3u * QUAD_BYTES), MMGR_CARCER_POOL(quad, q3)->base);
}

/**
 * @brief Every pool of a four pool region hands out storage inside its own bytes.
 *
 * @note The carve being right on paper is not the same as each pool taking from where it was told to,
 *       so this takes from all four and asks the pool itself whether the address is its own.
 */
void test_every_pool_of_a_four_pool_region_takes_from_its_own_bytes(void)
{
    const size_t names[4] = {q0, q1, q2, q3};

    for (size_t i = 0; i < 4u; i++)
    {
        CarcerCtx *const pool = &quad.pool[names[i]];
        void *const got = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = pool, .size = 32u);

        TEST_ASSERT_NOT_NULL_MESSAGE(got, "a take from a carved pool must succeed");
        TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = pool, .at = got),
                                 "and must land inside that pool rather than a neighbour");
    }
}

/**
 * @brief The eight pool carve lays every pool where it was told to and each takes from its own bytes.
 *
 * @note Eight is the ceiling, so this is the case that would break first if MMGR_NARG's table or the
 *       cumulative offsets in MMGR_CARCER_R16 were off by one.
 */
void test_an_eight_pool_region_carves_and_takes(void)
{
    const size_t names[8] = {o0, o1, o2, o3, o4, o5, o6, o7};

    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, (size_t)octo_count, "eight pools is a legal carve");
    TEST_ASSERT_EQUAL_size_t(sizeof(CarcerCtx) * 8u, sizeof octo.pool);

    for (size_t i = 0; i < 8u; i++)
    {
        CarcerCtx *const pool = &octo.pool[names[i]];

        TEST_ASSERT_EQUAL_PTR_MESSAGE(octo.bytes + (i * OCTO_BYTES), pool->base, "pool i starts after the i ahead of it");

        void *const got = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = pool, .size = 16u);

        TEST_ASSERT_NOT_NULL(got);
        TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = pool, .at = got),
                                 "and hands out storage inside itself");
    }
}

void test_init_records_the_region_it_was_given(void)
{
    TEST_ASSERT_EQUAL_PTR(ram.bytes, ram.init.at);
    TEST_ASSERT_EQUAL_size_t(A_BYTES + B_BYTES, ram.init.size);
}

void test_the_carve_lays_the_pools_end_to_end(void)
{
    TEST_ASSERT_EQUAL_PTR(ram.bytes, MMGR_CARCER_POOL(ram, a)->base);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, MMGR_CARCER_POOL(ram, a)->size);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(ram.bytes + A_BYTES, MMGR_CARCER_POOL(ram, b)->base,
                                  "the second pool starts where the first ends");
    TEST_ASSERT_EQUAL_size_t(B_BYTES, MMGR_CARCER_POOL(ram, b)->size);
}

void test_a_fresh_pool_is_empty_and_whole(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CARCER_POOL(ram, a)->persist_end);
    TEST_ASSERT_EQUAL_size_t(A_BYTES, MMGR_CALL(carcer.octas_praesto, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));
    TEST_ASSERT_EQUAL_size_t(A_BYTES, MMGR_CALL(carcer.interim_mark, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));
}

void test_align_up_rounds_to_a_whole_word(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(carcer.align_up, CarcerCfg, .size = 0u));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN, MMGR_CALL(carcer.align_up, CarcerCfg, .size = 1u));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_ALIGN,
                             MMGR_CALL(carcer.align_up, CarcerCfg, .size = MMGR_CARCER_ALIGN));
    TEST_ASSERT_EQUAL_size_t(2u * MMGR_CARCER_ALIGN,
                             MMGR_CALL(carcer.align_up, CarcerCfg, .size = MMGR_CARCER_ALIGN + 1u));
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

        void *const p = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = n);
        void *const q = MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = n);

        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_NOT_NULL(q);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)p % MMGR_CARCER_ALIGN, "a persistent tenancy is misaligned");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (uintptr_t)q % MMGR_CARCER_ALIGN, "an interim tenancy is misaligned");
    }
}

void test_persist_hands_out_the_bottom_and_walks_up(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    uint8_t *q = (uint8_t *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);

    TEST_ASSERT_TRUE_MESSAGE(p < q, "the next tenancy sits above the first");
    TEST_ASSERT_TRUE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = p));
}

void test_interim_hands_out_the_top_and_walks_down(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    uint8_t *q = (uint8_t *)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);

    TEST_ASSERT_TRUE_MESSAGE(q < p, "the next tenancy sits below the first");
    TEST_ASSERT_TRUE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = p));
}

void test_the_two_ends_take_from_the_same_middle(void)
{
    const size_t room = MMGR_CALL(carcer.octas_praesto, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a));

    (void)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    (void)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);

    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(carcer.octas_praesto, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)) < room,
                             "both ends take from the same gap");
}

/**
 * @brief A request larger than the pool is refused rather than trespassing.
 */
void test_both_ends_fail_closed(void)
{
    TEST_ASSERT_NULL(MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a),
                               .size = A_BYTES * 4u));
    TEST_ASSERT_NULL(MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a),
                               .size = A_BYTES * 4u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(A_BYTES,
                                     MMGR_CALL(carcer.octas_praesto, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)),
                                     "a refused request must not have moved a boundary");
}

/**
 * @brief A released block is handed out again, which is what the chain exists for.
 */
void test_a_released_block_is_reused(void)
{
    void *const p = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);

    (void)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .tenancy = p);

    void *const q = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, q, "the freed block is the one that fits");
}

/**
 * @brief Releases in any order, which is what the persistent end's long life needs.
 */
void test_persist_releases_out_of_order(void)
{
    void *const p = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 32u);
    void *const q = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 32u);
    void *const r = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 32u);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_NOT_NULL(r);

    // The middle one first, which a stack could not do
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .tenancy = q);
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .tenancy = p);
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .tenancy = r);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CARCER_POOL(ram, a)->persist_end,
                                     "every block released, so the end must have wound back to base");
}

/**
 * @brief Adjacent free blocks merge, so a later request larger than any one of them still fits.
 */
void test_adjacent_free_blocks_merge(void)
{
    void *const p = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 32u);
    void *const q = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 32u);
    void *const keep = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 32u);

    TEST_ASSERT_NOT_NULL(keep);
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .tenancy = p);
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .tenancy = q);

    // Larger than either freed block, so it only fits if the two became one
    void *const big = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 72u);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(p, big, "the two freed blocks must have merged into one");
}

void test_a_plain_release_leaves_the_bytes_alone(void)
{
    uint8_t *const p = (uint8_t *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a),
                                            .size = 64u);

    for (size_t i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .tenancy = p);

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
    uint8_t *const p = (uint8_t *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a),
                                            .size = 64u);

    for (size_t i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }
    MMGR_CALL(carcer.secura_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .tenancy = p);

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
    uint8_t *const p = (uint8_t *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a),
                                            .size = 33u);
    const size_t held = MMGR_CALL(carcer.align_up, CarcerCfg, .size = 33u);

    for (size_t i = 0; i < held; i++)
    {
        p[i] = 0xA5u;
    }
    MMGR_CALL(carcer.secura_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .tenancy = p);

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
        MMGR_CALL(carcer.wipe, CarcerCfg, .tenancy = scratch, .size = n);

        for (size_t i = 0; i < n; i++)
        {
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, scratch[i], "a byte inside the request survived the wipe");
        }
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, scratch[n], "the wipe cleared past what it was given");
    }
}

void test_a_mark_gives_back_everything_taken_after_it(void)
{
    const size_t before = MMGR_CALL(carcer.interim_mark, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a));

    (void)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    (void)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 128u);
    TEST_ASSERT_TRUE(MMGR_CARCER_POOL(ram, a)->interim_top < before);

    MMGR_CALL(carcer.interim_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .mark = before);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, MMGR_CARCER_POOL(ram, a)->interim_top,
                                     "the top must come back to where it was marked");
}

/**
 * @brief Two savepoints live at once, which is what holding the mark in the caller buys.
 */
void test_marks_nest_because_the_caller_holds_them(void)
{
    const size_t outer = MMGR_CALL(carcer.interim_mark, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a));

    (void)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);

    const size_t inner = MMGR_CALL(carcer.interim_mark, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a));

    (void)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 32u);

    MMGR_CALL(carcer.interim_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .mark = inner);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(inner, MMGR_CARCER_POOL(ram, a)->interim_top, "the inner savepoint comes back");

    MMGR_CALL(carcer.interim_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .mark = outer);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(outer, MMGR_CARCER_POOL(ram, a)->interim_top,
                                     "and the outer one still stands behind it");
}

void test_reset_gives_the_whole_interim_end_back(void)
{
    (void)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 128u);
    MMGR_CALL(carcer.interim_reset, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a));

    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_POOL(ram, a)->size, MMGR_CARCER_POOL(ram, a)->interim_top);
}

void test_owns_tells_a_pool_from_its_neighbour(void)
{
    void *p = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 32u);
    void *q = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, b), .size = 32u);

    TEST_ASSERT_TRUE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = p));
    TEST_ASSERT_TRUE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, b), .at = q));
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = q),
                              "a neighbour's bytes are not ours");
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, b), .at = p),
                              "and ours are not the neighbour's");
}

void test_owns_refuses_the_edges(void)
{
    const uint8_t *const base = MMGR_CARCER_POOL(ram, a)->base;
    const size_t size = MMGR_CARCER_POOL(ram, a)->size;

    TEST_ASSERT_TRUE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = base));
    TEST_ASSERT_TRUE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = base + size - 1u));
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = base + size),
                              "one past the end is outside");
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = base - 1),
                              "one below the base is outside");
    TEST_ASSERT_FALSE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = NULL));
}

void test_the_pools_do_not_share_a_fill_point(void)
{
    (void)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);

    TEST_ASSERT_TRUE(MMGR_CARCER_POOL(ram, a)->persist_end > 0u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CARCER_POOL(ram, b)->persist_end,
                                     "one pool filling must not move the other");
}






void test_a_span_over_pool_bytes_carries_the_pool_address(void)
{
    uint8_t *const p = (uint8_t *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a),
                                            .size = 64u);
    const mmgr_span s = MMGR_CALL(spat.from, SpatiumCfg, .buf = p, .cap = 64u);

    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .at = s.buf),
                             "the pool the span was carved from still owns its bytes");
}