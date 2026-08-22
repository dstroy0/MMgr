// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
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

/* ---------------------------------------------------------------------------------------------
 * writing
 *
 * A span counts what was asked for whether or not it fit, the way snprintf reports the length it
 * wanted. So a write past the end moves pos and latches overflow, and pos is a size and not a
 * position once that has happened.
 * ------------------------------------------------------------------------------------------- */

void test_put_writes_one_byte(void)
{
    uint8_t buf[4];
    mmgr_spat w = spat.init(&(SpatCfg){buf, sizeof buf});

    byteio.put(&w, 0xA5u);
    TEST_ASSERT_EQUAL_size_t(1u, w.pos);
    TEST_ASSERT_EQUAL_HEX8(0xA5u, buf[0]);
}

void test_put_be_writes_the_high_byte_first(void)
{
    uint8_t buf[4];
    mmgr_spat w = spat.init(&(SpatCfg){buf, sizeof buf});

    byteio.put_be(&w, 0x11223344u, 4);
    TEST_ASSERT_EQUAL_size_t(4u, w.pos);
    TEST_ASSERT_EQUAL_HEX8(0x11u, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x33u, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x44u, buf[3]);
}

void test_put_be_at_every_width(void)
{
    uint8_t buf[16];
    mmgr_spat w = spat.init(&(SpatCfg){buf, sizeof buf});

    byteio.put_be(&w, 0xEEu, 1);
    byteio.put_be(&w, 0xBEEFu, 2);
    byteio.put_be(&w, 0x0123456789ABCDEFull, 8);

    TEST_ASSERT_EQUAL_size_t(11u, w.pos);
    TEST_ASSERT_EQUAL_HEX8(0xEEu, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBEu, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0xEFu, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0xEFu, buf[10]);
}

void test_put_be_keeps_only_the_low_bytes(void)
{
    uint8_t buf[4];
    mmgr_spat w = spat.init(&(SpatCfg){buf, sizeof buf});

    byteio.put_be(&w, 0xDEADBEEFu, 2);
    TEST_ASSERT_EQUAL_size_t(2u, w.pos);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xBEu, buf[0], "two bytes of a four byte value is its low half");
    TEST_ASSERT_EQUAL_HEX8(0xEFu, buf[1]);
}

void test_raw_copies_a_run(void)
{
    uint8_t buf[8];
    mmgr_spat w = spat.init(&(SpatCfg){buf, sizeof buf});

    byteio.raw(&w, "abc", 3u);
    TEST_ASSERT_EQUAL_size_t(3u, w.pos);
    TEST_ASSERT_EQUAL_HEX8('a', buf[0]);
    TEST_ASSERT_EQUAL_HEX8('c', buf[2]);
}

void test_raw_of_nothing_does_not_latch(void)
{
    uint8_t buf[2];
    mmgr_spat w = spat.init(&(SpatCfg){buf, sizeof buf});
    w.pos = 2u;

    byteio.raw(&w, "", 0u);
    TEST_ASSERT_EQUAL_size_t(2u, w.pos);
}

/* ---------------------------------------------------------------------------------------------
 * reading
 * ------------------------------------------------------------------------------------------- */

void test_take_be_reads_what_put_be_wrote(void)
{
    static const uint8_t buf[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    size_t r_off = 0u;
    const uint8_t *r_buf = buf;
    uint64_t v = 0;

    byteio.take_be(r_buf, sizeof r_buf, &r_off, &v, 4u);
    TEST_ASSERT_EQUAL_HEX64(0x11223344ull, v);
    TEST_ASSERT_EQUAL_size_t(4u, r_off);
}

void test_take_be_walks_forward(void)
{
    static const uint8_t buf[4] = {0xAAu, 0xBBu, 0xCCu, 0xDDu};
    size_t r_off = 0u;
    const uint8_t *r_buf = buf;
    uint64_t a = 0;
    uint64_t b = 0;

    byteio.take_be(r_buf, sizeof r_buf, &r_off, &a, 1u);
    byteio.take_be(r_buf, sizeof r_buf, &r_off, &b, 3u);
    TEST_ASSERT_EQUAL_HEX64(0xAAull, a);
    TEST_ASSERT_EQUAL_HEX64(0xBBCCDDull, b);
    TEST_ASSERT_EQUAL_size_t(4u, r_off);
}

void test_take_be_of_nothing_succeeds(void)
{
    static const uint8_t buf[2] = {1u, 2u};
    size_t r_off = 0u;
    const uint8_t *r_buf = buf;
    uint64_t v = 0xFFull;

    byteio.take_be(r_buf, sizeof r_buf, &r_off, &v, 0u);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0ull, v, "zero bytes make zero");
    TEST_ASSERT_EQUAL_size_t(0u, r_off);
}

void test_rd_u32_reads_and_advances(void)
{
    static const uint8_t buf[8] = {0x00u, 0x00u, 0x01u, 0x00u, 0xDEu, 0xADu, 0xBEu, 0xEFu};
    size_t off = 0;
    uint32_t v = 0;

    byteio.rd_u32(buf, sizeof buf, &off, &v);
    TEST_ASSERT_EQUAL_HEX32(0x100u, v);
    TEST_ASSERT_EQUAL_size_t(4u, off);

    byteio.rd_u32(buf, sizeof buf, &off, &v);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, v);
    TEST_ASSERT_EQUAL_size_t(8u, off);
}
