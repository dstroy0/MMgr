// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_bitorum_introitus_exitus_accuracy.c
 * @brief Checks which bit of a written value lands at which position in the buffer, against a
 *        reference that appends one bit at a time and packs them afterwards.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note The module merges a partial byte with the low bits of the next value and moves whole bytes
 *       from a 64-bit working copy. The reference holds a bit per array element and packs at the
 *       end, so the two share no shift, no mask and no idea of a byte boundary.
 * @note Bit order is the whole contract here. The header states least significant first, and a
 *       writer that reversed a byte's bits produces the same byte count, the same overflow point and
 *       the same length, which is why nothing below compares lengths alone.
 * @note Widths that straddle a byte are the interesting ones. A width of 8 and a residue of 0 keeps
 *       every bit in the byte it started in, and a defect in the merge never surfaces there, so the
 *       sequences below are built from widths that leave a residue.
 * @note Contract checks on a bit count above 64, on a null buffer and on a capacity of zero live in
 *       test_bitorum_introitus_exitus. This file asks where the bits go.
 */
#include <stdint.h>
#include <stdio.h>

#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"

#include "unity.h"

/**
 * @brief Expands to 4096u, the most bits any case here writes into one stream.
 *
 * @note The longest case is 64 appends of up to 64 bits. The reference holds one array element per
 *       bit, and this bounds that array.
 */
#define MMGR_ACCURACY_BITOR_MAX_BITS 4096u

/**
 * @brief Expands to 512u, the bytes MMGR_ACCURACY_BITOR_MAX_BITS pack into.
 *
 * @note Every buffer here carries a guard byte past this. A write that ran long is caught at the
 *       byte after the one it was allowed.
 */
#define MMGR_ACCURACY_BITOR_MAX_BYTES (MMGR_ACCURACY_BITOR_MAX_BITS / 8u)

/**
 * @brief Expands to 0xA5, the byte every destination is filled with before a write.
 *
 * @note Neither 0x00 nor 0xFF. A byte the writer left alone is then distinguishable from one it
 *       wrote, which is what makes the untouched tail checkable.
 */
#define MMGR_ACCURACY_BITOR_GUARD 0xA5u

/**
 * @brief A stream of single bits, in the order they were appended.
 *
 * @note One array element per bit is the point. Nothing here packs until the comparison, so the
 *       reference never forms a byte and cannot share a byte-boundary defect with the module.
 */
typedef struct
{
    uint8_t bit[MMGR_ACCURACY_BITOR_MAX_BITS]; /**< One bit per element, 0 or 1. */
    size_t count;                              /**< Bits appended so far. */
} AccuracyBitStream;

/**
 * @brief Appends the low width bits of a value to a bit stream, least significant first.
 *
 * @param[in,out] stream Stream to append to [BORROWS].
 * @param[in]     value  Value whose low bits are appended.
 * @param[in]     width  How many of its bits to append, 0 through 64.
 * @note This is the whole reference for what a put contributes. The header states least significant
 *       first, and that ordering is the loop below and nothing else.
 * @note A width of 0 appends nothing, which is what a put of no bits is meant to do.
 * @warning The caller keeps the total under MMGR_ACCURACY_BITOR_MAX_BITS. Nothing here bounds it.
 */
static void accuracy_stream_append(AccuracyBitStream *stream, uint64_t value, unsigned width)
{
    for (unsigned position = 0u; position < width; position++)
    {
        // Explicit cast narrows the extracted bit to the uint8_t element holding it. The mask keeps
        // one bit, which is what the element carries
        stream->bit[stream->count] = (uint8_t)((value >> position) & 1u);
        stream->count++;
    }
}

/**
 * @brief Packs a bit stream into bytes, the first bit appended landing at the low bit of byte zero.
 *
 * @param[in]  stream Stream to pack [BORROWS].
 * @param[out] packed Destination for the packed bytes [BORROWS].
 * @return            Bytes written, counting a partial last byte as one.
 * @note The layout is the one the header describes: bit n of the stream is bit n modulo 8 of byte n
 *       over 8. A partial last byte is padded with zeros above its bits, which is what align writes.
 * @note Called after every stream is complete. Packing at the end is what keeps the reference from
 *       carrying the residue the module carries.
 * @warning packed must be writable for the byte count this returns.
 */
