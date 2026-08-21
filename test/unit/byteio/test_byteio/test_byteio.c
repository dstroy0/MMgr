// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "byteio/byteio.h"

void test_byteio_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("byteio.h compiled with no header before it");
}

void test_byteio_namespace_is_wired(void)
{
    const ByteioNs *ns = &byteio;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(ByteioNs), sizeof(*ns), "the namespace instance is not its own type");
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
    mmgr_spat w = spat.from(buf, sizeof buf);

    byteio.put(&w, 0xA5u);
    TEST_ASSERT_EQUAL_size_t(1u, w.pos);
    TEST_ASSERT_EQUAL_HEX8(0xA5u, buf[0]);
    TEST_ASSERT_FALSE(w.overflow);
}

void test_put_past_the_end_latches_overflow(void)
{
    uint8_t buf[2];
    mmgr_spat w = spat.from(buf, sizeof buf);

    byteio.put(&w, 1u);
    byteio.put(&w, 2u);
    TEST_ASSERT_FALSE(w.overflow);

    byteio.put(&w, 3u);
    TEST_ASSERT_TRUE_MESSAGE(w.overflow, "the third byte does not fit in two");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u, w.pos, "pos still counts what was wanted");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(2u, buf[1], "nothing was written past the end");
}

void test_put_to_a_sizing_span_only_counts(void)
{
    mmgr_spat w = spat.from(NULL, 0u);

    byteio.put(&w, 1u);
    byteio.put(&w, 2u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, w.pos, "a sizing pass measures without a buffer");
    TEST_ASSERT_TRUE(w.overflow);
}

void test_put_be_writes_the_high_byte_first(void)
{
    uint8_t buf[4];
    mmgr_spat w = spat.from(buf, sizeof buf);

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
    mmgr_spat w = spat.from(buf, sizeof buf);

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
    mmgr_spat w = spat.from(buf, sizeof buf);

    byteio.put_be(&w, 0xDEADBEEFu, 2);
    TEST_ASSERT_EQUAL_size_t(2u, w.pos);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xBEu, buf[0], "two bytes of a four byte value is its low half");
    TEST_ASSERT_EQUAL_HEX8(0xEFu, buf[1]);
}

void test_put_be_past_the_end_latches_overflow(void)
{
    uint8_t buf[2];
    mmgr_spat w = spat.from(buf, sizeof buf);

    byteio.put_be(&w, 0x11223344u, 4);
    TEST_ASSERT_TRUE(w.overflow);
    TEST_ASSERT_EQUAL_size_t(4u, w.pos);
}

void test_raw_copies_a_run(void)
{
    uint8_t buf[8];
    mmgr_spat w = spat.from(buf, sizeof buf);

    byteio.raw(&w, "abc", 3u);
    TEST_ASSERT_EQUAL_size_t(3u, w.pos);
    TEST_ASSERT_EQUAL_HEX8('a', buf[0]);
    TEST_ASSERT_EQUAL_HEX8('c', buf[2]);
    TEST_ASSERT_FALSE(w.overflow);
}

void test_raw_that_does_not_fit_latches_overflow(void)
{
    uint8_t buf[2];
    mmgr_spat w = spat.from(buf, sizeof buf);

    byteio.raw(&w, "abcdef", 6u);
    TEST_ASSERT_TRUE(w.overflow);
    TEST_ASSERT_EQUAL_size_t(6u, w.pos);
}

void test_raw_to_a_sizing_span_only_counts(void)
{
    mmgr_spat w = spat.from(NULL, 0u);

    byteio.raw(&w, "abc", 3u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u, w.pos, "a sizing pass measures without a buffer");
    TEST_ASSERT_TRUE(w.overflow);
}

void test_raw_of_nothing_to_a_sizing_span_does_not_latch(void)
{
    mmgr_spat w = spat.from(NULL, 0u);

    byteio.raw(&w, "", 0u);
    TEST_ASSERT_FALSE_MESSAGE(w.overflow, "there was nothing to not fit");
    TEST_ASSERT_EQUAL_size_t(0u, w.pos);
}

void test_raw_of_nothing_does_not_latch(void)
{
    uint8_t buf[2];
    mmgr_spat w = spat.from(buf, sizeof buf);
    w.pos = 2u;

    byteio.raw(&w, "", 0u);
    TEST_ASSERT_FALSE_MESSAGE(w.overflow, "a zero length write always fits, even at the end");
    TEST_ASSERT_EQUAL_size_t(2u, w.pos);
}

