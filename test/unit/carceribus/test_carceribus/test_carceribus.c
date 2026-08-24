#include "unity.h"

#include "carceribus/carceribus.h"

#define A_BYTES 1024u
#define B_BYTES 2048u

mmgr_carcer_init(ram, A_BYTES + B_BYTES, MMGR_POOL(a, A_BYTES), MMGR_POOL(b, B_BYTES));

void setUp(void)
{
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a),
              .size = MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, b),
              .size = MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, b)));
    MMGR_CALL(carcer.interim_reset, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a));
    MMGR_CALL(carcer.interim_reset, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, b));
}

void tearDown(void)
{
}


void test_carceribus_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("carceribus.h compiled with no header before it");
}

void test_carcer_namespace_is_wired(void)
{
    const CarceribusNs *ns = &carcer;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(CarceribusNs), sizeof(*ns), "the namespace instance is not its own type");
}


void test_the_machinery_sits_below_the_arena(void)
{
    TEST_ASSERT_TRUE_MESSAGE((uintptr_t)ram.bytes > (uintptr_t)&ram, "the arena must start above the machinery");
    TEST_ASSERT_EQUAL_size_t(sizeof(CarcerCtx) * MMGR_CARCER_MAX_REGIONS, sizeof ram.pool);
    TEST_ASSERT_EQUAL_size_t(sizeof(size_t) * MMGR_CARCER_MAX_REGIONS, sizeof ram.mark);
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
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));
    TEST_ASSERT_EQUAL_size_t(A_BYTES, MMGR_CALL(carcer.octas_praesto, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));
    TEST_ASSERT_EQUAL_size_t(A_BYTES, MMGR_CALL(carcer.interim_mark, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));
}


void test_persist_hands_out_the_bottom_and_walks_up(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    uint8_t *q = (uint8_t *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);

    TEST_ASSERT_EQUAL_PTR(MMGR_CARCER_POOL(ram, a)->base, p);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(p + 64u, q, "the next block follows the first");
    TEST_ASSERT_EQUAL_size_t(128u, MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));
}

void test_persist_reddo_winds_the_fill_point_back(void)
{
    (void)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    TEST_ASSERT_EQUAL_size_t(64u, MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));

    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)),
                                     "persist tears down");
}

void test_interim_hands_out_the_top_and_walks_down(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    uint8_t *q = (uint8_t *)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);

    TEST_ASSERT_EQUAL_PTR(MMGR_CARCER_POOL(ram, a)->base + A_BYTES - 64u, p);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(p - 64u, q, "the next block sits below the first");
}

void test_the_two_arms_grow_toward_each_other(void)
{
    const size_t room = MMGR_CALL(carcer.octas_praesto, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a));

    (void)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    (void)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 64u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(room - 128u,
                                     MMGR_CALL(carcer.octas_praesto, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)),
                                     "both ends take from the same gap");
}


void test_reset_puts_the_whole_interim_arm_back(void)
{
    (void)MMGR_CALL(carcer.interim_capio, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a), .size = 128u);
    MMGR_CALL(carcer.interim_reset, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a));
    TEST_ASSERT_EQUAL_size_t(MMGR_CARCER_POOL(ram, a)->size,
                             MMGR_CALL(carcer.interim_mark, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));
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

    TEST_ASSERT_EQUAL_size_t(64u, MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, a)));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, b)),
                                     "one pool filling must not move the other");
}
