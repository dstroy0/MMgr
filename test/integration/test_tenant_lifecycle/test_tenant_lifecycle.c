/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "locus_carcerum/locus_carcerum.h"
#include "memoria_operor/memoria_operor.h"

<<<<<<< HEAD
#define POOL_BYTES 2048u

Carceribus(ram, MMGR_SECURA(secret, POOL_BYTES), MMGR_SOLUTA(plain, POOL_BYTES));

void setUp(void)
{
    ram_secret_ctx.persist_end = 0;
    ram_plain_ctx.persist_end = 0;
    ram.secret.interim_reset();
    ram.plain.interim_reset();
=======
#define CELLBLOCK_BYTES 2048u

LocusCarcerum(ram, MMGR_MAXIMUM_SECURITY(secret, CELLBLOCK_BYTES), MMGR_MINIMUM_SECURITY(plain, CELLBLOCK_BYTES));

void setUp(void)
{
    ram_secret_ctx.persistent_end = 0;
    ram_plain_ctx.persistent_end = 0;
    ram.secret.temporary_buf_reset();
    ram.plain.temporary_buf_reset();
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
}

void tearDown(void)
{
}

void test_the_two_cellblocks_are_separate(void)
{
<<<<<<< HEAD
    void *s = ram.secret.persist_capio((size_t)32);
    void *p = ram.plain.persist_capio((size_t)32);
=======
    void *s = ram.secret.persistent_buf_alloc((size_t)32);
    void *p = ram.plain.persistent_buf_alloc((size_t)32);
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_NOT_EQUAL(s, p);
<<<<<<< HEAD
    TEST_ASSERT_TRUE_MESSAGE(ram.secret.owns(s), "the secure pool must claim what it handed out");
    TEST_ASSERT_TRUE_MESSAGE(ram.plain.owns(p), "the plaintext pool must claim what it handed out");
    TEST_ASSERT_FALSE_MESSAGE(ram.secret.owns(p), "the secure pool must not claim plaintext bytes");
    TEST_ASSERT_FALSE_MESSAGE(ram.plain.owns(s), "the plaintext pool must not claim secure bytes");
=======
    TEST_ASSERT_TRUE_MESSAGE(ram.secret.who_owns_buf(s), "the secure cellblock must claim what it handed out");
    TEST_ASSERT_TRUE_MESSAGE(ram.plain.who_owns_buf(p), "the plaintext cellblock must claim what it handed out");
    TEST_ASSERT_FALSE_MESSAGE(ram.secret.who_owns_buf(p), "the secure cellblock must not claim plaintext bytes");
    TEST_ASSERT_FALSE_MESSAGE(ram.plain.who_owns_buf(s), "the plaintext cellblock must not claim secure bytes");
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
}

void test_the_allocation_puts_them_back_to_back(void)
{
<<<<<<< HEAD
    TEST_ASSERT_EQUAL_size_t(POOL_BYTES, ram_secret_ctx.size);
    TEST_ASSERT_EQUAL_size_t(POOL_BYTES, ram_plain_ctx.size);

    // Two pools are two objects, so they do not share an address and there is no adjacency to rely
    // on. What matters is that neither one's bytes fall inside the other.
    TEST_ASSERT_FALSE_MESSAGE(ram.secret.owns(ram_plain_ctx.base), "a pool must not hold its neighbour bytes");
    TEST_ASSERT_FALSE_MESSAGE(ram.plain.owns(ram_secret_ctx.base), "in either direction");
=======
    TEST_ASSERT_EQUAL_size_t(CELLBLOCK_BYTES, ram_secret_ctx.size);
    TEST_ASSERT_EQUAL_size_t(CELLBLOCK_BYTES, ram_plain_ctx.size);

    // Two cellblocks are two objects, so they do not share an address and there is no adjacency to
    // rely on. What matters is that neither one's bytes fall inside the other.
    TEST_ASSERT_FALSE_MESSAGE(ram.secret.who_owns_buf(ram_plain_ctx.base),
                              "a cellblock must not hold its neighbor's bytes");
    TEST_ASSERT_FALSE_MESSAGE(ram.plain.who_owns_buf(ram_secret_ctx.base), "in either direction");
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
}

void test_maximum_security_release_zeroes_the_cell(void)
{
<<<<<<< HEAD
    unsigned char *p = (unsigned char *)ram.secret.persist_capio((size_t)32);
=======
    unsigned char *p = (unsigned char *)ram.secret.persistent_buf_alloc((size_t)32);
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);

<<<<<<< HEAD
    ram.secret.persist_reddo(p);
=======
    ram.secret.persistent_buf_release(p);
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, p[i], "released maximum security bytes must be zeroed");
    }
}

