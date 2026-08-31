#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"

#include "unity.h"

static uint8_t out[16];
static mmgr_bitor w;

void setUp(void)
{
    for (uint32_t i = 0; i < sizeof out; i++)
    {
        out[i] = 0xAAu;
    }
    w = EMBED_CALL(bitio.init, BitorumCfg, .out = out, .cap = sizeof out);
}

void tearDown(void)
{
}

static void put_bits(uint64_t val, embed_word bit_count)
{
    EMBED_CALL(bitio.put, BitorumCfg, .writer = &w, .val = val, .bit_count = bit_count);
}

void test_bitio_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("bitio.h compiled with no header before it");
}

void test_init_hands_back_an_empty_writer(void)
{
    TEST_ASSERT_EQUAL_PTR(out, w.out);
    TEST_ASSERT_EQUAL_size_t(sizeof out, w.cap);
    TEST_ASSERT_EQUAL_size_t(0u, w.bytes_written);
    TEST_ASSERT_EQUAL_size_t(0u, w.bit_count);
    TEST_ASSERT_FALSE(w.overflow);
}

void test_a_whole_byte_lands_as_that_byte(void)
{
    put_bits(0xC3u, 8);
    TEST_ASSERT_EQUAL_size_t(1u, w.bytes_written);
    TEST_ASSERT_EQUAL_HEX8(0xC3u, out[0]);
    TEST_ASSERT_FALSE(w.overflow);
}

void test_bits_pack_from_the_low_end(void)
{
    put_bits(0x1u, 1);
    put_bits(0x0u, 1);
    put_bits(0x3u, 2);
    put_bits(0x0u, 4);
    TEST_ASSERT_EQUAL_size_t(1u, w.bytes_written);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x0Du, out[0], "1 then 0 then 11 packs as 0b00001101");
}

void test_a_partial_write_waits_for_the_byte(void)
{
    put_bits(0x5u, 3);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, w.bytes_written, "three bits do not make a byte");
    TEST_ASSERT_EQUAL_size_t(3u, w.bit_count);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xAAu, out[0], "nothing written yet");
}

void test_padding_to_the_byte_writes_it(void)
{
    put_bits(0x5u, 3);
    put_bits(0x0u, 5);
    TEST_ASSERT_EQUAL_size_t(1u, w.bytes_written);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x05u, out[0], "the fragment sits in the low bits, zeros above");
    TEST_ASSERT_EQUAL_size_t(0u, w.bit_count);
}

void test_a_put_of_no_bits_writes_nothing(void)
{
    put_bits(0xFFu, 8);
    const size_t before = w.bytes_written;
    put_bits(0x0u, 0);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, w.bytes_written, "no bits complete no byte");
}

void test_a_put_of_no_bits_on_an_empty_writer_writes_nothing(void)
{
    put_bits(0x0u, 0);
    TEST_ASSERT_EQUAL_size_t(0u, w.bytes_written);
    TEST_ASSERT_FALSE(w.overflow);
}

void test_a_wide_put_spans_bytes(void)
{
    put_bits(0xDEADBEEFu, 32);
    TEST_ASSERT_EQUAL_size_t(4u, w.bytes_written);
    TEST_ASSERT_EQUAL_HEX8(0xEFu, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBEu, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0xADu, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0xDEu, out[3]);
}

void test_a_wide_put_onto_a_fragment_keeps_every_bit(void)
{
    put_bits(0x5u, 3);
    put_bits(0xDEADBEEFu, 32);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, w.bytes_written, "35 bits is four whole bytes and three left over");
    TEST_ASSERT_EQUAL_size_t(3u, w.bit_count);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x7Du, out[0], "the fragment keeps the low three bits");
    TEST_ASSERT_EQUAL_HEX8(0xF7u, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x6Du, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0xF5u, out[3]);
}

void test_n_at_or_above_32_takes_the_value_whole(void)
{
    put_bits(0xFFFFFFFFu, 32);
    TEST_ASSERT_EQUAL_size_t(4u, w.bytes_written);
    for (uint32_t i = 0; i < 4u; i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0xFFu, out[i]);
    }
}

void test_a_narrow_put_ignores_the_high_bits(void)
{
    put_bits(0xFFu, 4);
    put_bits(0x0u, 4);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x0Fu, out[0], "only the low n bits of the value are used");
}

void test_overflow_latches_and_stops_writing(void)
{
    w = EMBED_CALL(bitio.init, BitorumCfg, .out = out, .cap = 2u);

    put_bits(0x11u, 8);
    put_bits(0x22u, 8);
    TEST_ASSERT_FALSE(w.overflow);
    TEST_ASSERT_EQUAL_size_t(2u, w.bytes_written);

    put_bits(0x33u, 8);
    TEST_ASSERT_TRUE_MESSAGE(w.overflow, "a third byte does not fit");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, w.bytes_written, "and must not have been written");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xAAu, out[2], "the byte past the cap is untouched");
}

