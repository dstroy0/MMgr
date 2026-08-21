// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "bitio/bitio.h"

static uint8_t out[16];
static mmgr_bitio_writer w;

void setUp(void)
{
    for (unsigned i = 0; i < sizeof out; i++)
    {
        out[i] = 0xAAu;
    }
    w.out = out;
    w.cap = sizeof out;
    w.cnt = 0;
    w.acc = 0;
    w.nbits = 0;
    w.overflow = MMGR_FALSE;
}

void tearDown(void)
{
}

void test_bitio_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("bitio.h compiled with no header before it");
}

void test_a_whole_byte_lands_as_that_byte(void)
{
    bitio.put(&w, 0xC3u, 8);
    TEST_ASSERT_EQUAL_size_t(1u, w.cnt);
    TEST_ASSERT_EQUAL_HEX8(0xC3u, out[0]);
    TEST_ASSERT_FALSE(w.overflow);
}

void test_bits_pack_from_the_low_end(void)
{
    // acc |= bits << nbits, and the flush takes acc & 0xFF, so the first put occupies the low bits
    bitio.put(&w, 0x1u, 1);
    bitio.put(&w, 0x0u, 1);
    bitio.put(&w, 0x3u, 2);
    bitio.put(&w, 0x0u, 4);
    TEST_ASSERT_EQUAL_size_t(1u, w.cnt);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x0Du, out[0], "1 then 0 then 11 packs as 0b00001101");
}

void test_a_partial_write_waits_for_the_byte(void)
{
    bitio.put(&w, 0x5u, 3);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, w.cnt, "three bits do not make a byte");
    TEST_ASSERT_EQUAL_INT(3, w.nbits);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xAAu, out[0], "nothing written yet");
}

void test_align_flushes_the_partial_byte(void)
{
    bitio.put(&w, 0x5u, 3);
    bitio.align(&w);
    TEST_ASSERT_EQUAL_size_t(1u, w.cnt);
    TEST_ASSERT_EQUAL_HEX8(0x05u, out[0]);
    TEST_ASSERT_EQUAL_INT(0, w.nbits);
}

void test_align_on_a_boundary_writes_nothing(void)
{
    bitio.put(&w, 0xFFu, 8);
    const size_t before = w.cnt;
    bitio.align(&w);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, w.cnt, "already aligned, so there is nothing to flush");
}

void test_align_on_an_empty_writer_writes_nothing(void)
{
    bitio.align(&w);
    TEST_ASSERT_EQUAL_size_t(0u, w.cnt);
    TEST_ASSERT_FALSE(w.overflow);
}

void test_a_wide_put_spans_bytes(void)
{
    bitio.put(&w, 0xDEADBEEFu, 32);
    TEST_ASSERT_EQUAL_size_t(4u, w.cnt);
    TEST_ASSERT_EQUAL_HEX8(0xEFu, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBEu, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0xADu, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0xDEu, out[3]);
}

void test_n_at_or_above_32_takes_the_value_whole(void)
{
    // the mask arm would be undefined at n == 32, so the implementation branches around it
    bitio.put(&w, 0xFFFFFFFFu, 32);
    TEST_ASSERT_EQUAL_size_t(4u, w.cnt);
    for (unsigned i = 0; i < 4u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0xFFu, out[i]);
    }
}

void test_a_narrow_put_ignores_the_high_bits(void)
{
    bitio.put(&w, 0xFFu, 4);
    bitio.align(&w);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x0Fu, out[0], "only the low n bits of the value are used");
}

void test_overflow_latches_and_stops_writing(void)
{
    w.cap = 2u;
    bitio.put(&w, 0x11u, 8);
    bitio.put(&w, 0x22u, 8);
    TEST_ASSERT_FALSE(w.overflow);
    TEST_ASSERT_EQUAL_size_t(2u, w.cnt);

    bitio.put(&w, 0x33u, 8);
    TEST_ASSERT_TRUE_MESSAGE(w.overflow, "a third byte does not fit");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, w.cnt, "and must not have been written");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xAAu, out[2], "the byte past the cap is untouched");
}

void test_a_put_after_overflow_is_ignored(void)
{
    w.cap = 1u;
    bitio.put(&w, 0x11u, 8);
    bitio.put(&w, 0x22u, 8);
    TEST_ASSERT_TRUE(w.overflow);

    const size_t cnt = w.cnt;
    bitio.put(&w, 0x44u, 8);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(cnt, w.cnt, "overflow latches, so later puts do nothing");
}

void test_align_overflows_when_there_is_no_room(void)
{
    w.cap = 1u;
    bitio.put(&w, 0xFFu, 8);
    TEST_ASSERT_EQUAL_size_t(1u, w.cnt);
    bitio.put(&w, 0x1u, 3);
    bitio.align(&w);
    TEST_ASSERT_TRUE_MESSAGE(w.overflow, "the partial byte has nowhere to go");
}

void test_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_bitio_put, bitio.put);
    TEST_ASSERT_EQUAL_PTR(mmgr_bitio_align, bitio.align);
}