static size_t accuracy_stream_pack(const AccuracyBitStream *stream, uint8_t *packed)
{
    const size_t bytes = (stream->count + 7u) / 8u;

    for (size_t index = 0u; index < bytes; index++)
    {
        packed[index] = 0u;
    }
    for (size_t index = 0u; index < stream->count; index++)
    {
        // Explicit cast narrows the placed bit to the uint8_t the buffer holds. The shift is under 8
        packed[index / 8u] |= (uint8_t)(stream->bit[index] << (index % 8u));
    }
    return bytes;
}

/**
 * @brief Returns the whole bytes a stream has filled, leaving any partial byte out.
 *
 * @param[in] stream Stream to measure [BORROWS].
 * @return           Bits appended, over eight.
 * @note This is what bytes_written is expected to hold before align runs. put writes whole bytes and
 *       nothing else, and the bits past the last whole byte are the residue.
 */
static size_t accuracy_stream_whole_bytes(const AccuracyBitStream *stream)
{
    return stream->count / 8u;
}

/**
 * @brief Advances a deterministic pattern and returns the next 64-bit value.
 *
 * @param[in,out] state Generator state [BORROWS].
 * @return              The advanced state.
 * @note A fixed recurrence. A failure reproduces on the next run with the same values in the same
 *       order, and nothing here is seeded from the clock.
 * @note The multiplier and increment are the ones Knuth gives for a 64-bit linear congruential
 *       sequence. The values only have to be varied and repeatable, and no statistical property of
 *       them is relied on.
 */
static uint64_t accuracy_next_pattern(uint64_t *state)
{
    *state = (*state * UINT64_C(6364136223846793005)) + UINT64_C(1442695040888963407);
    return *state;
}

/**
 * @brief Fills a buffer with the guard byte.
 *
 * @param[out] buffer Bytes to fill [BORROWS].
 * @param[in]  bytes  How many to fill.
 * @note Every case starts here. A byte the writer did not reach still holds the guard, which is what
 *       catches a write that ran past the bytes it accounted for.
 */
static void accuracy_fill_guard(uint8_t *buffer, size_t bytes)
{
    for (size_t index = 0u; index < bytes; index++)
    {
        buffer[index] = MMGR_ACCURACY_BITOR_GUARD;
    }
}

/**
 * @brief Compares a written buffer against the packed reference, byte by byte.
 *
 * @param[in] produced Bytes the module wrote [BORROWS].
 * @param[in] expected Bytes the reference packed [BORROWS].
 * @param[in] bytes    How many to compare.
 * @param[in] label    Text naming the case, printed when a byte disagrees.
 * @note Reports the first byte that disagrees along with both values, since a bit order defect moves
 *       every byte and a single mismatch is enough to name it.
 */
static void accuracy_expect_bytes(const uint8_t *produced, const uint8_t *expected, size_t bytes, const char *label)
{
    for (size_t index = 0u; index < bytes; index++)
    {
        char message[128];

        (void)snprintf(message, sizeof message, "%s: byte %u holds the wrong bits", label, (unsigned)index);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(expected[index], produced[index], message);
    }
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every writer and buffer here has automatic storage inside the case that builds it, and there
 *       is no shared state to prepare.
 */
void setUp(void)
{
}

/**
 * @brief Runs after each Unity test case.
 *
 * @note Required alongside setUp, since the generated runner calls both around every case.
 * @note Nothing here allocates, so there is nothing to release.
 */
void tearDown(void)
{
}

/**
 * @brief Checks the packing this suite rests on against bytes worked out by hand.
 *
 * @note Exists to catch a defect in the reference as itself. Without this case a broken
 *       accuracy_stream_append or accuracy_stream_pack would surface as a writer mismatch, and the
 *       module would be blamed for it.
 * @note Three appends of 3 and 2 bits give the bits 1, 0, 1, 1, 1 in order. Packed least significant
 *       first that is 1 + 4 + 8 + 16, which is 0x1D.
 * @note A twelve bit value spans two bytes. 0xABC packs as 0xBC then 0x0A, with the high nibble of
 *       the second byte left zero, and those two bytes are the padding align is meant to produce.
 */
void test_the_exact_packing_this_suite_relies_on_is_itself_right(void)
{
    AccuracyBitStream stream = {{0u}, 0u};
    uint8_t packed[MMGR_ACCURACY_BITOR_MAX_BYTES];

    accuracy_stream_append(&stream, 0x5u, 3u);
    accuracy_stream_append(&stream, 0x3u, 2u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(5u, stream.count, "five bits were appended");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, accuracy_stream_pack(&stream, packed), "five bits pack into one byte");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x1Du, packed[0], "the five bits did not pack least significant first");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, accuracy_stream_whole_bytes(&stream), "five bits fill no whole byte");

    stream.count = 0u;
    accuracy_stream_append(&stream, 0xABCu, 12u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, accuracy_stream_pack(&stream, packed), "twelve bits pack into two bytes");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xBCu, packed[0], "the low byte of the twelve bit value is wrong");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x0Au, packed[1], "the high nibble is not padded with zeros above it");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, accuracy_stream_whole_bytes(&stream), "twelve bits fill one whole byte");
}

