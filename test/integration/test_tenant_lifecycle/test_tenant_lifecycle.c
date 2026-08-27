/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "carceribus/carceribus.h"
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
    void *s = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = secret(), .size = (size_t)32);
    void *p = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = plain(), .size = (size_t)32);

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
    unsigned char *p = (unsigned char *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = secret(), .size = (size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);

    MMGR_CALL(carcer.secura_reddo, CarcerCfg, .pool = secret(), .tenancy = p, .size = (size_t)32);
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, p[i], "released secure bytes must be wiped");
    }
}

void test_plaintext_release_does_not_wipe(void)
{
    unsigned char *p = (unsigned char *)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = plain(), .size = (size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);

    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = plain(), .size = (size_t)32);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, p[0], "the plaintext custodian does not clear, that is its whole point");
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);
}

void test_each_tenant_tracks_its_own_fill(void)
{
    // persist_end is the pool's own member: the pool is a type the caller declares, so how far an
    // end has grown is read rather than asked for. The figure includes each block's header, so what
    // is asserted is that it moved, not what it moved to.
    TEST_ASSERT_EQUAL_size_t(0u, secret()->persist_end);
    TEST_ASSERT_EQUAL_size_t(0u, plain()->persist_end);

    (void)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = secret(), .size = (size_t)64);
    TEST_ASSERT_TRUE(secret()->persist_end >= 64u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, plain()->persist_end, "one tenant filling must not move the other");

    const size_t secret_was = secret()->persist_end;

    (void)MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = plain(), .size = (size_t)128);
    TEST_ASSERT_EQUAL_size_t(secret_was, secret()->persist_end);
    TEST_ASSERT_TRUE(plain()->persist_end >= 128u);
}

void test_release_gives_the_bytes_back_to_the_right_tenant(void)
{
    void *s = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = secret(), .size = (size_t)64);
    void *p = MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = plain(), .size = (size_t)64);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);

    const size_t plain_held = plain()->persist_end;

    MMGR_CALL(carcer.secura_reddo, CarcerCfg, .pool = secret(), .tenancy = s);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, secret()->persist_end, "the only block released, so the end winds back");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(plain_held, plain()->persist_end,
                                     "releasing one tenant must not touch the other");

    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = plain(), .tenancy = p);
    TEST_ASSERT_EQUAL_size_t(0u, plain()->persist_end);
}
