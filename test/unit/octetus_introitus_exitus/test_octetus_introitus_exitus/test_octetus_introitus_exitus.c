/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "spatium/spatium.h"

static uint8_t buf[32];

void setUp(void)
{
    for (size_t i = 0; i < sizeof buf; i++)
    {
        buf[i] = 0xFFu;
    }
}

void tearDown(void)
{
}

static mmgr_span fill(void)
{
    return MMGR_CALL(spat.from, SpatiumCfg, .buf = buf, .cap = sizeof buf);
}

void test_byteio_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("byteio.h compiled with no header before it");
}

void test_byteio_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(OctetusIntroitusExitusNs), sizeof byteio,
                                     "the namespace instance is not its own type");
    TEST_ASSERT_EQUAL_PTR(mmgr_byteio_put_be, byteio.put_be);
    TEST_ASSERT_EQUAL_PTR(mmgr_byteio_take_be, byteio.take_be);
}

void test_put_appends_one_byte_and_moves_the_cursor(void)
{
    mmgr_span w = fill();

    MMGR_CALL(byteio.put, OctetusCfg, .w = &w, .byte = 0xA5u);

    TEST_ASSERT_EQUAL_HEX8(0xA5u, buf[0]);
    TEST_ASSERT_EQUAL_size_t(1u, w.pos);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFFu, buf[1], "one byte appended writes exactly one byte");
}

void test_put_be_writes_the_high_byte_first(void)
{
    mmgr_span w = fill();

    MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = (uint64_t)0x11223344ull, .bytes = (size_t)4);

    TEST_ASSERT_EQUAL_HEX8(0x11u, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x33u, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x44u, buf[3]);
}

/**
 * @brief A field writes its own bytes and nothing beyond them, at every width.
 *
 * @note The entry this replaced always stored a whole word and zero filled above the value, so the
 *       bytes after a short field were cleared. Appending into a span cannot do that: what follows
 *       belongs to whatever is appended next.
 */
void test_put_be_writes_only_the_bytes_it_was_given(void)
{
    for (size_t n = 1u; n <= 8u; n++)
    {
        setUp();

        mmgr_span w = fill();

        MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = (uint64_t)0x0102030405060708ull, .bytes = n);

        TEST_ASSERT_EQUAL_size_t(n, w.pos);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFFu, buf[n], "the byte past the field was written");
    }
}

void test_put_be_keeps_only_the_low_bytes(void)
{
    mmgr_span w = fill();

    MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = (uint64_t)0xDEADBEEFu, .bytes = (size_t)2);

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xBEu, buf[0], "two bytes of a four byte value is its low half");
    TEST_ASSERT_EQUAL_HEX8(0xEFu, buf[1]);
}

void test_appends_follow_one_another(void)
{
    mmgr_span w = fill();

    MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = (uint64_t)0xAABBu, .bytes = (size_t)2);
    MMGR_CALL(byteio.put, OctetusCfg, .w = &w, .byte = 0xCCu);
    MMGR_CALL(byteio.raw, OctetusCfg, .w = &w, .src = (const uint8_t *)"xy", .bytes = (size_t)2);

    TEST_ASSERT_EQUAL_HEX8(0xAAu, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBBu, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCCu, buf[2]);
    TEST_ASSERT_EQUAL_HEX8('x', buf[3]);
    TEST_ASSERT_EQUAL_HEX8('y', buf[4]);
    TEST_ASSERT_EQUAL_size_t(5u, w.pos);
    TEST_ASSERT_TRUE(MMGR_CALL(spat.ok, SpatiumCfg, .s = w));
}

void test_take_be_reads_what_put_be_wrote_at_every_width(void)
{
    static const uint64_t vals[8] = {0xEEull,
                                     0xBEEFull,
                                     0x123456ull,
                                     0x89ABCDEFull,
                                     0x0102030405ull,
                                     0x010203040506ull,
                                     0x01020304050607ull,
                                     0x0123456789ABCDEFull};

    for (size_t n = 1u; n <= 8u; n++)
    {
        setUp();

        mmgr_span w = fill();
        mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = buf, .cap = sizeof buf);
        uint64_t v = 0;

        MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = vals[n - 1u], .bytes = n);
        TEST_ASSERT_TRUE(MMGR_CALL(byteio.take_be, OctetusCfg, .r = &r, .out = &v, .bytes = n));
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(vals[n - 1u], v, "a take of the width that was put gives the value back");
    }
}

