#include "unity.h"

#include "custodia_secura/custodia_secura.h"
#include "custodia_soluta/custodia_soluta.h"
#include "memoria_operor/memoria_operor.h"

#define SECRET_BYTES 1024u
#define PLAIN_BYTES 4096u

mmgr_carcer_init(g_ram, SECRET_BYTES + PLAIN_BYTES, MMGR_POOL(g_secret, SECRET_BYTES),
                 MMGR_POOL(g_plain, PLAIN_BYTES));

static CarcerCfg *secret(void)
{
    return &g_ram.pool[g_secret];
}

static CarcerCfg *plain(void)
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
    void *s = mmgr_secura_init(secret(), (size_t)32);
    void *p = mmgr_soluta_init(plain(), (size_t)32);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_EQUAL(s, p);
    TEST_ASSERT_TRUE_MESSAGE(mmgr_carcer_owns(secret(), s), "the secure pool must claim what it handed out");
    TEST_ASSERT_TRUE_MESSAGE(mmgr_carcer_owns(plain(), p), "the plaintext pool must claim what it handed out");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_carcer_owns(secret(), p), "the secure pool must not claim plaintext bytes");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_carcer_owns(plain(), s), "the plaintext pool must not claim secure bytes");
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
    unsigned char *p = (unsigned char *)mmgr_secura_init(secret(), (size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    mmgr_memor_set(p, (uint8_t)0xA5u, (size_t)32u);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);

    mmgr_secura_reddo(secret(), p, (size_t)32);
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, p[i], "released secure bytes must be wiped");
    }
}

void test_plaintext_release_does_not_wipe(void)
{
    unsigned char *p = (unsigned char *)mmgr_soluta_init(plain(), (size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    mmgr_memor_set(p, (uint8_t)0xA5u, (size_t)32u);

    mmgr_soluta_reddo(plain(), (size_t)32);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, p[0], "the plaintext custodian does not clear, that is its whole point");
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);
}

void test_each_tenant_tracks_its_own_fill(void)
{
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_secura_used(secret()));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_soluta_used(plain()));

    (void)mmgr_secura_init(secret(), (size_t)64);
    TEST_ASSERT_EQUAL_size_t(64u, mmgr_secura_used(secret()));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_soluta_used(plain()), "one tenant filling must not move the other");

    (void)mmgr_soluta_init(plain(), (size_t)128);
    TEST_ASSERT_EQUAL_size_t(64u, mmgr_secura_used(secret()));
    TEST_ASSERT_EQUAL_size_t(128u, mmgr_soluta_used(plain()));
}

void test_release_gives_the_bytes_back_to_the_right_tenant(void)
{
    void *s = mmgr_secura_init(secret(), (size_t)64);
    void *p = mmgr_soluta_init(plain(), (size_t)64);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);

    mmgr_secura_reddo(secret(), s, (size_t)64);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_secura_used(secret()));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(64u, mmgr_soluta_used(plain()), "releasing one tenant must not touch the other");

    mmgr_soluta_reddo(plain(), (size_t)64);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_soluta_used(plain()));
}
