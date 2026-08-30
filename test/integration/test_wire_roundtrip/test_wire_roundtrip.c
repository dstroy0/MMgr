/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "spatium/spatium.h"
#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"
#include "cellularum_laboro/cellularum_laboro.h"

static uint64_t store[4];
static uint8_t *mem;

void setUp(void)
{
    for (unsigned i = 0; i < sizeof store / sizeof store[0]; i++)
    {
        store[i] = 0u;
    }
    mem = (uint8_t *)store;
}

void tearDown(void)
{
}

void test_a_byte_written_is_the_byte_read(void)
{
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem, .cap = sizeof store);
    uint64_t got = 0;

    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = (uint64_t)0xA5u, .bytes = (size_t)1);
    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = (uint64_t)0x5Au, .bytes = (size_t)1);

    // The span carried the cursor, so the second byte landed after the first without being told where
    TEST_ASSERT_EQUAL_UINT8(0xA5u, mem[0]);
    TEST_ASSERT_EQUAL_UINT8(0x5Au, mem[1]);
    TEST_ASSERT_EQUAL_size_t(2u, w.pos);
    TEST_ASSERT_TRUE(MMGR_CALL(spat.ok, SpatiumCfg, .span = w));

    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = mem, .cap = sizeof store);

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.take_be, OctetusCfg, .read_span = &r, .out = &got, .bytes = (size_t)1));
    TEST_ASSERT_EQUAL_HEX64(0xA5ull, got);
    TEST_ASSERT_TRUE(MMGR_CALL(byteio.take_be, OctetusCfg, .read_span = &r, .out = &got, .bytes = (size_t)1));
    TEST_ASSERT_EQUAL_HEX64(0x5Aull, got);
}

void test_big_endian_fields_round_trip_at_every_width(void)
{
    static const struct
    {
        uint64_t v;
        size_t n;
    } cases[] = {
        {0x00u, 1u},           {0xFFu, 1u},             {0x1234u, 2u},          {0xFFFFu, 2u},
        {0x123456u, 3u},       {0x12345678u, 4u},       {0xFFFFFFFFu, 4u},      {0x123456789Au, 5u},
        {0x123456789ABCu, 6u}, {0x123456789ABCDEu, 7u}, {0x123456789ABCDEF0ull, 8u},
    };

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        setUp();

        mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem, .cap = sizeof store);
        mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = mem, .cap = sizeof store);
        uint64_t got = 0;

        MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = cases[i].v, .bytes = cases[i].n);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(cases[i].n, w.pos, "the cursor moved by what was written");

        TEST_ASSERT_TRUE(MMGR_CALL(byteio.take_be, OctetusCfg, .read_span = &r, .out = &got, .bytes = cases[i].n));
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(cases[i].v, got, "a field read back must be the field written");
        TEST_ASSERT_EQUAL_size_t(cases[i].n, r.pos);
    }
}

void test_the_writer_puts_the_high_byte_first(void)
{
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem, .cap = sizeof store);

    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = (uint64_t)0x11223344u, .bytes = (size_t)4);

    TEST_ASSERT_EQUAL_UINT8(0x11u, mem[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22u, mem[1]);
    TEST_ASSERT_EQUAL_UINT8(0x33u, mem[2]);
    TEST_ASSERT_EQUAL_UINT8(0x44u, mem[3]);
}

/**
 * @brief An odd width writes exactly its own bytes and no more, which the old writer did not.
 *
 * @note The entry this replaced always stored eight bytes and zero filled above the value. Over a
 *       span that would trample whatever followed, so it writes the count it was given.
 */
void test_an_odd_width_writes_only_its_own_bytes(void)
{
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem, .cap = sizeof store);

    mem[3] = 0xEEu;
    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = (uint64_t)0x112233u, .bytes = (size_t)3);

    TEST_ASSERT_EQUAL_UINT8(0x11u, mem[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22u, mem[1]);
    TEST_ASSERT_EQUAL_UINT8(0x33u, mem[2]);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xEEu, mem[3], "the byte past a three byte field was not touched");
}