void test_takes_follow_one_another(void)
{
    mmgr_span w = fill();
    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = buf, .cap = sizeof buf);
    uint64_t a = 0;
    uint64_t b = 0;

    MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = (uint64_t)0x1122u, .bytes = (size_t)2);
    MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = (uint64_t)0x334455u, .bytes = (size_t)3);

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.take_be, OctetusCfg, .r = &r, .out = &a, .bytes = (size_t)2));
    TEST_ASSERT_TRUE(MMGR_CALL(byteio.take_be, OctetusCfg, .r = &r, .out = &b, .bytes = (size_t)3));

    TEST_ASSERT_EQUAL_HEX64(0x1122ull, a);
    TEST_ASSERT_EQUAL_HEX64(0x334455ull, b);
    TEST_ASSERT_EQUAL_size_t(5u, r.pos);
}

void test_take_be_of_fewer_bytes_takes_the_leading_ones(void)
{
    mmgr_span w = fill();
    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = buf, .cap = sizeof buf);
    uint64_t v = 0;

    MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = (uint64_t)0x0123456789ABCDEFull, .bytes = (size_t)8);
    TEST_ASSERT_TRUE(MMGR_CALL(byteio.take_be, OctetusCfg, .r = &r, .out = &v, .bytes = (size_t)3));
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x012345ull, v, "three bytes of an eight byte value is its leading half");
}

/**
 * @brief The latch contains an append that does not fit, in the build that does not trap on one.
 *
 * @note Shipping builds only. An append past the end is a build failure, and MMGR_DEBUG_CHECKS traps
 *       on it. What this covers is the other build: a wrong program that is not stopped must still
 *       be kept off the end of the buffer.
 */
void test_an_append_past_the_end_latches_and_writes_nothing(void)
{
#if MMGR_DEBUG_CHECKS
    TEST_IGNORE_MESSAGE("an append past the end traps under checks; the latch is the shipping path");
#else
    uint8_t small[4] = {0u, 0u, 0u, 0u};
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = small, .cap = sizeof small);

    MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = (uint64_t)0x1122334455667788ull, .bytes = (size_t)8);

    TEST_ASSERT_TRUE(w.overflow);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, small[0], "nothing was stored");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, w.pos, "but the cursor counted what was wanted");
#endif
}

/**
 * @brief Once a span has latched, every later append is refused too.
 *
 * @note Shipping builds only, for the same reason as the test above: reaching the latch at all takes
 *       an append that traps under MMGR_DEBUG_CHECKS. The behavior worth pinning here is that the
 *       latch does not un-stick - a later append that would have fit is still refused, so a wrong
 *       program cannot resume writing into a buffer it has already overrun.
 */
void test_a_latched_span_refuses_what_follows(void)
{
#if MMGR_DEBUG_CHECKS
    TEST_IGNORE_MESSAGE("reaching the latch traps under checks; the refusal is the shipping path");
#else
    uint8_t small[4] = {0u, 0u, 0u, 0u};
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = small, .cap = sizeof small);

    MMGR_CALL(byteio.put_be, OctetusCfg, .w = &w, .val = (uint64_t)0xFFFFFFFFFFFFFFFFull, .bytes = (size_t)8);
    MMGR_CALL(byteio.put, OctetusCfg, .w = &w, .byte = 0xAAu);

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, small[0], "a byte that would have fit is still refused");
#endif
}

void test_a_take_past_the_end_fails_and_holds_the_cursor(void)
{
    uint8_t small[4] = {1u, 2u, 3u, 4u};
    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = small, .cap = sizeof small);
    uint64_t v = 0xFFull;

    TEST_ASSERT_FALSE(MMGR_CALL(byteio.take_be, OctetusCfg, .r = &r, .out = &v, .bytes = (size_t)8));
    TEST_ASSERT_TRUE(r.err);
    TEST_ASSERT_EQUAL_size_t(0u, r.pos);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xFFull, v, "a failed take does not touch the output");
}

void test_a_raw_run_appends_as_it_is(void)
{
    mmgr_span w = fill();

    MMGR_CALL(byteio.raw, OctetusCfg, .w = &w, .src = (const uint8_t *)"0123456789", .bytes = (size_t)10);

    TEST_ASSERT_EQUAL_size_t(10u, w.pos);
    for (size_t i = 0; i < 10u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8((uint8_t)('0' + i), buf[i]);
    }
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFFu, buf[10], "and nothing past the run");
}

/* ---------------------------------------------------------------------------------------------
 * The length-prefixed run and the fixed-width integer, moved here from the string module. They
 * only ever needed an endian read and a pair of memory moves, so a string suite was never where
 * they belonged.
 * ------------------------------------------------------------------------------------------- */

void test_rd_str_reads_a_length_prefixed_run(void)
{
    static const uint8_t src[9] = {0x00u, 0x00u, 0x00u, 0x03u, 'a', 'b', 'c', 'x', 'y'};
    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = src, .cap = sizeof src);
    const uint8_t *s = NULL;
    size_t slen = 0;

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.rd_str, OctetusCfg, .r = &r, .blob = &s, .blen = &slen));
    TEST_ASSERT_EQUAL_size_t(3u, slen);
    TEST_ASSERT_EQUAL_PTR(src + 4, s);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(7u, r.pos, "the cursor lands past the run, ready for the next field");
}