void test_raw_after_an_overflowed_position(void)
{
    uint8_t buf[4];
    mmgr_spat w = spat.from(buf, sizeof buf);
    w.pos = 9u;

    byteio.raw(&w, "ab", 2u);
    TEST_ASSERT_TRUE_MESSAGE(w.overflow, "a position already past the end cannot take a write");
    TEST_ASSERT_EQUAL_size_t(11u, w.pos);
}

/* ---------------------------------------------------------------------------------------------
 * reading
 * ------------------------------------------------------------------------------------------- */

void test_take_be_reads_what_put_be_wrote(void)
{
    static const uint8_t buf[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    mmgr_fspat r = spat.cfrom(buf, sizeof buf);
    uint64_t v = 0;

    TEST_ASSERT_TRUE(byteio.take_be(&r, 4u, &v));
    TEST_ASSERT_EQUAL_HEX64(0x11223344ull, v);
    TEST_ASSERT_EQUAL_size_t(4u, r.pos);
    TEST_ASSERT_FALSE(r.err);
}

void test_take_be_walks_forward(void)
{
    static const uint8_t buf[4] = {0xAAu, 0xBBu, 0xCCu, 0xDDu};
    mmgr_fspat r = spat.cfrom(buf, sizeof buf);
    uint64_t a = 0;
    uint64_t b = 0;

    TEST_ASSERT_TRUE(byteio.take_be(&r, 1u, &a));
    TEST_ASSERT_TRUE(byteio.take_be(&r, 3u, &b));
    TEST_ASSERT_EQUAL_HEX64(0xAAull, a);
    TEST_ASSERT_EQUAL_HEX64(0xBBCCDDull, b);
    TEST_ASSERT_EQUAL_size_t(4u, r.pos);
}

void test_take_be_short_read_latches_the_error(void)
{
    static const uint8_t buf[2] = {1u, 2u};
    mmgr_fspat r = spat.cfrom(buf, sizeof buf);
    uint64_t v = 0xFFull;

    TEST_ASSERT_FALSE(byteio.take_be(&r, 4u, &v));
    TEST_ASSERT_TRUE(r.err);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, r.pos, "a refused read does not consume");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xFFull, v, "a refused read does not write the output");
}

void test_take_be_from_a_position_past_the_end(void)
{
    static const uint8_t buf[4] = {0};
    mmgr_fspat r = spat.cfrom(buf, sizeof buf);
    r.pos = 9u;
    uint64_t v = 0;

    TEST_ASSERT_FALSE(byteio.take_be(&r, 1u, &v));
    TEST_ASSERT_TRUE(r.err);
}

void test_take_be_of_nothing_succeeds(void)
{
    static const uint8_t buf[2] = {1u, 2u};
    mmgr_fspat r = spat.cfrom(buf, sizeof buf);
    uint64_t v = 0xFFull;

    TEST_ASSERT_TRUE(byteio.take_be(&r, 0u, &v));
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0ull, v, "zero bytes make zero");
    TEST_ASSERT_EQUAL_size_t(0u, r.pos);
}

void test_rd_u32_reads_and_advances(void)
{
    static const uint8_t buf[8] = {0x00u, 0x00u, 0x01u, 0x00u, 0xDEu, 0xADu, 0xBEu, 0xEFu};
    size_t off = 0;
    uint32_t v = 0;

    TEST_ASSERT_TRUE(byteio.rd_u32(buf, sizeof buf, &off, &v));
    TEST_ASSERT_EQUAL_HEX32(0x100u, v);
    TEST_ASSERT_EQUAL_size_t(4u, off);

    TEST_ASSERT_TRUE(byteio.rd_u32(buf, sizeof buf, &off, &v));
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, v);
    TEST_ASSERT_EQUAL_size_t(8u, off);
}

void test_rd_u32_refuses_a_short_tail(void)
{
    static const uint8_t buf[6] = {0};
    size_t off = 4u;
    uint32_t v = 0;

    TEST_ASSERT_FALSE_MESSAGE(byteio.rd_u32(buf, sizeof buf, &off, &v), "two bytes left is not four");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, off, "a refused read does not advance");
}