void test_minimum_security_release_does_not_zero(void)
{
<<<<<<< HEAD
    unsigned char *p = (unsigned char *)ram.plain.persist_capio((size_t)32);
=======
    unsigned char *p = (unsigned char *)ram.plain.persistent_buf_alloc((size_t)32);
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b

    TEST_ASSERT_NOT_NULL(p);
    MMGR_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);

<<<<<<< HEAD
    ram.plain.persist_reddo(p);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, p[0], "the plaintext custodian does not clear, that is its whole point");
=======
    ram.plain.persistent_buf_release(p);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, p[0],
                                    "the minimum security guard does not clear, that is its whole point");
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);
}

void test_each_cellblock_tracks_its_own_fill(void)
{
<<<<<<< HEAD
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
=======
    // persistent_end is the cellblock's own member. The cellblock is a type the caller declares, so
    // how far a tier has grown is read rather than asked for. The value includes each cell's header,
    // so what is asserted is that it moved, not what it moved to.
    TEST_ASSERT_EQUAL_size_t(0u, ram_secret_ctx.persistent_end);
    TEST_ASSERT_EQUAL_size_t(0u, ram_plain_ctx.persistent_end);

    (void)ram.secret.persistent_buf_alloc((size_t)64);
    TEST_ASSERT_TRUE(ram_secret_ctx.persistent_end >= 64u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_plain_ctx.persistent_end,
                                     "one cellblock filling must not move the other");

    const size_t secret_was = ram_secret_ctx.persistent_end;

    (void)ram.plain.persistent_buf_alloc((size_t)128);
    TEST_ASSERT_EQUAL_size_t(secret_was, ram_secret_ctx.persistent_end);
    TEST_ASSERT_TRUE(ram_plain_ctx.persistent_end >= 128u);
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
}

void test_release_gives_the_bytes_back_to_the_right_cellblock(void)
{
<<<<<<< HEAD
    void *s = ram.secret.persist_capio((size_t)64);
    void *p = ram.plain.persist_capio((size_t)64);
=======
    void *s = ram.secret.persistent_buf_alloc((size_t)64);
    void *p = ram.plain.persistent_buf_alloc((size_t)64);
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);

<<<<<<< HEAD
    const size_t plain_held = ram_plain_ctx.persist_end;

    ram.secret.persist_reddo(s);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_secret_ctx.persist_end, "the only block released, so the end winds back");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(plain_held, ram_plain_ctx.persist_end,
                                     "releasing one tenant must not touch the other");

    ram.plain.persist_reddo(p);
    TEST_ASSERT_EQUAL_size_t(0u, ram_plain_ctx.persist_end);
=======
    const size_t plain_held = ram_plain_ctx.persistent_end;

    ram.secret.persistent_buf_release(s);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ram_secret_ctx.persistent_end,
                                     "the only cell released, so the tier winds back");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(plain_held, ram_plain_ctx.persistent_end,
                                     "releasing one cellblock must not touch the other");

    ram.plain.persistent_buf_release(p);
    TEST_ASSERT_EQUAL_size_t(0u, ram_plain_ctx.persistent_end);
>>>>>>> ff25dbb79dd5d22658a3389362925178e2b55a9b
}
