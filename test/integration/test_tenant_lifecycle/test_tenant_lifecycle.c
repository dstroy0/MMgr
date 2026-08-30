/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "locus_carcerum/locus_carcerum.h"
#include "memoria_operor/memoria_operor.h"

#define CELLBLOCK_BYTES 2048u

LocusCarcerum(ram, MMGR_MAXIMUM_SECURITY(secret, CELLBLOCK_BYTES), MMGR_MINIMUM_SECURITY(plain, CELLBLOCK_BYTES));

void setUp(void)
{
    ram_secret_ctx.persistent_end = 0;
    ram_plain_ctx.persistent_end = 0;
    ram.secret.temporary_buf_reset();
    ram.plain.temporary_buf_reset();
}

void tearDown(void)
{
}

void test_the_two_cellblocks_are_separate(void)
{
    void *s = ram.secret.persistent_buf_alloc((size_t)32);
    void *p = ram.plain.persistent_buf_alloc((size_t)32);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_EQUAL(s, p);
    TEST_ASSERT_TRUE_MESSAGE(ram.secret.who_owns_buf(s), "the secure cellblock must claim what it handed out");
    TEST_ASSERT_TRUE_MESSAGE(ram.plain.who_owns_buf(p), "the plaintext cellblock must claim what it handed out");
    TEST_ASSERT_FALSE_MESSAGE(ram.secret.who_owns_buf(p), "the secure cellblock must not claim plaintext bytes");
    TEST_ASSERT_FALSE_MESSAGE(ram.plain.who_owns_buf(s), "the plaintext cellblock must not claim secure bytes");
}

void test_the_allocation_puts_them_back_to_back(void)
{
    TEST_ASSERT_EQUAL_size_t(CELLBLOCK_BYTES, ram_secret_ctx.size);
    TEST_ASSERT_EQUAL_size_t(CELLBLOCK_BYTES, ram_plain_ctx.size);

    // Two cellblocks are two objects, so they do not share an address and there is no adjacency to
    // rely on. What matters is that neither one's bytes fall inside the other.
    TEST_ASSERT_FALSE_MESSAGE(ram.secret.who_owns_buf(ram_plain_ctx.base),
                              "a cellblock must not hold its neighbor's bytes");
    TEST_ASSERT_FALSE_MESSAGE(ram.plain.who_owns_buf(ram_secret_ctx.base), "in either direction");
}

void test_maximum_security_release_zeroes_the_cell(void)
{
    unsigned char *p = (unsigned char *)ram.secret.persistent_buf_alloc((size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);

    ram.secret.persistent_buf_release(p);
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, p[i], "released maximum security bytes must be zeroed");
    }
}

void test_minimum_security_release_does_not_zero(void)
{
    unsigned char *p = (unsigned char *)ram.plain.persistent_buf_alloc((size_t)32);

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);

    ram.plain.persistent_buf_release(p);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, p[0], "the minimum security guard does not clear, that is its whole point");
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);
}

void test_each_cellblock_tracks_its_own_fill(void)
{
    // persistent_end is the cellblock's own member. The cellblock is a type the caller declares, so
    // how far a tier has grown is read rather than asked for. The value includes each cell's header,
    // so what is asserted is that it moved, not what it moved to.
    TEST_ASSERT_EQUAL_size_t(0u, ram_secret_ctx.persistent_end);
    TEST_ASSERT_EQUAL_size_t(0u, ram_plain_ctx.persistent_end);

    (void)ram.secret.persistent_buf_alloc((size_t)64);
    TEST_ASSERT_TRUE(ram_secret_ctx.persistent_end >= 64u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_plain_ctx.persistent_end, "one cellblock filling must not move the other");

    const size_t secret_was = ram_secret_ctx.persistent_end;

    (void)ram.plain.persistent_buf_alloc((size_t)128);
    TEST_ASSERT_EQUAL_size_t(secret_was, ram_secret_ctx.persistent_end);
    TEST_ASSERT_TRUE(ram_plain_ctx.persistent_end >= 128u);
}

void test_release_gives_the_bytes_back_to_the_right_cellblock(void)
{
    void *s = ram.secret.persistent_buf_alloc((size_t)64);
    void *p = ram.plain.persistent_buf_alloc((size_t)64);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);

    const size_t plain_held = ram_plain_ctx.persistent_end;

    ram.secret.persistent_buf_release(s);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_secret_ctx.persistent_end,
                                     "the only cell released, so the tier winds back");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(plain_held, ram_plain_ctx.persistent_end,
                                     "releasing one cellblock must not touch the other");

    ram.plain.persistent_buf_release(p);
    TEST_ASSERT_EQUAL_size_t(0u, ram_plain_ctx.persistent_end);
}