/**
 * @brief Checks a single put of every width from 1 to 64 against the packed reference.
 *
 * @note Each width is written twice, once from an all ones value and once from a pattern whose bits
 *       differ along its length. An all ones value passes a writer that reversed a byte, and the
 *       pattern is what catches that.
 * @note The value is masked to the width before the reference sees it, since put is documented to
 *       take the low bits and the bits above the width are not part of what was written.
 * @note bytes_written is checked before align and again after, so the whole byte count and the
 *       residue are both accounted for.
 */
void test_a_single_put_of_every_width_lands_where_the_reference_puts_it(void)
{
    static const uint64_t source_of[] = {UINT64_MAX, UINT64_C(0x0123456789ABCDEF)};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in. The array is two
    // entries, far inside what an unsigned carries
    const unsigned source_count = (unsigned)(sizeof source_of / sizeof source_of[0]);

    for (unsigned source_index = 0u; source_index < source_count; source_index++)
    {
        for (unsigned width = 1u; width <= 64u; width++)
        {
            // A width of 64 would shift a 64-bit value by its full width, which C leaves undefined,
            // so the all ones mask is written out instead of computed
            const uint64_t mask = (width == 64u) ? UINT64_MAX : ((UINT64_C(1) << width) - 1u);
            const uint64_t value = source_of[source_index] & mask;
            AccuracyBitStream stream = {{0u}, 0u};
            uint8_t expected[MMGR_ACCURACY_BITOR_MAX_BYTES];
            uint8_t produced[MMGR_ACCURACY_BITOR_MAX_BYTES];
            char label[64];

            accuracy_stream_append(&stream, value, width);
            accuracy_fill_guard(produced, sizeof produced);

            mmgr_bitor writer = EMBED_CALL(bitio.init, BitorumCfg, .out = produced, .cap = sizeof produced);

            EMBED_CALL(bitio.put, BitorumCfg, .writer = &writer, .val = value, .bit_count = width);

            // Packed once and read twice. The stream gains no bit between the two comparisons, and
            // the whole byte count is a prefix of what align finishes
            const size_t total = accuracy_stream_pack(&stream, expected);

            (void)snprintf(label, sizeof label, "a put of %u bits before align", width);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(accuracy_stream_whole_bytes(&stream), writer.bytes_written, label);
            accuracy_expect_bytes(produced, expected, accuracy_stream_whole_bytes(&stream), label);

            EMBED_CALL(bitio.align, BitorumCfg, .writer = &writer);

            (void)snprintf(label, sizeof label, "a put of %u bits after align", width);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(total, writer.bytes_written, label);
            accuracy_expect_bytes(produced, expected, total, label);
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_BITOR_GUARD, produced[total],
                                           "the writer put bits past the byte it reported");
        }
    }
}

