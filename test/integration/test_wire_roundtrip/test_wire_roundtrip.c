// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// byteio -> spatium -> endian -> proximus_operor
//
// Writing a frame and reading it back. Each module has its own unit suite; what this asserts is
// that the writer's idea of a field and the reader's idea of the same field are one idea, at every
// width and across the alignment the span happens to land on.
#include "unity.h"

#include "byteio/byteio.h"
#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"
#include "spatium/spatium.h"

void test_a_byte_written_is_the_byte_read(void)
{
    uint8_t mem[16];
    mmgr_spat w = spat.from(mem, sizeof mem);

    byteio.put(&w, 0xA5u);
    byteio.put(&w, 0x5Au);
    TEST_ASSERT_TRUE(spat.ok(w));
    TEST_ASSERT_EQUAL_size_t(2u, spat.len(w));
    TEST_ASSERT_EQUAL_UINT8(0xA5u, mem[0]);
    TEST_ASSERT_EQUAL_UINT8(0x5Au, mem[1]);
}

void test_big_endian_fields_round_trip_at_every_width(void)
{
    static const struct
    {
        uint64_t v;
        int32_t n;
    } cases[] = {
        {0x00u, 1},     {0xFFu, 1},       {0x1234u, 2},     {0xFFFFu, 2},
        {0x123456u, 3}, {0x12345678u, 4}, {0xFFFFFFFFu, 4}, {0x123456789ABCDEF0ull, 8},
    };

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        uint8_t mem[16];
        mmgr_spat w = spat.from(mem, sizeof mem);
        byteio.put_be(&w, cases[i].v, cases[i].n);
        TEST_ASSERT_TRUE(spat.ok(w));
        TEST_ASSERT_EQUAL_size_t((size_t)cases[i].n, spat.len(w));

        mmgr_fspat r = spat.produced(w);
        uint64_t got = 0;
        TEST_ASSERT_TRUE(byteio.take_be(&r, (size_t)cases[i].n, &got));
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(cases[i].v, got, "a field read back must be the field written");
    }
}

void test_the_writer_puts_the_high_byte_first(void)
{
    uint8_t mem[8];
    mmgr_spat w = spat.from(mem, sizeof mem);
    byteio.put_be(&w, 0x11223344u, 4);

    // big endian on the wire, whatever this host does internally
    TEST_ASSERT_EQUAL_UINT8(0x11u, mem[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22u, mem[1]);
    TEST_ASSERT_EQUAL_UINT8(0x33u, mem[2]);
    TEST_ASSERT_EQUAL_UINT8(0x44u, mem[3]);
}

void test_endian_entries_agree_with_the_wire_writer(void)
{
    uint8_t viabyteio[8];
    uint8_t viaendian[8];
    mmgr_spat w = spat.from(viabyteio, sizeof viabyteio);

    byteio.put_be(&w, 0xDEADBEEFu, 4);
    endian.wr32be(viaendian, 0xDEADBEEFu);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, memor.cmp(viabyteio, viaendian, 4u),
                                  "two ways of writing the same field must produce the same bytes");

    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, endian.rd32be(viabyteio));
}

void test_a_short_read_is_refused_and_latches(void)
{
    uint8_t mem[2] = {0x01u, 0x02u};
    mmgr_fspat r = spat.cfrom(mem, sizeof mem);
    uint64_t got = 0;

    TEST_ASSERT_FALSE_MESSAGE(byteio.take_be(&r, 4u, &got), "four bytes are not there to read");
    TEST_ASSERT_FALSE_MESSAGE(spat.cok(r), "and the reader must say so afterwards");
}

void test_a_write_past_the_span_latches_and_cannot_be_read_back(void)
{
    uint8_t mem[4];
    mmgr_spat w = spat.from(mem, sizeof mem);

    byteio.put_be(&w, 0x11223344u, 4);
    TEST_ASSERT_TRUE(spat.ok(w));

    byteio.put(&w, 0xFFu);
    TEST_ASSERT_FALSE_MESSAGE(spat.ok(w), "the span is full and must report it");

    // len counts what was asked for, not what fit - the snprintf contract, so a caller can size a
    // buffer from a failed pass. That is only safe because everything that could act on it either
    // checks ok() or clamps, which is what the rest of this case pins.
    TEST_ASSERT_EQUAL_size_t_MESSAGE(5u, spat.len(w), "len reports what was needed once overflow has latched");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, spat.room(w), "room must clamp, not underflow past the end");

    mmgr_fspat r = spat.produced(w);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, r.len, "a reader over an overflowed span must be empty, not 5 bytes long");
    TEST_ASSERT_NULL(r.buf);

    TEST_ASSERT_LESS_OR_EQUAL_size_t_MESSAGE(sizeof mem, spat.len(spat.first(w, 99u)) + spat.room(spat.first(w, 99u)),
                                             "first() must clamp to the buffer");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x44u, mem[3], "the last byte that fit is intact");
}

void test_a_length_prefixed_string_round_trips(void)
{
    uint8_t mem[32];
    mmgr_spat w = spat.from(mem, sizeof mem);

    byteio.put_be(&w, 5u, 4);
    byteio.raw(&w, "hello", 5u);
    TEST_ASSERT_TRUE(spat.ok(w));

    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 0;
    TEST_ASSERT_TRUE(byteio.rd_str(mem, spat.len(w), &off, &s, &slen));
    TEST_ASSERT_EQUAL_UINT32(5u, slen);
    TEST_ASSERT_EQUAL_INT(0, memor.cmp(s, "hello", 5u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(9u, off, "the cursor must have advanced past prefix and payload");
}

void test_raw_bytes_survive_an_unaligned_start(void)
{
    // the writer lands wherever the previous field left it, so the bulk path has to work unaligned
    for (unsigned skew = 0; skew < 8u; skew++)
    {
        uint8_t mem[64];
        mmgr_spat w = spat.from(mem, sizeof mem);
        for (unsigned i = 0; i < skew; i++)
        {
            byteio.put(&w, 0u);
        }
        byteio.raw(&w, "0123456789abcdef", 16u);
        TEST_ASSERT_TRUE(spat.ok(w));
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, memor.cmp(mem + skew, "0123456789abcdef", 16u),
                                      "a bulk write must survive whatever alignment it starts at");
    }
}
