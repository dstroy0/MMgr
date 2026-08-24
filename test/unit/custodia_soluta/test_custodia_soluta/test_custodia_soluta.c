#include "unity.h"

#include "custodia_soluta/custodia_soluta.h"

#define REGION 1024u

mmgr_carcer_init(ram, REGION, MMGR_POOL(plain, REGION));

void setUp(void)
{
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, plain),
              .size = MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, plain)));
    MMGR_CALL(carcer.interim_reset, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, plain));
}

void tearDown(void)
{
}


void test_soluta_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("custodia_soluta.h compiled with no header before it");
}

void test_soluta_namespace_is_wired(void)
{
    const CustodiaSolutaNs *ns = &soluta;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(CustodiaSolutaNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}


void test_init_hands_back_usable_memory(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(soluta.init, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain), .n = 64u);

    TEST_ASSERT_NOT_NULL(p);
    p[0] = 0x11u;
    p[63] = 0x22u;
    TEST_ASSERT_EQUAL_HEX8(0x11u, p[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, p[63]);
}

void test_init_comes_out_of_the_pool_it_was_given(void)
{
    const void *p = MMGR_CALL(soluta.init, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain), .n = 64u);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = MMGR_CARCER_POOL(ram, plain), .at = p),
                             "the region did not come from the pool");
}

void test_used_grows_with_what_was_taken(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(soluta.used, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain)));

    (void)MMGR_CALL(soluta.init, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain), .n = 64u);
    TEST_ASSERT_EQUAL_size_t(64u, MMGR_CALL(soluta.used, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain)));

    (void)MMGR_CALL(soluta.init, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain), .n = 32u);
    TEST_ASSERT_EQUAL_size_t(96u, MMGR_CALL(soluta.used, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain)));
}

void test_release_gives_the_bytes_back(void)
{
    void *p = MMGR_CALL(soluta.init, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain), .n = 32u);

    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_size_t(32u, MMGR_CALL(soluta.used, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain)));

    MMGR_CALL(soluta.release, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain), .n = 32u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(soluta.used, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain)),
                                     "releasing did not give the bytes back");
}

void test_release_does_not_wipe(void)
{
    uint8_t *p = (uint8_t *)MMGR_CALL(soluta.init, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain), .n = 32u);

    TEST_ASSERT_NOT_NULL(p);
    for (unsigned i = 0; i < 32u; i++)
    {
        p[i] = 0xA5u;
    }

    MMGR_CALL(soluta.release, SolutaCfg, .pool = MMGR_CARCER_POOL(ram, plain), .n = 32u);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xA5u, p[0], "the plaintext custodian does not clear, that is its whole point");
    TEST_ASSERT_EQUAL_HEX8(0xA5u, p[31]);
}