/**
 * @brief Checks long runs of puts at widths that leave a residue against the packed reference.
 *
 * @note This is the case the file exists for. A bit that crosses a byte boundary is merged by the
 *       module out of a residue and a fresh value, and that merge is the one piece of arithmetic
 *       nothing else here exercises.
 * @note The widths run 1 through 17 and repeat, which never settles on a multiple of eight. A run of
 *       widths that all divide eight keeps the residue empty and the merge unused.
 * @note Sixty four appends a run and thirty two runs, so the merge is exercised at every residue
 *       length from 0 to 7 many times over, at values that differ every time.
 */
void test_a_run_of_puts_across_byte_boundaries_lands_where_the_reference_puts_it(void)
{
    uint64_t state = UINT64_C(0x0123456789ABCDEF);

    for (unsigned run = 0u; run < 32u; run++)
    {
        AccuracyBitStream stream = {{0u}, 0u};
        uint8_t expected[MMGR_ACCURACY_BITOR_MAX_BYTES];
        uint8_t produced[MMGR_ACCURACY_BITOR_MAX_BYTES];
        char label[64];

        accuracy_fill_guard(produced, sizeof produced);

        mmgr_bitor writer = EMBED_CALL(bitio.init, BitorumCfg, .out = produced, .cap = sizeof produced);

        for (unsigned step = 0u; step < 64u; step++)
        {
            const unsigned width = (step % 17u) + 1u;
            const uint64_t mask = (UINT64_C(1) << width) - 1u;
            const uint64_t value = accuracy_next_pattern(&state) & mask;

            accuracy_stream_append(&stream, value, width);
            EMBED_CALL(bitio.put, BitorumCfg, .writer = &writer, .val = value, .bit_count = width);
            (void)accuracy_stream_pack(&stream, expected);

            (void)snprintf(label, sizeof label, "run %u, step %u", run, step);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(accuracy_stream_whole_bytes(&stream), writer.bytes_written, label);
            accuracy_expect_bytes(produced, expected, accuracy_stream_whole_bytes(&stream), label);
        }

        EMBED_CALL(bitio.align, BitorumCfg, .writer = &writer);

        const size_t total = accuracy_stream_pack(&stream, expected);

        (void)snprintf(label, sizeof label, "run %u after align", run);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(total, writer.bytes_written, label);
        accuracy_expect_bytes(produced, expected, total, label);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_BITOR_GUARD, produced[total],
                                       "the writer put bits past the byte it reported");
    }
}

/**
 * @brief Checks that the bits read back out of the buffer are the values that were written.
 *
 * @note The cases above compare packed bytes. This one goes the other way and takes the buffer apart
 *       a bit at a time to rebuild each value, which is what a reader of this format would do.
 * @note A pair of defects that cancel in the packing comparison, such as a reversal applied on the
 *       way in and undone by the reference, still fails here, because the values are the ones the
 *       case handed the writer and nothing derived them again.
 */
void test_the_bits_read_back_out_rebuild_the_values_that_were_written(void)
{
    static const unsigned width_of[] = {1u, 3u, 5u, 7u, 9u, 13u, 17u, 31u, 33u, 64u};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned width_count = (unsigned)(sizeof width_of / sizeof width_of[0]);
    uint64_t written_of[sizeof width_of / sizeof width_of[0]];
    uint8_t produced[MMGR_ACCURACY_BITOR_MAX_BYTES];
    uint64_t state = UINT64_C(0xFEDCBA9876543210);
    size_t position = 0u;

    accuracy_fill_guard(produced, sizeof produced);

    mmgr_bitor writer = EMBED_CALL(bitio.init, BitorumCfg, .out = produced, .cap = sizeof produced);

    for (unsigned index = 0u; index < width_count; index++)
    {
        const uint64_t mask = (width_of[index] == 64u) ? UINT64_MAX : ((UINT64_C(1) << width_of[index]) - 1u);

        written_of[index] = accuracy_next_pattern(&state) & mask;
        EMBED_CALL(bitio.put, BitorumCfg, .writer = &writer, .val = written_of[index], .bit_count = width_of[index]);
    }
    EMBED_CALL(bitio.align, BitorumCfg, .writer = &writer);

    for (unsigned index = 0u; index < width_count; index++)
    {
        uint64_t rebuilt = 0u;
        char message[96];

        for (unsigned bit = 0u; bit < width_of[index]; bit++)
        {
            // Explicit cast widens the extracted bit to the uint64_t the value is rebuilt in, and
            // the shift puts it back at the position it was written from
            rebuilt |= (uint64_t)((produced[position / 8u] >> (position % 8u)) & 1u) << bit;
            position++;
        }

        (void)snprintf(message, sizeof message, "the value written at %u bits did not come back", width_of[index]);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(written_of[index], rebuilt, message);
    }
}

