// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "endian/endian.h"

static uint8_t b[16];

void setUp(void)
{
    for (unsigned i = 0; i < sizeof b; i++)
    {
        b[i] = 0x5Au;
    }
}

void tearDown(void)
{
}

void test_endian_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("endian.h compiled with no header before it");
}

void test_little_endian_puts_the_low_byte_first(void)
{
    TEST_ASSERT_EQUAL_size_t(2u, endian.wr16le(b, 0x1122u));
    TEST_ASSERT_EQUAL_HEX8(0x22u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[1]);

    TEST_ASSERT_EQUAL_size_t(4u, endian.wr32le(b, 0x11223344u));
    TEST_ASSERT_EQUAL_HEX8(0x44u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[3]);

    TEST_ASSERT_EQUAL_size_t(8u, endian.wr64le(b, 0x1122334455667788ull));
    TEST_ASSERT_EQUAL_HEX8(0x88u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[7]);
}

void test_big_endian_puts_the_high_byte_first(void)
{
    TEST_ASSERT_EQUAL_size_t(2u, endian.wr16be(b, 0x1122u));
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, b[1]);

    TEST_ASSERT_EQUAL_size_t(4u, endian.wr32be(b, 0x11223344u));
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x44u, b[3]);

    TEST_ASSERT_EQUAL_size_t(8u, endian.wr64be(b, 0x1122334455667788ull));
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x88u, b[7]);
}

void test_every_width_round_trips_little_endian(void)
{
    static const uint64_t vals[] = {0ull,
                                    1ull,
                                    0x7Full,
                                    0x80ull,
                                    0xFFull,
                                    0x1234ull,
                                    0xFFFFull,
                                    0x12345678ull,
                                    0xFFFFFFFFull,
                                    0x123456789ABCDEF0ull,
                                    0xFFFFFFFFFFFFFFFFull};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        endian.wr16le(b, (uint16_t)vals[i]);
        TEST_ASSERT_EQUAL_HEX16((uint16_t)vals[i], endian.rd16le(b));

        endian.wr32le(b, (uint32_t)vals[i]);
        TEST_ASSERT_EQUAL_HEX32((uint32_t)vals[i], endian.rd32le(b));

        endian.wr64le(b, vals[i]);
        TEST_ASSERT_EQUAL_HEX64(vals[i], endian.rd64le(b));
    }
}

void test_every_width_round_trips_big_endian(void)
{
    static const uint64_t vals[] = {0ull,
                                    1ull,
                                    0x7Full,
                                    0x80ull,
                                    0xFFull,
                                    0x1234ull,
                                    0xFFFFull,
                                    0x12345678ull,
                                    0xFFFFFFFFull,
                                    0x123456789ABCDEF0ull,
                                    0xFFFFFFFFFFFFFFFFull};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        endian.wr16be(b, (uint16_t)vals[i]);
        TEST_ASSERT_EQUAL_HEX16((uint16_t)vals[i], endian.rd16be(b));

        endian.wr32be(b, (uint32_t)vals[i]);
        TEST_ASSERT_EQUAL_HEX32((uint32_t)vals[i], endian.rd32be(b));

        endian.wr64be(b, vals[i]);
        TEST_ASSERT_EQUAL_HEX64(vals[i], endian.rd64be(b));
    }
}

void test_the_two_orders_are_byte_reverses_of_each_other(void)
{
    uint8_t le[8];
    uint8_t be[8];

    endian.wr64le(le, 0x0102030405060708ull);
    endian.wr64be(be, 0x0102030405060708ull);
    for (unsigned i = 0; i < 8u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(le[i], be[7u - i], "one order is the other reversed");
    }
}

void test_a_read_of_the_other_order_is_the_byte_swap(void)
{
    endian.wr16be(b, 0x1234u);
    TEST_ASSERT_EQUAL_HEX16(0x3412u, endian.rd16le(b));

    endian.wr32be(b, 0x12345678u);
    TEST_ASSERT_EQUAL_HEX32(0x78563412u, endian.rd32le(b));

    endian.wr64be(b, 0x123456789ABCDEF0ull);
    TEST_ASSERT_EQUAL_HEX64(0xF0DEBC9A78563412ull, endian.rd64le(b));
}

void test_writes_touch_exactly_their_width(void)
{
    endian.wr16le(b, 0xFFFFu);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x5Au, b[2], "a 16-bit write must not touch the third byte");

    for (unsigned i = 0; i < sizeof b; i++)
    {
        b[i] = 0x5Au;
    }
    endian.wr32be(b, 0xFFFFFFFFu);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x5Au, b[4], "a 32-bit write must not touch the fifth byte");

    for (unsigned i = 0; i < sizeof b; i++)
    {
        b[i] = 0x5Au;
    }
    endian.wr64le(b, 0xFFFFFFFFFFFFFFFFull);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x5Au, b[8], "a 64-bit write must not touch the ninth byte");
}

void test_reads_are_unaffected_by_the_bytes_after_them(void)
{
    endian.wr16le(b, 0x1122u);
    b[2] = 0xFFu;
    b[3] = 0xFFu;
    TEST_ASSERT_EQUAL_HEX16(0x1122u, endian.rd16le(b));
}

void test_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_wr16le, endian.wr16le);
    TEST_ASSERT_EQUAL_PTR(mmgr_rd64be, endian.rd64be);
}