void test_a_put_after_overflow_is_ignored(void)
{
    w = EMBED_CALL(bitio.init, BitorumCfg, .out = out, .cap = 1u);

    put_bits(0x11u, 8);
    put_bits(0x22u, 8);
    TEST_ASSERT_TRUE(w.overflow);

    const size_t before = w.bytes_written;
    put_bits(0x44u, 8);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(before, w.bytes_written, "overflow latches, so later puts do nothing");
}

void test_a_completed_byte_with_no_room_overflows(void)
{
    w = EMBED_CALL(bitio.init, BitorumCfg, .out = out, .cap = 1u);

    put_bits(0xFFu, 8);
    TEST_ASSERT_EQUAL_size_t(1u, w.bytes_written);
    put_bits(0x1u, 3);
    TEST_ASSERT_FALSE_MESSAGE(w.overflow, "three bits complete no byte, so nothing needed room");
    put_bits(0x0u, 5);
    TEST_ASSERT_TRUE_MESSAGE(w.overflow, "the byte they complete has nowhere to go");
}

void test_two_writers_do_not_share_state(void)
{
    static uint8_t other[4];
    mmgr_bitor second = EMBED_CALL(bitio.init, BitorumCfg, .out = other, .cap = sizeof other);

    put_bits(0xC3u, 8);
    EMBED_CALL(bitio.put, BitorumCfg, .writer = &second, .val = 0x5Au, .bit_count = 8);

    TEST_ASSERT_EQUAL_HEX8(0xC3u, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x5Au, other[0]);
    TEST_ASSERT_EQUAL_size_t(1u, w.bytes_written);
    TEST_ASSERT_EQUAL_size_t(1u, second.bytes_written);
}

void test_namespace_is_wired(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_bitor_put, bitio.put);
    TEST_ASSERT_EQUAL_PTR(mmgr_bitor_init, bitio.init);
}

void test_align_writes_the_partial_byte(void)
{
    put_bits(0x5u, 4u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, w.bytes_written, "four bits do not fill a byte, so put writes none");

    EMBED_CALL(bitio.align, BitorumCfg, .writer = &w);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, w.bytes_written, "align is what writes them");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x05u, out[0], "the bits sit low with zero padding above them");
    TEST_ASSERT_EQUAL_size_t(0u, w.bit_count);
    TEST_ASSERT_FALSE(w.overflow);
}

void test_align_on_a_byte_boundary_writes_nothing(void)
{
    put_bits(0xA5u, 8u);
    TEST_ASSERT_EQUAL_size_t(1u, w.bytes_written);

    EMBED_CALL(bitio.align, BitorumCfg, .writer = &w);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, w.bytes_written, "there was no residue, so nothing was written");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xAAu, out[1], "and the byte past the stream was not touched");
}

void test_align_twice_is_the_same_as_once(void)
{
    put_bits(0x3u, 2u);
    EMBED_CALL(bitio.align, BitorumCfg, .writer = &w);
    EMBED_CALL(bitio.align, BitorumCfg, .writer = &w);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, w.bytes_written, "the second align had no residue to write");
    TEST_ASSERT_EQUAL_HEX8(0x03u, out[0]);
}

void test_a_stream_of_odd_length_round_trips(void)
{

    put_bits(0x5u, 3u);
    put_bits(0x1Au, 5u);
    put_bits(0x9u, 4u);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, w.bytes_written, "twelve bits fill one byte and leave four over");
    EMBED_CALL(bitio.align, BitorumCfg, .writer = &w);
    TEST_ASSERT_EQUAL_size_t(2u, w.bytes_written);

    TEST_ASSERT_EQUAL_HEX8((uint8_t)(0x5u | (0x1Au << 3)), out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x09u, out[1]);
}

void test_align_on_a_full_buffer_overflows_rather_than_writing(void)
{
    for (size_t i = 0; i < sizeof out; i++)
    {
        put_bits(0xFFu, 8u);
    }
    TEST_ASSERT_EQUAL_size_t(sizeof out, w.bytes_written);
    TEST_ASSERT_FALSE(w.overflow);

    put_bits(0x1u, 1u);
    EMBED_CALL(bitio.align, BitorumCfg, .writer = &w);

    TEST_ASSERT_TRUE_MESSAGE(w.overflow, "there was no room for the partial byte");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof out, w.bytes_written, "and nothing was written past the buffer");
}

void test_the_align_entry_is_wired(void)
{
    TEST_ASSERT_EQUAL_PTR(mmgr_bitor_align, bitio.align);
}