/**
 * @brief Checks that align pads above the residue with zeros and writes nothing on a second call.
 *
 * @note The padding is the one part of the output no caller supplied, and a writer that padded with
 *       ones produces a byte a reader cannot tell from data.
 * @note Every residue length from 1 to 7 is covered by writing that many one bits. All ones below
 *       the padding makes a padding bit that leaked through visible as a set bit above them.
 * @note The second align is checked to move nothing, since a stream that called it twice would
 *       otherwise gain a byte of zeros.
 */
void test_align_pads_the_last_byte_with_zeros_above_the_bits_written(void)
{
    for (unsigned held = 1u; held < 8u; held++)
    {
        const uint64_t all_ones = (UINT64_C(1) << held) - 1u;
        // The bits written are all ones, so the byte is exactly the low held bits set and the rest
        // clear. Explicit cast narrows that to the uint8_t the buffer holds
        const uint8_t expected = (uint8_t)all_ones;
        uint8_t produced[MMGR_ACCURACY_BITOR_MAX_BYTES];
        char message[96];

        accuracy_fill_guard(produced, sizeof produced);

        mmgr_bitor writer = EMBED_CALL(bitio.init, BitorumCfg, .out = produced, .cap = sizeof produced);

        EMBED_CALL(bitio.put, BitorumCfg, .writer = &writer, .val = all_ones, .bit_count = held);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, writer.bytes_written, "a partial byte was written before align");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_BITOR_GUARD, produced[0],
                                       "the residue reached the buffer before align");

        EMBED_CALL(bitio.align, BitorumCfg, .writer = &writer);
        (void)snprintf(message, sizeof message, "align did not pad %u held bits with zeros", held);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, writer.bytes_written, "align did not write the partial byte");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(expected, produced[0], message);

        EMBED_CALL(bitio.align, BitorumCfg, .writer = &writer);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, writer.bytes_written, "a second align wrote another byte");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_BITOR_GUARD, produced[1],
                                       "a second align put a byte past the first");
    }
}

/**
 * @brief Checks that a put of no bits moves nothing.
 *
 * @note A width of 0 is the boundary of the append loop, and a writer that treated it as one bit
 *       would shift every value after it by a position. Every later byte then differs, which makes
 *       this the cheapest case here and one of the more destructive defects.
 * @note Checked from an empty writer and again from one holding a residue, since the two take
 *       different paths through the byte count.
 */
void test_a_put_of_no_bits_moves_nothing(void)
{
    uint8_t produced[MMGR_ACCURACY_BITOR_MAX_BYTES];

    accuracy_fill_guard(produced, sizeof produced);

    mmgr_bitor writer = EMBED_CALL(bitio.init, BitorumCfg, .out = produced, .cap = sizeof produced);

    EMBED_CALL(bitio.put, BitorumCfg, .writer = &writer, .val = UINT64_MAX, .bit_count = 0u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, writer.bytes_written, "a put of no bits wrote a byte");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_BITOR_GUARD, produced[0], "a put of no bits reached the buffer");

    EMBED_CALL(bitio.put, BitorumCfg, .writer = &writer, .val = 0x5u, .bit_count = 3u);
    EMBED_CALL(bitio.put, BitorumCfg, .writer = &writer, .val = UINT64_MAX, .bit_count = 0u);
    EMBED_CALL(bitio.put, BitorumCfg, .writer = &writer, .val = 0x3u, .bit_count = 2u);
    EMBED_CALL(bitio.align, BitorumCfg, .writer = &writer);

    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, writer.bytes_written, "the stream is five bits and fills one byte");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x1Du, produced[0], "a put of no bits displaced the bits around it");
}