void test_rd_str_reads_an_empty_run(void)
{
    static const uint8_t src[4] = {0u, 0u, 0u, 0u};
    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = src, .cap = sizeof src);
    const uint8_t *s = NULL;
    size_t slen = 9u;

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.rd_str, OctetusCfg, .r = &r, .blob = &s, .blen = &slen));
    TEST_ASSERT_EQUAL_size_t(0u, slen);
    TEST_ASSERT_EQUAL_size_t(4u, r.pos);
}

void test_rd_str_rewinds_when_the_run_is_cut_short(void)
{
    static const uint8_t src[6] = {0x00u, 0x00u, 0x00u, 0x09u, 'a', 'b'};
    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = src, .cap = sizeof src);
    const uint8_t *s = NULL;
    size_t slen = 0;

    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(byteio.rd_str, OctetusCfg, .r = &r, .blob = &s, .blen = &slen),
                              "the length claims nine, two are there");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, r.pos, "the cursor is put back where it started, not left mid field");
}

void test_rd_str_refuses_a_missing_length(void)
{
    static const uint8_t src[2] = {0u, 0u};
    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = src, .cap = sizeof src);
    const uint8_t *s = NULL;
    size_t slen = 0;

    TEST_ASSERT_FALSE(MMGR_CALL(byteio.rd_str, OctetusCfg, .r = &r, .blob = &s, .blen = &slen));
    TEST_ASSERT_EQUAL_size_t(0u, r.pos);
}

void test_rd_str_refuses_a_cursor_already_past_the_end(void)
{
    static const uint8_t src[8] = {0u, 0u, 0u, 1u, 'x', 0u, 0u, 0u};
    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = src, .cap = sizeof src);
    const uint8_t *s = NULL;
    size_t slen = 0;

    r.pos = sizeof src + 1u;
    TEST_ASSERT_FALSE(MMGR_CALL(byteio.rd_str, OctetusCfg, .r = &r, .blob = &s, .blen = &slen));
    TEST_ASSERT_NULL_MESSAGE(s, "a refused read writes nothing through the outputs");
}

void test_mpint_fixed_right_aligns_and_pads(void)
{
    static const uint8_t m[2] = {0x12u, 0x34u};
    uint8_t out[4] = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    mmgr_span f = MMGR_CALL(spat.from, SpatiumCfg, .buf = out, .cap = sizeof out);

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.mpint_fixed, OctetusCfg, .w = &f, .src = m, .bytes = sizeof m));
    TEST_ASSERT_EQUAL_HEX8(0x00u, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x12u, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0x34u, out[3]);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof out, f.pos, "the field is written whole, not appended to");
}

void test_mpint_fixed_drops_the_sign_padding(void)
{
    static const uint8_t m[3] = {0x00u, 0x80u, 0x01u};
    uint8_t out[2] = {0xFFu, 0xFFu};
    mmgr_span f = MMGR_CALL(spat.from, SpatiumCfg, .buf = out, .cap = sizeof out);

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.mpint_fixed, OctetusCfg, .w = &f, .src = m, .bytes = sizeof m));
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x80u, out[0], "the leading zero is not part of the value");
    TEST_ASSERT_EQUAL_HEX8(0x01u, out[1]);
}

void test_mpint_fixed_of_an_exact_width(void)
{
    static const uint8_t m[2] = {0xABu, 0xCDu};
    uint8_t out[2] = {0};
    mmgr_span f = MMGR_CALL(spat.from, SpatiumCfg, .buf = out, .cap = sizeof out);

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.mpint_fixed, OctetusCfg, .w = &f, .src = m, .bytes = sizeof m));
    TEST_ASSERT_EQUAL_HEX8(0xABu, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xCDu, out[1]);
}

void test_mpint_fixed_of_zero_is_all_zero(void)
{
    static const uint8_t m[3] = {0u, 0u, 0u};
    uint8_t out[4] = {1u, 2u, 3u, 4u};
    mmgr_span f = MMGR_CALL(spat.from, SpatiumCfg, .buf = out, .cap = sizeof out);

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.mpint_fixed, OctetusCfg, .w = &f, .src = m, .bytes = sizeof m));
    for (unsigned i = 0; i < 4u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0u, out[i]);
    }
}

void test_mpint_fixed_refuses_a_value_too_wide(void)
{
    static const uint8_t m[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t out[2] = {0xFFu, 0xFFu};
    mmgr_span f = MMGR_CALL(spat.from, SpatiumCfg, .buf = out, .cap = sizeof out);

    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(byteio.mpint_fixed, OctetusCfg, .w = &f, .src = m, .bytes = sizeof m),
                              "four bytes do not fit in two");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFFu, out[0], "a refused conversion leaves the output alone");
}