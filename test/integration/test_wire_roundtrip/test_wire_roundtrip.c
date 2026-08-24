#include "unity.h"

#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"
#include "cellularum_laboro/cellularum_laboro.h"

void test_a_byte_written_is_the_byte_read(void)
{
    uint64_t store[2] = {0, 0};
    uint8_t *mem = (uint8_t *)store;
    uint64_t got = 0;

    MMGR_CALL(byteio.put, OctetusCfg, .at = mem, .val = (uint64_t)0xA5u, .bytes = (size_t)1);
    MMGR_CALL(byteio.put, OctetusCfg, .at = mem + 8, .val = (uint64_t)0x5Au, .bytes = (size_t)1);
    TEST_ASSERT_EQUAL_UINT8(0xA5u, mem[0]);
    TEST_ASSERT_EQUAL_UINT8(0x5Au, mem[8]);

    MMGR_CALL(byteio.take, OctetusCfg, .from = mem, .out = &got, .bytes = (size_t)1);
    TEST_ASSERT_EQUAL_HEX64(0xA5ull, got);
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
        uint64_t store[1] = {0};
        uint8_t *mem = (uint8_t *)store;
        uint64_t got = 0;

        MMGR_CALL(byteio.put, OctetusCfg, .at = mem, .val = cases[i].v, .bytes = (size_t)cases[i].n);
        MMGR_CALL(byteio.take, OctetusCfg, .from = mem, .out = &got, .bytes = (size_t)cases[i].n);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(cases[i].v, got, "a field read back must be the field written");
    }
}

void test_the_writer_puts_the_high_byte_first(void)
{
    uint64_t store[1] = {0};
    uint8_t *mem = (uint8_t *)store;

    MMGR_CALL(byteio.put, OctetusCfg, .at = mem, .val = (uint64_t)0x11223344u, .bytes = (size_t)4);

        TEST_ASSERT_EQUAL_UINT8(0x11u, mem[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22u, mem[1]);
    TEST_ASSERT_EQUAL_UINT8(0x33u, mem[2]);
    TEST_ASSERT_EQUAL_UINT8(0x44u, mem[3]);
}

void test_endian_entries_agree_with_the_wire_writer(void)
{
    uint64_t store[1] = {0};
    uint8_t *viabyteio = (uint8_t *)store;
    uint8_t viaendian[8];

    MMGR_CALL(byteio.put, OctetusCfg, .at = viabyteio, .val = (uint64_t)0xDEADBEEFu, .bytes = (size_t)4);
    magna_extremitas.wr(&(EndianCfg){viaendian, 0, 0xDEADBEEFu, MMGR_ENDIAN_32});
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, MMGR_CALL(memor.cmp, MemoriaCfg, .src = viabyteio, .other = viaendian, .bytes = (size_t)4),
                                  "two ways of writing the same field must produce the same bytes");

    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, (uint32_t)magna_extremitas.rd(&(EndianCfg){0, viabyteio, 0, MMGR_ENDIAN_32}));
}

void test_a_length_prefixed_string_round_trips(void)
{
    uint64_t store[4] = {0, 0, 0, 0};
    uint8_t *mem = (uint8_t *)store;

    MMGR_CALL(byteio.put, OctetusCfg, .at = mem, .val = (uint64_t)5u, .bytes = (size_t)4);
    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = mem + 4, .src = "hello", .bytes = (size_t)5);

    size_t off = 0;
    const uint8_t *s = NULL;
    uint32_t slen = 0;
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.rd_str, CatenaFinitaCfg, .src = mem, .cap = (size_t)9, .at = off, .out = &s, .slen = &slen));
    TEST_ASSERT_EQUAL_UINT32(5u, slen);
    TEST_ASSERT_EQUAL_INT(0, MMGR_CALL(memor.cmp, MemoriaCfg, .src = s, .other = "hello", .bytes = (size_t)5));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(9u, (size_t)(s - mem) + slen,
                                     "the address handed back is the position: base plus offset, plus the run");
}

void test_raw_bytes_survive_an_unaligned_start(void)
{
        for (unsigned skew = 0; skew < 8u; skew++)
    {
        uint8_t mem[64];

        MMGR_CALL(memor.cpy, MemoriaCfg, .dst = mem + skew, .src = "0123456789abcdef", .bytes = (size_t)16);
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, MMGR_CALL(memor.cmp, MemoriaCfg, .src = mem + skew, .other = "0123456789abcdef", .bytes = (size_t)16),
                                      "a bulk write must survive whatever alignment it starts at");
    }
}