void test_rd_u32_refuses_an_offset_past_the_end(void)
{
    static const uint8_t buf[8] = {0};
    size_t off = 99u;
    uint32_t v = 0;

    TEST_ASSERT_FALSE(byteio.rd_u32(buf, sizeof buf, &off, &v));
    TEST_ASSERT_EQUAL_size_t(99u, off);
}

void test_rd_str_reads_a_length_prefixed_run(void)
{
    static const uint8_t buf[9] = {0x00u, 0x00u, 0x00u, 0x03u, 'a', 'b', 'c', 'x', 'y'};
    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 0;

    TEST_ASSERT_TRUE(byteio.rd_str(buf, sizeof buf, &off, &s, &slen));
    TEST_ASSERT_EQUAL_UINT32(3u, slen);
    TEST_ASSERT_EQUAL_PTR(buf + 4, s);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(7u, off, "the offset lands past the run, ready for the next field");
}

void test_rd_str_reads_an_empty_run(void)
{
    static const uint8_t buf[4] = {0u, 0u, 0u, 0u};
    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 9u;

    TEST_ASSERT_TRUE(byteio.rd_str(buf, sizeof buf, &off, &s, &slen));
    TEST_ASSERT_EQUAL_UINT32(0u, slen);
    TEST_ASSERT_EQUAL_size_t(4u, off);
}

void test_rd_str_rewinds_when_the_run_is_cut_short(void)
{
    static const uint8_t buf[6] = {0x00u, 0x00u, 0x00u, 0x09u, 'a', 'b'};
    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 0;

    TEST_ASSERT_FALSE_MESSAGE(byteio.rd_str(buf, sizeof buf, &off, &s, &slen), "the length claims nine, two are there");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, off, "the offset is put back where it started, not left mid field");
}

void test_rd_str_refuses_a_missing_length(void)
{
    static const uint8_t buf[2] = {0u, 0u};
    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 0;

    TEST_ASSERT_FALSE(byteio.rd_str(buf, sizeof buf, &off, &s, &slen));
    TEST_ASSERT_EQUAL_size_t(0u, off);
}

/* ---------------------------------------------------------------------------------------------
 * mpint
 * ------------------------------------------------------------------------------------------- */

void test_mpint_fixed_right_aligns_and_pads(void)
{
    static const uint8_t m[2] = {0x12u, 0x34u};
    uint8_t out[4] = {0xFFu, 0xFFu, 0xFFu, 0xFFu};

    TEST_ASSERT_TRUE(byteio.mpint_fixed(m, sizeof m, out, sizeof out));
    TEST_ASSERT_EQUAL_HEX8(0x00u, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x12u, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0x34u, out[3]);
}

void test_mpint_fixed_drops_the_sign_padding(void)
{
    // An mpint carries a leading zero when the top bit of the value would read as negative.
    static const uint8_t m[3] = {0x00u, 0x80u, 0x01u};
    uint8_t out[2] = {0xFFu, 0xFFu};

    TEST_ASSERT_TRUE(byteio.mpint_fixed(m, sizeof m, out, sizeof out));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x80u, out[0], "the leading zero is not part of the value");
    TEST_ASSERT_EQUAL_HEX8(0x01u, out[1]);
}

void test_mpint_fixed_of_an_exact_width(void)
{
    static const uint8_t m[2] = {0xABu, 0xCDu};
    uint8_t out[2] = {0};

    TEST_ASSERT_TRUE(byteio.mpint_fixed(m, sizeof m, out, sizeof out));
    TEST_ASSERT_EQUAL_HEX8(0xABu, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xCDu, out[1]);
}

void test_mpint_fixed_of_zero_is_all_zero(void)
{
    static const uint8_t m[3] = {0u, 0u, 0u};
    uint8_t out[4] = {1u, 2u, 3u, 4u};

    TEST_ASSERT_TRUE(byteio.mpint_fixed(m, sizeof m, out, sizeof out));
    for (unsigned i = 0; i < 4u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0u, out[i]);
    }
}

void test_mpint_fixed_refuses_a_value_too_wide(void)
{
    static const uint8_t m[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t out[2] = {0xFFu, 0xFFu};

    TEST_ASSERT_FALSE_MESSAGE(byteio.mpint_fixed(m, sizeof m, out, sizeof out), "four bytes do not fit in two");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFFu, out[0], "a refused conversion leaves the output alone");
}
