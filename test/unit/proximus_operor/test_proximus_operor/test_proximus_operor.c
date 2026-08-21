// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "proximus_operor/proximus_operor.h"

// deliberately oversized and offset, so every case can pick its own alignment
static _Alignas(16) uint8_t mem[64];

void setUp(void)
{
    for (unsigned i = 0; i < sizeof mem; i++)
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
    // the whole point of the proxim entries: alignment is not a precondition
    for (unsigned off = 0; off < 8u; off++)
    {
        uint8_t *p = mem + off;
        const uint16_t w16 = (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
        const uint32_t w32 = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        uint64_t w64 = 0;
        for (unsigned i = 0; i < 8u; i++)
        {
            w64 |= (uint64_t)p[i] << (i * 8u);
        }

        TEST_ASSERT_EQUAL_HEX16(w16, proxim.u16(p));
        TEST_ASSERT_EQUAL_HEX32(w32, proxim.u32(p));
        TEST_ASSERT_EQUAL_HEX64(w64, proxim.u64(p));
    }
}

void test_load_selects_by_width(void)
{
    uint8_t *p = mem + 3u; // deliberately unaligned
    TEST_ASSERT_EQUAL_HEX64((uint64_t)p[0], proxim.load(p, 1u));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)proxim.u16(p), proxim.load(p, 2u));
    TEST_ASSERT_EQUAL_HEX64((uint64_t)proxim.u32(p), proxim.load(p, 4u));
    TEST_ASSERT_EQUAL_HEX64(proxim.u64(p), proxim.load(p, 8u));
}

void test_load_of_an_unsupported_width_reads_nothing(void)
{
    // defensive: the library only ever asks for 1, 2, 4 or 8
    TEST_ASSERT_EQUAL_HEX64(0u, proxim.load(mem, 3u));
    TEST_ASSERT_EQUAL_HEX64(0u, proxim.load(mem, 0u));
    TEST_ASSERT_EQUAL_HEX64(0u, proxim.load(mem, 16u));
}

void test_writes_round_trip_at_every_alignment(void)
{
    for (unsigned off = 0; off < 8u; off++)
    {
        uint8_t *p = mem + off;

        proxim.put_u16(p, 0xBEEFu);
        TEST_ASSERT_EQUAL_HEX16(0xBEEFu, proxim.u16(p));

        proxim.put_u32(p, 0xDEADBEEFu);
        TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, proxim.u32(p));

        proxim.put_u64(p, 0x0123456789ABCDEFull);
        TEST_ASSERT_EQUAL_HEX64(0x0123456789ABCDEFull, proxim.u64(p));
    }
}

void test_a_write_touches_exactly_its_width(void)
{
    uint8_t *p = mem + 5u;
    const uint8_t before = p[2];
    proxim.put_u16(p, 0x0000u);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(before, p[2], "a 16-bit write must not reach the third byte");
}

void test_aligned_entries_round_trip(void)
{
    uint8_t *p = mem; // mem is 16-aligned
    proxim.al_put_u16(p, 0x1234u);
    TEST_ASSERT_EQUAL_HEX64(0x1234u, proxim.al_load(p, 2u));

    proxim.al_put_u32(p, 0x12345678u);
    TEST_ASSERT_EQUAL_HEX64(0x12345678u, proxim.al_load(p, 4u));

    proxim.al_put_u64(p, 0x123456789ABCDEF0ull);
    TEST_ASSERT_EQUAL_HEX64(0x123456789ABCDEF0ull, proxim.al_load(p, 8u));

    TEST_ASSERT_EQUAL_HEX64((uint64_t)p[0], proxim.al_load(p, 1u));
}

void test_aligned_load_of_an_unsupported_width_reads_nothing(void)
{
    TEST_ASSERT_EQUAL_HEX64(0u, proxim.al_load(mem, 3u));
    TEST_ASSERT_EQUAL_HEX64(0u, proxim.al_load(mem, 0u));
}

void test_the_bulk_move_word_round_trips(void)
{
    unsigned char *p = (unsigned char *)mem;
    const mmgr_migro_word v = (mmgr_migro_word)0x0123456789ABCDEFull;

    proxim.mv_put(p, v);
    TEST_ASSERT_EQUAL_HEX64((uint64_t)v, (uint64_t)proxim.mv_load(p));
}

void test_the_move_word_is_the_configured_width(void)
{
    TEST_ASSERT_EQUAL_size_t(MMGR_RAW_WORD, sizeof(mmgr_migro_word));
    TEST_ASSERT_EQUAL_UINT(MMGR_RAW_WORD * 8u, MMGR_MV_BITS);
}

void test_read_copies_exactly_the_bytes_asked_for(void)
{
    uint8_t dst[16];
    for (unsigned i = 0; i < sizeof dst; i++)
    {
        dst[i] = 0xEEu;
    }

    proxim.read(dst, mem, 5u);
    for (unsigned i = 0; i < 5u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(mem[i], dst[i]);
    }
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xEEu, dst[5], "and not one byte more");
}

void test_read_of_nothing_writes_nothing(void)
{
    uint8_t dst[4] = {1u, 2u, 3u, 4u};
    proxim.read(dst, mem, 0u);
    TEST_ASSERT_EQUAL_HEX8(1u, dst[0]);
}

void test_read_at_every_alignment(void)
{
    uint8_t dst[32];
    for (unsigned off = 0; off < 8u; off++)
    {
        for (unsigned n = 1u; n <= 17u; n++)
        {
            for (unsigned i = 0; i < sizeof dst; i++)
            {
                dst[i] = 0u;
            }
            proxim.read(dst, mem + off, n);
            for (unsigned i = 0; i < n; i++)
            {
                TEST_ASSERT_EQUAL_HEX8(mem[off + i], dst[i]);
            }
        }
    }
}

void test_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_proxim_u16, proxim.u16);
    TEST_ASSERT_EQUAL_PTR(mmgr_proxim_read, proxim.read);
}