void test_endian_entries_agree_with_the_wire_writer(void)
{
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem, .cap = sizeof store);
    uint8_t viaendian[8];

    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = (uint64_t)0xDEADBEEFu, .bytes = (size_t)4);
    magna_extremitas.wr(&(EndianCfg){viaendian, 0, 0xDEADBEEFu, MMGR_ENDIAN_32});

    TEST_ASSERT_EQUAL_INT_MESSAGE(0,
                                  MMGR_CALL(memor.cmp, MemoriaCfg, .src = mem, .other = viaendian, .bytes = (size_t)4),
                                  "two ways of writing the same field must produce the same bytes");
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, (uint32_t)magna_extremitas.rd(&(EndianCfg){0, mem, 0, MMGR_ENDIAN_32}));
}

/**
 * @brief An append that does not fit latches the span rather than writing past it.
 *
 * @note Shipping builds only. Writing eight bytes into four is a wrong program, and under
 *       MMGR_DEBUG_CHECKS byteio_claim traps on it, which is what that build is for. What is left to
 *       test is the other build's behavior: a wrong program that is not stopped must still be
 *       contained, so the append stores nothing rather than running off the end.
 */
void test_a_field_past_the_end_latches_rather_than_writing(void)
{
#if MMGR_DEBUG_CHECKS
    TEST_IGNORE_MESSAGE("an append past the end traps under checks; the latch is the shipping path");
#else
    uint8_t small[4] = {0u, 0u, 0u, 0u};
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = small, .cap = sizeof small);

    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = (uint64_t)0x1122334455667788ull,
              .bytes = (size_t)8);

    TEST_ASSERT_TRUE_MESSAGE(w.overflow, "eight bytes into four must latch");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, small[0], "and must not have written anything");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, w.pos, "while still counting what it would have needed");
#endif
}

/**
 * @brief A read that runs out says so and leaves the cursor where it was.
 */
void test_a_field_past_the_end_of_a_read_leaves_the_cursor(void)
{
    uint8_t small[4] = {1u, 2u, 3u, 4u};
    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = small, .cap = sizeof small);
    uint64_t got = 0xFFu;

    TEST_ASSERT_FALSE(MMGR_CALL(byteio.take_be, OctetusCfg, .read_span = &r, .out = &got, .bytes = (size_t)8));
    TEST_ASSERT_TRUE(r.err);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, r.pos, "a failed read does not move the cursor");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xFFull, got, "and does not touch the output");
}

void test_a_run_of_bytes_appends_as_it_is(void)
{
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem, .cap = sizeof store);

    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = (uint64_t)5u, .bytes = (size_t)4);
    MMGR_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = (const uint8_t *)"hello", .bytes = (size_t)5);

    TEST_ASSERT_EQUAL_size_t(9u, w.pos);
    TEST_ASSERT_TRUE(MMGR_CALL(spat.ok, SpatiumCfg, .span = w));
    TEST_ASSERT_EQUAL_INT(0, MMGR_CALL(memor.cmp, MemoriaCfg, .src = mem + 4, .other = "hello", .bytes = (size_t)5));
}

void test_a_single_byte_append_counts_the_cursor(void)
{
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem, .cap = sizeof store);

    MMGR_CALL(byteio.put, OctetusCfg, .write_span = &w, .byte = 0x7Fu);
    MMGR_CALL(byteio.put, OctetusCfg, .write_span = &w, .byte = 0x80u);

    TEST_ASSERT_EQUAL_UINT8(0x7Fu, mem[0]);
    TEST_ASSERT_EQUAL_UINT8(0x80u, mem[1]);
    TEST_ASSERT_EQUAL_size_t(2u, w.pos);
}

