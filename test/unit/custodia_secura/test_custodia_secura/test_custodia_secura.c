#include "unity.h"

#include "custodia_secura/custodia_secura.h"

#define REGION 1024u

mmgr_carcer_init(ram, REGION, MMGR_POOL(secret, REGION));

void setUp(void)
{
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, secret),
              .size = MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, secret)));
    MMGR_CALL(carcer.interim_reset, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, secret));
}

void tearDown(void)
{
}


void test_secura_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("custodia_secura.h compiled with no header before it");
}

void test_secura_namespace_is_wired(void)
{
    const CustodiaSecuraNs *ns = &secura;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(CustodiaSecuraNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}


void test_init_hands_back_usable_memory(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(secura.init, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .n = 64u);

    TEST_ASSERT_NOT_NULL(p);
    p[0] = 0x11u;
    p[63] = 0x22u;
    TEST_ASSERT_EQUAL_HEX8(0x11u, p[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, p[63]);
}

void test_init_comes_out_of_the_pool_it_was_given(void)
{
    const void *p = MMGR_CALL(secura.init, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .n = 64u);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, secret), .at = p),
                             "the region did not come from the pool");
}

void test_used_grows_with_what_was_taken(void)
{
    const size_t before = MMGR_CALL(secura.used, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret));

    (void)MMGR_CALL(secura.init, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .n = 64u);
    TEST_ASSERT_GREATER_OR_EQUAL_size_t_MESSAGE(before + 64u,
                                                MMGR_CALL(secura.used, SecuraCfg,
                                                          .pool = MMGR_CARCER_POOL(ram, secret)),
                                                "the region did not move the fill point");
}


void test_wipe_clears_the_region(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(secura.init, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .n = 64u);

    TEST_ASSERT_NOT_NULL(p);
    for (unsigned i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }

    MMGR_CALL(secura.wipe, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .at = p, .n = 64u);
    for (unsigned i = 0; i < 64u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0u, p[i]);
    }
}

void test_wipe_stays_inside_what_it_was_asked_for(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(secura.init, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .n = 64u);

    TEST_ASSERT_NOT_NULL(p);
    for (unsigned i = 0; i < 64u; i++)
    {
        p[i] = 0xA5u;
    }

    MMGR_CALL(secura.wipe, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .at = p, .n = 32u);
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0u, p[i]);
    }
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, p[32], "the bytes past the run are untouched");
    TEST_ASSERT_EQUAL_HEX8(0xA5u, p[63]);
}

void test_wipe_of_nothing_touches_nothing(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(secura.init, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .n = 64u);

    TEST_ASSERT_NOT_NULL(p);
    p[0] = 0x5Au;
    MMGR_CALL(secura.wipe, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .at = p, .n = 0u);
    TEST_ASSERT_EQUAL_HEX8(0x5Au, p[0]);
}


void test_release_wipes_what_it_gives_up(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(secura.init, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .n = 32u);

    TEST_ASSERT_NOT_NULL(p);
    for (unsigned i = 0; i < 32u; i++)
    {
        p[i] = 0xC3u;
    }

    MMGR_CALL(secura.release, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .at = p, .n = 32u);
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, p[i], "a released byte kept its value");
    }
}

void test_release_gives_the_bytes_back(void)
{
    const size_t before = MMGR_CALL(secura.used, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret));
    uint8_t *p = (uint8_t *)MMGR_CALL(secura.init, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .n = 32u);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_GREATER_THAN_size_t(before, MMGR_CALL(secura.used, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret)));

    MMGR_CALL(secura.release, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret), .at = p, .n = 32u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, MMGR_CALL(secura.used, SecuraCfg, .pool = MMGR_CARCER_POOL(ram, secret)),
                                     "releasing did not give the bytes back");
}
