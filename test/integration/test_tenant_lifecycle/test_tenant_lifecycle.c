#include "unity.h"

#include "custodia_secura/custodia_secura.h"
#include "custodia_soluta/custodia_soluta.h"
#include "memoria_operor/memoria_operor.h"

#define SECRET_BYTES 1024u
#define PLAIN_BYTES 4096u

mmgr_carcer_init(g_ram, SECRET_BYTES + PLAIN_BYTES, MMGR_POOL(g_secret, SECRET_BYTES),
                 MMGR_POOL(g_plain, PLAIN_BYTES));

static CarcerCtx *secret(void)
{
    return &g_ram.pool[g_secret];
}

static CarcerCtx *plain(void)
{
    return &g_ram.pool[g_plain];
}

void setUp(void)
{
    secret()->persist_end = 0;
    secret()->interim_top = secret()->size;
    plain()->persist_end = 0;
    plain()->interim_top = plain()->size;
}

void tearDown(void)
{
}

void test_the_two_pools_are_separate_tenants(void)
{
    void *s = MMGR_CALL(secura.init, SecuraCfg, .pool = secret(), .bytes = (size_t)32);
    void *p = MMGR_CALL(soluta.init, SolutaCfg, .pool = plain(), .bytes = (size_t)32);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_EQUAL(s, p);
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = secret(), .at = s), "the secure pool must claim what it handed out");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = plain(), .at = p), "the plaintext pool must claim what it handed out");
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = secret(), .at = p), "the secure pool must not claim plaintext bytes");
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(carcer.owns, CarcerCfg, .pool = plain(), .at = s), "the plaintext pool must not claim secure bytes");
}

void test_the_carve_puts_them_back_to_back(void)
{
    TEST_ASSERT_EQUAL_size_t(SECRET_BYTES, secret()->size);
    TEST_ASSERT_EQUAL_size_t(PLAIN_BYTES, plain()->size);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(secret()->base + SECRET_BYTES, plain()->base,
                                  "the second pool starts where the first ends");
}

void test_secure_release_wipes_what_it_gives_back(void)
{
    unsigned char *p = (unsigned char *)MMGR_CALL(secura.init, SecuraCfg, .pool = secret(), .bytes = (size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);

    MMGR_CALL(secura.release, SecuraCfg, .pool = secret(), .at = p, .bytes = (size_t)32);
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, p[i], "released secure bytes must be wiped");
    }
}

void test_plaintext_release_does_not_wipe(void)
{
    unsigned char *p = (unsigned char *)MMGR_CALL(soluta.init, SolutaCfg, .pool = plain(), .bytes = (size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);

    MMGR_CALL(soluta.release, SolutaCfg, .pool = plain(), .bytes = (size_t)32);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, p[0], "the plaintext custodian does not clear, that is its whole point");
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);
}

void test_each_tenant_tracks_its_own_fill(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(secura.used, SecuraCfg, .pool = secret()));
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(soluta.used, SolutaCfg, .pool = plain()));

    (void)MMGR_CALL(secura.init, SecuraCfg, .pool = secret(), .bytes = (size_t)64);
    TEST_ASSERT_EQUAL_size_t(64u, MMGR_CALL(secura.used, SecuraCfg, .pool = secret()));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(soluta.used, SolutaCfg, .pool = plain()), "one tenant filling must not move the other");

    (void)MMGR_CALL(soluta.init, SolutaCfg, .pool = plain(), .bytes = (size_t)128);
    TEST_ASSERT_EQUAL_size_t(64u, MMGR_CALL(secura.used, SecuraCfg, .pool = secret()));
    TEST_ASSERT_EQUAL_size_t(128u, MMGR_CALL(soluta.used, SolutaCfg, .pool = plain()));
}

void test_release_gives_the_bytes_back_to_the_right_tenant(void)
{
    void *s = MMGR_CALL(secura.init, SecuraCfg, .pool = secret(), .bytes = (size_t)64);
    void *p = MMGR_CALL(soluta.init, SolutaCfg, .pool = plain(), .bytes = (size_t)64);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);

    MMGR_CALL(secura.release, SecuraCfg, .pool = secret(), .at = s, .bytes = (size_t)64);
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(secura.used, SecuraCfg, .pool = secret()));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(64u, MMGR_CALL(soluta.used, SolutaCfg, .pool = plain()), "releasing one tenant must not touch the other");

    MMGR_CALL(soluta.release, SolutaCfg, .pool = plain(), .bytes = (size_t)64);
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(soluta.used, SolutaCfg, .pool = plain()));
}
