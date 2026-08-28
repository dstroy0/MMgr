/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "carceribus/carceribus.h"
#include "memoria_operor/memoria_operor.h"

#define POOL_BYTES 2048u

Carceribus(ram, MMGR_SECURA(secret, POOL_BYTES), MMGR_SOLUTA(plain, POOL_BYTES));

void setUp(void)
{
    ram_secret_ctx.persist_end = 0;
    ram_plain_ctx.persist_end = 0;
    ram.secret.interim_reset();
    ram.plain.interim_reset();
}

void tearDown(void)
{
}

void test_the_two_pools_are_separate_tenants(void)
{
    void *s = ram.secret.persist_capio((size_t)32);
    void *p = ram.plain.persist_capio((size_t)32);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_EQUAL(s, p);
    TEST_ASSERT_TRUE_MESSAGE(ram.secret.owns(s), "the secure pool must claim what it handed out");
    TEST_ASSERT_TRUE_MESSAGE(ram.plain.owns(p), "the plaintext pool must claim what it handed out");
    TEST_ASSERT_FALSE_MESSAGE(ram.secret.owns(p), "the secure pool must not claim plaintext bytes");
    TEST_ASSERT_FALSE_MESSAGE(ram.plain.owns(s), "the plaintext pool must not claim secure bytes");
}

void test_the_carve_puts_them_back_to_back(void)
{
    TEST_ASSERT_EQUAL_size_t(POOL_BYTES, ram_secret_ctx.size);
    TEST_ASSERT_EQUAL_size_t(POOL_BYTES, ram_plain_ctx.size);

    // Two pools are two objects, so they do not share an address and there is no adjacency to rely
    // on. What matters is that neither one's bytes fall inside the other.
    TEST_ASSERT_FALSE_MESSAGE(ram.secret.owns(ram_plain_ctx.base), "a pool must not hold its neighbour bytes");
    TEST_ASSERT_FALSE_MESSAGE(ram.plain.owns(ram_secret_ctx.base), "in either direction");
}

void test_secure_release_wipes_what_it_gives_back(void)
{
    unsigned char *p = (unsigned char *)ram.secret.persist_capio((size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);

    ram.secret.persist_reddo(p);
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, p[i], "released secure bytes must be wiped");
    }
}

void test_plaintext_release_does_not_wipe(void)
{
    unsigned char *p = (unsigned char *)ram.plain.persist_capio((size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);

    ram.plain.persist_reddo(p);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, p[0], "the plaintext custodian does not clear, that is its whole point");
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);
}

void test_each_tenant_tracks_its_own_fill(void)
{
    // persist_end is the pool's own member: the pool is a type the caller declares, so how far an
    // end has grown is read rather than asked for. The figure includes each block's header, so what
    // is asserted is that it moved, not what it moved to.
    TEST_ASSERT_EQUAL_size_t(0u, ram_secret_ctx.persist_end);
    TEST_ASSERT_EQUAL_size_t(0u, ram_plain_ctx.persist_end);

    (void)ram.secret.persist_capio((size_t)64);
    TEST_ASSERT_TRUE(ram_secret_ctx.persist_end >= 64u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_plain_ctx.persist_end, "one tenant filling must not move the other");

    const size_t secret_was = ram_secret_ctx.persist_end;

    (void)ram.plain.persist_capio((size_t)128);
    TEST_ASSERT_EQUAL_size_t(secret_was, ram_secret_ctx.persist_end);
    TEST_ASSERT_TRUE(ram_plain_ctx.persist_end >= 128u);
}

void test_release_gives_the_bytes_back_to_the_right_tenant(void)
{
    void *s = ram.secret.persist_capio((size_t)64);
    void *p = ram.plain.persist_capio((size_t)64);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);

    const size_t plain_held = ram_plain_ctx.persist_end;

    ram.secret.persist_reddo(s);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_secret_ctx.persist_end, "the only block released, so the end winds back");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(plain_held, ram_plain_ctx.persist_end,
                                     "releasing one tenant must not touch the other");

    ram.plain.persist_reddo(p);
    TEST_ASSERT_EQUAL_size_t(0u, ram_plain_ctx.persist_end);
}
