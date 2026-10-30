#include "unity.h"

#include "locus_carcerum/locus_carcerum.h"
#include "memoria_operor/memoria_operor.h"

#define CELLBLOCK_BYTES 2048u

ParsMemoriaeInternae(secret, CELLBLOCK_BYTES);
ParsMemoriaeInternae(plain, CELLBLOCK_BYTES);

LocusCarcerum(ram, MMGR_MAXIMUM_SECURITY(secret), MMGR_MINIMUM_SECURITY(plain));

static CellOwner cells = MMGR_CELL_OWNER("cells");

void setUp(void)
{
    if (embed_atomic_usize_load(&cells.place) == 0u)
    {
        TEST_ASSERT_TRUE(mmgr_owner_register(&cells));
    }
    mmgr_cellblock_emptied(&ram_secret_ctx);
    mmgr_cellblock_emptied(&ram_plain_ctx);
    ram.secret.temporary_buf_reset();
    ram.plain.temporary_buf_reset();
}

void tearDown(void)
{
}

void test_the_two_cellblocks_are_separate(void)
{
    void *s = ram.secret.persistent_buf_alloc((size_t)32, &cells);
    void *p = ram.plain.persistent_buf_alloc((size_t)32, &cells);

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

    TEST_ASSERT_FALSE_MESSAGE(ram.secret.who_owns_buf(ram_plain_ctx.base),
                              "a cellblock must not hold its neighbor's bytes");
    TEST_ASSERT_FALSE_MESSAGE(ram.plain.who_owns_buf(ram_secret_ctx.base), "in either direction");
}

void test_maximum_security_release_zeroes_the_cell(void)
{
    unsigned char *p = (unsigned char *)ram.secret.persistent_buf_alloc((size_t)32, &cells);

    TEST_ASSERT_NOT_NULL(p);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);

    ram.secret.persistent_buf_release(p, &cells);
    for (unsigned i = 0; i < 32u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, p[i], "released maximum security bytes must be zeroed");
    }
}

void test_minimum_security_release_does_not_zero(void)
{
    unsigned char *p = (unsigned char *)ram.plain.persistent_buf_alloc((size_t)32, &cells);

    TEST_ASSERT_NOT_NULL(p);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = p, .val = (uint8_t)0xA5u, .bytes = (size_t)32u);

    ram.plain.persistent_buf_release(p, &cells);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5u, p[0], "the minimum security guard does not clear, that is its whole point");
    TEST_ASSERT_EQUAL_UINT8(0xA5u, p[31]);
}

void test_each_cellblock_tracks_its_own_fill(void)
{

    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellblock_persistent_end(&ram_secret_ctx));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellblock_persistent_end(&ram_plain_ctx));

    (void)ram.secret.persistent_buf_alloc((size_t)64, &cells);
    TEST_ASSERT_TRUE(mmgr_cellblock_persistent_end(&ram_secret_ctx) >= 64u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_cellblock_persistent_end(&ram_plain_ctx), "one cellblock filling must not move the other");

    const size_t secret_was = mmgr_cellblock_persistent_end(&ram_secret_ctx);

    (void)ram.plain.persistent_buf_alloc((size_t)128, &cells);
    TEST_ASSERT_EQUAL_size_t(secret_was, mmgr_cellblock_persistent_end(&ram_secret_ctx));
    TEST_ASSERT_TRUE(mmgr_cellblock_persistent_end(&ram_plain_ctx) >= 128u);
}

void test_release_gives_the_bytes_back_to_the_right_cellblock(void)
{
    void *s = ram.secret.persistent_buf_alloc((size_t)64, &cells);
    void *p = ram.plain.persistent_buf_alloc((size_t)64, &cells);

    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NOT_NULL(p);

    const size_t plain_held = mmgr_cellblock_persistent_end(&ram_plain_ctx);

    ram.secret.persistent_buf_release(s, &cells);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_cellblock_persistent_end(&ram_secret_ctx),
                                     "the only cell released, so the tier winds back");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(plain_held, mmgr_cellblock_persistent_end(&ram_plain_ctx),
                                     "releasing one cellblock must not touch the other");

    ram.plain.persistent_buf_release(p, &cells);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_cellblock_persistent_end(&ram_plain_ctx));
}
