#include "unity.h"

#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

void test_byteio_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("byteio.h compiled with no header before it");
}

void test_byteio_namespace_is_wired(void)
{
    const OctetusIntroitusExitusNs *ns = &byteio;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(OctetusIntroitusExitusNs), sizeof(*ns), "the namespace instance is not its own type");
}


void test_put_writes_one_byte(void)
{
    uint64_t store[1] = {~(uint64_t)0};
    uint8_t *buf = (uint8_t *)store;

    MMGR_CALL(byteio.put, OctetusCfg, .at = buf, .val = (uint64_t)0xA5u, .bytes = (size_t)1);
    TEST_ASSERT_EQUAL_HEX8(0xA5u, buf[0]);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, buf[1], "a put stores the whole word, so the bytes past n are cleared");
}

void test_put_writes_the_high_byte_first(void)
{
    uint64_t store[1] = {~(uint64_t)0};
    uint8_t *buf = (uint8_t *)store;

    MMGR_CALL(byteio.put, OctetusCfg, .at = buf, .val = (uint64_t)0x11223344ull, .bytes = (size_t)4);
    TEST_ASSERT_EQUAL_HEX8(0x11u, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x33u, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x44u, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0u, buf[4]);
}

void test_put_at_every_width(void)
{
    uint64_t store[3] = {~(uint64_t)0, ~(uint64_t)0, ~(uint64_t)0};
    uint8_t *buf = (uint8_t *)store;

    MMGR_CALL(byteio.put, OctetusCfg, .at = buf, .val = (uint64_t)0xEEu, .bytes = (size_t)1);
    MMGR_CALL(byteio.put, OctetusCfg, .at = buf + 8, .val = (uint64_t)0xBEEFu, .bytes = (size_t)2);
    MMGR_CALL(byteio.put, OctetusCfg, .at = buf + 16, .val = (uint64_t)0x0123456789ABCDEFull, .bytes = (size_t)8);

    TEST_ASSERT_EQUAL_HEX8(0xEEu, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBEu, buf[8]);
    TEST_ASSERT_EQUAL_HEX8(0xEFu, buf[9]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, buf[16]);
    TEST_ASSERT_EQUAL_HEX8(0xEFu, buf[23]);
}

void test_put_keeps_only_the_low_bytes(void)
{
    uint64_t store[1] = {~(uint64_t)0};
    uint8_t *buf = (uint8_t *)store;

    MMGR_CALL(byteio.put, OctetusCfg, .at = buf, .val = (uint64_t)0xDEADBEEFu, .bytes = (size_t)2);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xBEu, buf[0], "two bytes of a four byte value is its low half");
    TEST_ASSERT_EQUAL_HEX8(0xEFu, buf[1]);
}


void test_take_reads_what_put_wrote(void)
{
    uint64_t store[1] = {0};
    uint8_t *buf = (uint8_t *)store;
    uint64_t v = 0;

    MMGR_CALL(byteio.put, OctetusCfg, .at = buf, .val = (uint64_t)0x11223344ull, .bytes = (size_t)4);
    MMGR_CALL(byteio.take, OctetusCfg, .from = buf, .out = &v, .bytes = (size_t)4);
    TEST_ASSERT_EQUAL_HEX64(0x11223344ull, v);
}

void test_take_reads_back_at_every_width(void)
{
    static const uint64_t vals[8] = {0xEEull,
                                     0xBEEFull,
                                     0x123456ull,
                                     0x89ABCDEFull,
                                     0x0102030405ull,
                                     0x010203040506ull,
                                     0x01020304050607ull,
                                     0x0123456789ABCDEFull};
    uint64_t store[1] = {0};
    uint8_t *buf = (uint8_t *)store;

    for (size_t n = 1u; n <= 8u; n++)
    {
        uint64_t v = 0;

        MMGR_CALL(byteio.put, OctetusCfg, .at = buf, .val = vals[n - 1u], .bytes = n);
        MMGR_CALL(byteio.take, OctetusCfg, .from = buf, .out = &v, .bytes = n);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(vals[n - 1u], v, "a take of the width that was put gives the value back");
    }
}

void test_take_of_fewer_bytes_takes_the_leading_ones(void)
{
    uint64_t store[1] = {0};
    uint8_t *buf = (uint8_t *)store;
    uint64_t v = 0;

    MMGR_CALL(byteio.put, OctetusCfg, .at = buf, .val = (uint64_t)0x0123456789ABCDEFull, .bytes = (size_t)8);
    MMGR_CALL(byteio.take, OctetusCfg, .from = buf, .out = &v, .bytes = (size_t)3);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x012345ull, v, "three bytes of an eight byte value is its leading half");
}

void test_take_reads_a_pattern_it_did_not_write(void)
{
    static const uint64_t store[1] = {0};
    const uint8_t *buf = (const uint8_t *)store;
    uint64_t v = 0xFFull;

    MMGR_CALL(byteio.take, OctetusCfg, .from = buf, .out = &v, .bytes = (size_t)8);
    TEST_ASSERT_EQUAL_HEX64(0ull, v);
}