void test_a_length_prefixed_string_round_trips(void)
{
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem, .cap = sizeof store);

    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = (uint64_t)5u, .bytes = (size_t)4);
    MMGR_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = (const uint8_t *)"hello", .bytes = (size_t)5);

    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = mem, .cap = 9u);
    const uint8_t *s = NULL;
    size_t slen = 0;

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.rd_str, OctetusCfg, .read_span = &r, .blob = &s, .blob_bytes = &slen));
    TEST_ASSERT_EQUAL_size_t(5u, slen);
    TEST_ASSERT_EQUAL_INT(0, MMGR_CALL(memor.cmp, MemoriaCfg, .src = s, .other = "hello", .bytes = (size_t)5));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(9u, (size_t)(s - mem) + slen,
                                     "the address handed back is the position: base plus offset, plus the run");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(9u, r.pos, "the cursor moved past the length and its run together");
}

/**
 * @brief A run whose length reaches past the end leaves the cursor where it started.
 *
 * @note The length alone having been read does not count: a partial read is not a read, and a cursor
 *       left between the length and a run that is not there means nothing to whoever reads next.
 */
void test_a_length_prefix_promising_more_than_is_there_is_refused(void)
{
    mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = mem, .cap = sizeof store);

    MMGR_CALL(byteio.put_be, OctetusCfg, .write_span = &w, .value = (uint64_t)99u, .bytes = (size_t)4);

    mmgr_cspan r = MMGR_CALL(spat.cfrom, SpatiumCfg, .cbuf = mem, .cap = 8u);
    const uint8_t *s = (const uint8_t *)"untouched";
    size_t slen = 123u;

    TEST_ASSERT_FALSE(MMGR_CALL(byteio.rd_str, OctetusCfg, .read_span = &r, .blob = &s, .blob_bytes = &slen));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, r.pos, "the cursor went back to where it started");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(123u, slen, "and nothing was written through the outputs");
}

void test_an_integer_right_aligns_into_a_fixed_field(void)
{
    static const uint8_t narrow[3] = {0x00u, 0x12u, 0x34u};
    uint8_t field[8];
    mmgr_span f = MMGR_CALL(spat.from, SpatiumCfg, .buf = field, .cap = sizeof field);

    TEST_ASSERT_TRUE(MMGR_CALL(byteio.mpint_fixed, OctetusCfg, .write_span = &f, .src = narrow, .bytes = (size_t)3));

    // The leading zero is skipped, so the two bytes that carry the value land at the field's end
    for (unsigned i = 0; i < 6u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, field[i], "the front of the field is zero filled");
    }
    TEST_ASSERT_EQUAL_UINT8(0x12u, field[6]);
    TEST_ASSERT_EQUAL_UINT8(0x34u, field[7]);
}

void test_an_integer_wider_than_its_field_is_refused(void)
{
    static const uint8_t wide[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t field[2] = {0xAAu, 0xBBu};
    mmgr_span f = MMGR_CALL(spat.from, SpatiumCfg, .buf = field, .cap = sizeof field);

    TEST_ASSERT_FALSE(MMGR_CALL(byteio.mpint_fixed, OctetusCfg, .write_span = &f, .src = wide, .bytes = (size_t)4));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xAAu, field[0], "a refused integer writes nothing");
    TEST_ASSERT_EQUAL_UINT8(0xBBu, field[1]);
}

void test_raw_bytes_survive_an_unaligned_start(void)
{
    for (unsigned skew = 0; skew < 8u; skew++)
    {
        uint8_t buf[64];
        mmgr_span w = MMGR_CALL(spat.from, SpatiumCfg, .buf = buf + skew, .cap = 16u);

        MMGR_CALL(byteio.raw, OctetusCfg, .write_span = &w, .src = (const uint8_t *)"0123456789abcdef",
                  .bytes = (size_t)16);

        TEST_ASSERT_EQUAL_INT_MESSAGE(
            0, MMGR_CALL(memor.cmp, MemoriaCfg, .src = buf + skew, .other = "0123456789abcdef", .bytes = (size_t)16),
            "a bulk write must survive whatever alignment it starts at");
    }
}
