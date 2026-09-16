#include "endian/endian.c"

#include "unity.h"

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
    TEST_ASSERT_EQUAL_size_t(2u, parva_extremitas.wr(&(EndianCfg){b, 0, 0x1122u, MMGR_ENDIAN_16}));
    TEST_ASSERT_EQUAL_HEX8(0x22u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[1]);

    TEST_ASSERT_EQUAL_size_t(4u, parva_extremitas.wr(&(EndianCfg){b, 0, 0x11223344u, MMGR_ENDIAN_32}));
    TEST_ASSERT_EQUAL_HEX8(0x44u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[3]);

    TEST_ASSERT_EQUAL_size_t(8u, parva_extremitas.wr(&(EndianCfg){b, 0, 0x1122334455667788ull, MMGR_ENDIAN_64}));
    TEST_ASSERT_EQUAL_HEX8(0x88u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[7]);
}

void test_big_endian_puts_the_high_byte_first(void)
{
    TEST_ASSERT_EQUAL_size_t(2u, magna_extremitas.wr(&(EndianCfg){b, 0, 0x1122u, MMGR_ENDIAN_16}));
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, b[1]);

    TEST_ASSERT_EQUAL_size_t(4u, magna_extremitas.wr(&(EndianCfg){b, 0, 0x11223344u, MMGR_ENDIAN_32}));
    TEST_ASSERT_EQUAL_HEX8(0x11u, b[0]);
    TEST_ASSERT_EQUAL_HEX8(0x44u, b[3]);

    TEST_ASSERT_EQUAL_size_t(8u, magna_extremitas.wr(&(EndianCfg){b, 0, 0x1122334455667788ull, MMGR_ENDIAN_64}));
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
        parva_extremitas.wr(&(EndianCfg){b, 0, (uint16_t)vals[i], MMGR_ENDIAN_16});
        TEST_ASSERT_EQUAL_HEX16((uint16_t)vals[i],
                                (uint16_t)parva_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_16}));

        parva_extremitas.wr(&(EndianCfg){b, 0, (uint32_t)vals[i], MMGR_ENDIAN_32});
        TEST_ASSERT_EQUAL_HEX32((uint32_t)vals[i],
                                (uint32_t)parva_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_32}));

        parva_extremitas.wr(&(EndianCfg){b, 0, vals[i], MMGR_ENDIAN_64});
        TEST_ASSERT_EQUAL_HEX64(vals[i], parva_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_64}));
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
        magna_extremitas.wr(&(EndianCfg){b, 0, (uint16_t)vals[i], MMGR_ENDIAN_16});
        TEST_ASSERT_EQUAL_HEX16((uint16_t)vals[i],
                                (uint16_t)magna_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_16}));

        magna_extremitas.wr(&(EndianCfg){b, 0, (uint32_t)vals[i], MMGR_ENDIAN_32});
        TEST_ASSERT_EQUAL_HEX32((uint32_t)vals[i],
                                (uint32_t)magna_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_32}));

        magna_extremitas.wr(&(EndianCfg){b, 0, vals[i], MMGR_ENDIAN_64});
        TEST_ASSERT_EQUAL_HEX64(vals[i], magna_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_64}));
    }
}

void test_the_two_orders_are_byte_reverses_of_each_other(void)
{
    uint8_t lo_first[8];
    uint8_t hi_first[8];

    parva_extremitas.wr(&(EndianCfg){lo_first, 0, 0x0102030405060708ull, MMGR_ENDIAN_64});
    magna_extremitas.wr(&(EndianCfg){hi_first, 0, 0x0102030405060708ull, MMGR_ENDIAN_64});
    for (unsigned i = 0; i < 8u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(lo_first[i], hi_first[7u - i], "one order is the other reversed");
    }
}

void test_a_read_of_the_other_order_is_the_byte_swap(void)
{
    magna_extremitas.wr(&(EndianCfg){b, 0, 0x1234u, MMGR_ENDIAN_16});
    TEST_ASSERT_EQUAL_HEX16(0x3412u, (uint16_t)parva_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_16}));

    magna_extremitas.wr(&(EndianCfg){b, 0, 0x12345678u, MMGR_ENDIAN_32});
    TEST_ASSERT_EQUAL_HEX32(0x78563412u, (uint32_t)parva_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_32}));

    magna_extremitas.wr(&(EndianCfg){b, 0, 0x123456789ABCDEF0ull, MMGR_ENDIAN_64});
    TEST_ASSERT_EQUAL_HEX64(0xF0DEBC9A78563412ull, parva_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_64}));
}

void test_writes_touch_exactly_their_width(void)
{
    parva_extremitas.wr(&(EndianCfg){b, 0, 0xFFFFu, MMGR_ENDIAN_16});
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x5Au, b[2], "a 16-bit write must not touch the third byte");

    for (unsigned i = 0; i < sizeof b; i++)
    {
        b[i] = 0x5Au;
    }
    magna_extremitas.wr(&(EndianCfg){b, 0, 0xFFFFFFFFu, MMGR_ENDIAN_32});
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x5Au, b[4], "a 32-bit write must not touch the fifth byte");

    for (unsigned i = 0; i < sizeof b; i++)
    {
        b[i] = 0x5Au;
    }
    parva_extremitas.wr(&(EndianCfg){b, 0, 0xFFFFFFFFFFFFFFFFull, MMGR_ENDIAN_64});
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x5Au, b[8], "a 64-bit write must not touch the ninth byte");
}

void test_reads_are_unaffected_by_the_bytes_after_them(void)
{
    parva_extremitas.wr(&(EndianCfg){b, 0, 0x1122u, MMGR_ENDIAN_16});
    b[2] = 0xFFu;
    b[3] = 0xFFu;
    TEST_ASSERT_EQUAL_HEX16(0x1122u, (uint16_t)parva_extremitas.rd(&(EndianCfg){0, b, 0, MMGR_ENDIAN_16}));
}
