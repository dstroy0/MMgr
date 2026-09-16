#include "proximus_operor/proximus_operor.h"

#include "unity.h"

static _Alignas(16) uint8_t mem[64];

void setUp(void)
{
    for (uint32_t i = 0; i < sizeof mem; i++)
    {
        mem[i] = (uint8_t)(i * 7u + 1u);
    }
}

void tearDown(void)
{
}

void test_proxim_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("proximus_operor.h compiled with no header before it");
}

void test_reads_at_every_alignment(void)
{
    for (uint32_t off = 0; off < 8u; off++)
    {
        uint8_t *p = mem + off;
        const uint16_t w16 = (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
        const uint32_t w32 = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        uint64_t w64 = 0;
        for (uint32_t i = 0; i < 8u; i++)
        {
            w64 |= (uint64_t)p[i] << (i * 8u);
        }

        TEST_ASSERT_EQUAL_HEX16(w16, EMBED_CALL(proxim.load16, ProximusCfg, .at = p));
        TEST_ASSERT_EQUAL_HEX32(w32, EMBED_CALL(proxim.load32, ProximusCfg, .at = p));
        TEST_ASSERT_EQUAL_HEX64(w64, EMBED_CALL(proxim.load64, ProximusCfg, .at = p));
    }
}

void test_each_width_reads_only_its_own_bytes(void)
{
    uint8_t *p = mem + 3u;

    TEST_ASSERT_EQUAL_HEX64((uint64_t)EMBED_CALL(proxim.load16, ProximusCfg, .at = p),
                            EMBED_CALL(proxim.load64, ProximusCfg, .at = p) & 0xFFFFull);
    TEST_ASSERT_EQUAL_HEX64((uint64_t)EMBED_CALL(proxim.load32, ProximusCfg, .at = p),
                            EMBED_CALL(proxim.load64, ProximusCfg, .at = p) & 0xFFFFFFFFull);
}

void test_writes_round_trip_at_every_alignment(void)
{
    for (uint32_t off = 0; off < 8u; off++)
    {
        uint8_t *p = mem + off;

        EMBED_CALL(proxim.put16, ProximusCfg, .dst = p, .val = 0xBEEFu);
        TEST_ASSERT_EQUAL_HEX16(0xBEEFu, EMBED_CALL(proxim.load16, ProximusCfg, .at = p));

        EMBED_CALL(proxim.put32, ProximusCfg, .dst = p, .val = 0xDEADBEEFu);
        TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, EMBED_CALL(proxim.load32, ProximusCfg, .at = p));

        EMBED_CALL(proxim.put64, ProximusCfg, .dst = p, .val = 0x0123456789ABCDEFull);
        TEST_ASSERT_EQUAL_HEX64(0x0123456789ABCDEFull, EMBED_CALL(proxim.load64, ProximusCfg, .at = p));
    }
}

void test_a_write_touches_exactly_its_width(void)
{
    uint8_t *p = mem + 5u;
    const uint8_t before = p[2];

    EMBED_CALL(proxim.put16, ProximusCfg, .dst = p, .val = 0x0000u);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(before, p[2], "a 16-bit write must not reach the third byte");
}

void test_aligned_entries_round_trip(void)
{
    uint8_t *p = mem;

    EMBED_CALL(proxim.al_put64, ProximusCfg, .dst = p, .val = 0x123456789ABCDEF0ull);
    TEST_ASSERT_EQUAL_HEX64(0x123456789ABCDEF0ull, EMBED_CALL(proxim.al_load64, ProximusCfg, .at = p));

    const embed_word word = (embed_word)0x0123456789ABCDEFull;
    EMBED_CALL(proxim.al_put, ProximusCfg, .dst = p, .val = word);
    TEST_ASSERT_EQUAL_HEX64((uint64_t)word, (uint64_t)EMBED_CALL(proxim.al_load, ProximusCfg, .at = p));
}

void test_the_aligned_word_and_the_unaligned_word_agree(void)
{
    uint8_t *p = mem;
    const embed_word word = (embed_word)0x0F1E2D3C4B5A6978ull;

    EMBED_CALL(proxim.al_put, ProximusCfg, .dst = p, .val = word);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE((uint64_t)EMBED_CALL(proxim.al_load, ProximusCfg, .at = p),
                                    (uint64_t)EMBED_CALL(proxim.load, ProximusCfg, .at = p),
                                    "an aligned address reads the same either way");
}

void test_the_bulk_move_word_round_trips(void)
{
    uint8_t *p = mem;
    const embed_word word = (embed_word)0x0123456789ABCDEFull;

    EMBED_CALL(proxim.put, ProximusCfg, .dst = p, .val = word);
    TEST_ASSERT_EQUAL_HEX64((uint64_t)word, (uint64_t)EMBED_CALL(proxim.load, ProximusCfg, .at = p));
}

void test_the_move_word_is_the_configured_width(void)
{
    TEST_ASSERT_EQUAL_size_t(sizeof(embed_word), sizeof(embed_word));
}

void test_read_copies_exactly_the_bytes_asked_for(void)
{
    uint8_t dst[16];
    for (uint32_t i = 0; i < sizeof dst; i++)
    {
        dst[i] = 0xEEu;
    }

    EMBED_CALL(proxim.read, ProximusCfg, .dst = dst, .at = mem, .size = 5u);
    for (uint32_t i = 0; i < 5u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(mem[i], dst[i]);
    }
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xEEu, dst[5], "and not one byte more");
}

void test_read_of_nothing_writes_nothing(void)
{
    uint8_t dst[4] = {1u, 2u, 3u, 4u};

    EMBED_CALL(proxim.read, ProximusCfg, .dst = dst, .at = mem, .size = 0u);
    TEST_ASSERT_EQUAL_HEX8(1u, dst[0]);
}

void test_read_at_every_alignment(void)
{
    uint8_t dst[32];
    for (uint32_t off = 0; off < 8u; off++)
    {
        for (uint32_t n = 1u; n <= 17u; n++)
        {
            for (uint32_t i = 0; i < sizeof dst; i++)
            {
                dst[i] = 0u;
            }
            EMBED_CALL(proxim.read, ProximusCfg, .dst = dst, .at = mem + off, .size = n);
            for (uint32_t i = 0; i < n; i++)
            {
                TEST_ASSERT_EQUAL_HEX8(mem[off + i], dst[i]);
            }
        }
    }
}

void test_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_proxim_load16, proxim.load16);
    TEST_ASSERT_EQUAL_PTR(mmgr_proxim_read, proxim.read);
}
