// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_octetus_introitus_exitus_accuracy.c
 * @brief Checks which byte of a value lands at which offset in a span, against a shift-per-byte
 *        reference, at every width from one to eight.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note The big endian entries reverse a value once and then store or load it at the widest step the
 *       count allows. A width of seven is three accesses of different sizes, where the reference is
 *       one shift per byte and reaches the same layout with no reversal and no step selection.
 * @note Byte order is the whole contract, and a width that stored its pieces in the wrong order still
 *       advances the cursor by the right amount and leaves a span that reports no failure. Only the
 *       bytes show it.
 * @note Widths that are not powers of two are the interesting ones. A width of eight is a single
 *       store and a width of seven is three, and the three have to land next to each other in the
 *       right order.
 * @note Nothing here overruns a fill span. byteio_claim asserts on that, and the checks environment
 *       traps a failed assertion, so overrunning a writer is test_octetus_introitus_exitus's to cover.
 *       The short read and the refused field are covered here, since neither asserts.
 * @note Contract checks live in test_octetus_introitus_exitus. This file asks where the bytes go.
 */
#include <stdint.h>
#include <stdio.h>

#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

#include "unity.h"

/**
 * @brief Expands to 64u, the bytes every span here covers.
 *
 * @note Room for several eight byte fields with space left over, so no case runs a writer past its
 *       end by accident.
 */
#define MMGR_ACCURACY_BYTEIO_BUFFER 64u

/**
 * @brief Expands to 0xA5, the byte a buffer is filled with before a write.
 *
 * @note A byte no case writes on purpose. A position still holding it is one the call did not reach.
 */
#define MMGR_ACCURACY_BYTEIO_GUARD 0xA5u

/**
 * @brief Returns the byte a big endian field of a given width carries at a given offset.
 *
 * @param[in] value  Value the field holds.
 * @param[in] width  Bytes in the field, 1 through 8.
 * @param[in] offset Offset within the field, 0 being the first byte written.
 * @return           That byte.
 * @note One shift and one mask. Most significant first means offset 0 holds the highest byte of the
 *       low width bytes, which is the shift below and nothing else.
 * @note This is the whole reference. It reverses nothing and selects no step width, which is what
 *       lets it disagree with the module.
 */
static uint8_t accuracy_big_endian_byte(uint64_t value, unsigned width, unsigned offset)
{
    // Explicit cast narrows the shifted value to the byte a field position holds. The mask keeps
    // eight bits, which is what a uint8_t carries
    return (uint8_t)((value >> (8u * (width - 1u - offset))) & 0xFFu);
}

/**
 * @brief Returns the value a run of bytes stands for, read most significant first.
 *
 * @param[in] at    First byte of the run [BORROWS].
 * @param[in] width Bytes in the run, 1 through 8.
 * @return          The value.
 * @note The inverse of accuracy_big_endian_byte, assembled a byte at a time. Used where the bytes are
 *       laid down by hand and the value is what the module has to arrive at.
 */
static uint64_t accuracy_big_endian_value(const uint8_t *at, unsigned width)
{
    uint64_t value = 0u;

    for (unsigned offset = 0u; offset < width; offset++)
    {
        // Explicit cast widens each byte to the uint64_t the value is assembled in before the shift
        value |= (uint64_t)at[offset] << (8u * (width - 1u - offset));
    }
    return value;
}

/**
 * @brief Fills a buffer with the guard byte.
 *
 * @param[out] buffer Bytes to fill [BORROWS].
 * @note Every case starts here, which tells a byte the call wrote from one it left alone.
 */
static void accuracy_fill_guard(uint8_t *buffer)
{
    for (size_t index = 0u; index < MMGR_ACCURACY_BYTEIO_BUFFER; index++)
    {
        buffer[index] = MMGR_ACCURACY_BYTEIO_GUARD;
    }
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every span and buffer here has automatic storage inside the case that builds it, and each
 *       case builds its own span, so there is no shared state to prepare.
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
 * @brief Checks the byte arithmetic this suite rests on against values worked out by hand.
 *
 * @note Exists to catch a defect in the reference as itself. A reference that put the bytes in the
 *       other order would report the module as wrong at every width, and the module would be blamed.
 * @note The expectations are literals a reader can check. 0x0102030405060708 at a width of eight
 *       holds 0x01 first and 0x08 last, and at a width of three holds only its low three bytes.
 * @note The two helpers are checked against each other as well, since one is meant to be the other's
 *       inverse and a pair that agreed while both were wrong would pass every case below.
 */
void test_the_byte_arithmetic_this_suite_relies_on_is_itself_right(void)
{
    static const uint8_t laid_out[8] = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u};

    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x01u, accuracy_big_endian_byte(UINT64_C(0x0102030405060708), 8u, 0u),
                                   "the first byte of a width of eight is not the highest");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x08u, accuracy_big_endian_byte(UINT64_C(0x0102030405060708), 8u, 7u),
                                   "the last byte of a width of eight is not the lowest");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x06u, accuracy_big_endian_byte(UINT64_C(0x0102030405060708), 3u, 0u),
                                   "a width of three does not start at the third byte from the bottom");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x08u, accuracy_big_endian_byte(UINT64_C(0x0102030405060708), 3u, 2u),
                                   "a width of three does not end at the lowest byte");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xEFu, accuracy_big_endian_byte(UINT64_C(0xEF), 1u, 0u),
                                   "a width of one is not the lowest byte");

    TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x0102030405060708), accuracy_big_endian_value(laid_out, 8u),
                                    "eight bytes do not assemble into the value they stand for");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x010203), accuracy_big_endian_value(laid_out, 3u),
                                    "three bytes do not assemble into the value they stand for");

    for (unsigned width = 1u; width <= 8u; width++)
    {
        // A width of eight would shift a 64-bit value by its full width, which C leaves undefined, so
        // the all ones mask is written out instead of computed
        const uint64_t mask = (width == 8u) ? UINT64_MAX : ((UINT64_C(1) << (8u * width)) - 1u);
        uint8_t rebuilt[8];
        char message[96];

        for (unsigned offset = 0u; offset < width; offset++)
        {
            rebuilt[offset] = accuracy_big_endian_byte(UINT64_C(0x0102030405060708), width, offset);
        }
        (void)snprintf(message, sizeof message, "the two helpers disagree at a width of %u", width);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x0102030405060708) & mask, accuracy_big_endian_value(rebuilt, width),
                                        message);
    }
}

/**
 * @brief Checks that a big endian append lays every byte where the reference puts it.
 *
 * @note This is the case the file exists for. Every width from one to eight is offered, at values
 *       whose bytes all differ. A step that stored its piece at the wrong offset is then visible as
 *       the wrong byte and not merely as a wrong total.
 * @note The cursor is checked to advance by the width, and the byte past the field is checked to be
 *       untouched, which fails a store wider than the count it was given.
 * @note Every value has eight different bytes or is an extreme. A value with repeated bytes passes a
 *       reversal that put them in the wrong places.
 */
void test_a_big_endian_append_lays_every_byte_where_the_reference_puts_it(void)
{
    static const uint64_t value_of[] = {UINT64_C(0x0102030405060708), UINT64_C(0xFEDCBA9876543210), 0uLL, UINT64_MAX,
                                        UINT64_C(0x00FF00FF00FF00FF)};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned index = 0u; index < value_count; index++)
    {
        for (unsigned width = 1u; width <= 8u; width++)
        {
            uint8_t buffer[MMGR_ACCURACY_BYTEIO_BUFFER];
            char message[128];

            accuracy_fill_guard(buffer);

            mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buffer, .cap = sizeof buffer);

            EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = &span, .value = value_of[index], .bytes = width);

            (void)snprintf(message, sizeof message, "a width of %u carrying 0x%016llX", width,
                           (unsigned long long)value_of[index]);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(width, span.pos, message);
            TEST_ASSERT_FALSE_MESSAGE(span.overflow, "a field inside the span latched overflow");

            for (unsigned offset = 0u; offset < width; offset++)
            {
                char byte_message[160];

                (void)snprintf(byte_message, sizeof byte_message, "%s: byte %u", message, offset);
                TEST_ASSERT_EQUAL_HEX8_MESSAGE(accuracy_big_endian_byte(value_of[index], width, offset), buffer[offset],
                                               byte_message);
            }
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_BYTEIO_GUARD, buffer[width],
                                           "the append reached past the width it was given");
        }
    }
}

/**
 * @brief Checks that a big endian read returns the value its bytes stand for.
 *
 * @note The bytes are laid down here one at a time and the expectation is assembled from them by the
 *       reference, so nothing about the value comes from the module's own gathering.
 * @note Every width is read from the same run of bytes, which is what shows the width bounds the read
 *       and the run does not.
 * @note The cursor is checked to advance by the width, and a read that succeeded is checked to leave
 *       the span usable.
 */
void test_a_big_endian_read_returns_the_value_its_bytes_stand_for(void)
{
    static const uint8_t laid_out[8] = {0x8Fu, 0x1Cu, 0x27u, 0x64u, 0xB3u, 0x05u, 0xDEu, 0x41u};

    for (unsigned width = 1u; width <= 8u; width++)
    {
        mmgr_cspan span = EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = laid_out, .cap = sizeof laid_out);
        uint64_t taken = 0u;
        char message[128];

        (void)snprintf(message, sizeof message, "a read of %u bytes", width);
        TEST_ASSERT_TRUE_MESSAGE(
            EMBED_CALL(byteio.take_be, OctetusCfg, .read_span = &span, .bytes = width, .out = &taken), message);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_big_endian_value(laid_out, width), taken, message);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(width, span.pos, message);
        TEST_ASSERT_FALSE_MESSAGE(span.err, "a read inside the span set the error flag");
    }
}

/**
 * @brief Checks that a value written and read back at the same width is the value that went in.
 *
 * @note The two cases above pin each direction against the reference. This one puts them together,
 *       because a pair that disagreed with each other would still lose a caller's value.
 * @note The value is masked to the width first. Only the low width bytes are written, and the rest
 *       are not part of what comes back.
 * @note A run of fields is written into one span and read back in order, so the cursor threading is
 *       exercised as well as the single field.
 */
void test_a_value_written_and_read_at_the_same_width_comes_back(void)
{
    static const unsigned width_of[] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    static const uint64_t value_of[] = {
        UINT64_C(0x0102030405060708), UINT64_C(0xFEDCBA9876543210), 1uLL, UINT64_MAX, 0uLL,
        UINT64_C(0x8000000000000000)};
    // Explicit casts narrow the sizeof quotients to the unsigned the loops count in
    const unsigned width_count = (unsigned)(sizeof width_of / sizeof width_of[0]);
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned value_index = 0u; value_index < value_count; value_index++)
    {
        uint8_t buffer[MMGR_ACCURACY_BYTEIO_BUFFER];
        mmgr_span span;

        accuracy_fill_guard(buffer);
        span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buffer, .cap = sizeof buffer);

        for (unsigned index = 0u; index < width_count; index++)
        {
            EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = &span, .value = value_of[value_index],
                       .bytes = width_of[index]);
        }

        mmgr_cspan reader = EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = buffer, .cap = span.pos);

        for (unsigned index = 0u; index < width_count; index++)
        {
            // A width of eight would shift a 64-bit value by its full width, which C leaves
            // undefined, so the all ones mask is written out instead of computed
            const uint64_t mask = (width_of[index] == 8u) ? UINT64_MAX : ((UINT64_C(1) << (8u * width_of[index])) - 1u);
            uint64_t taken = 0u;
            char message[128];

            (void)snprintf(message, sizeof message, "field %u of width %u carrying 0x%016llX", index, width_of[index],
                           (unsigned long long)value_of[value_index]);
            TEST_ASSERT_TRUE_MESSAGE(
                EMBED_CALL(byteio.take_be, OctetusCfg, .read_span = &reader, .bytes = width_of[index], .out = &taken),
                message);
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(value_of[value_index] & mask, taken, message);
        }
    }
}

/**
 * @brief Checks that a raw append copies its bytes unchanged and advances the cursor by the count.
 *
 * @note raw is the one append that changes nothing about the bytes, and the claim is exactly that.
 *       Every length from none to past a word is offered, since the copy underneath moves whole words
 *       and finishes the odd bytes separately.
 * @note Successive appends are checked to land end to end, which is what the cursor is for. A cursor
 *       that advanced by the wrong amount leaves a gap or an overlap between two runs.
 */
void test_a_raw_append_copies_its_bytes_unchanged(void)
{
    static const uint8_t source[24] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
                                       0x99u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu, 0x01u,
                                       0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u};

    for (unsigned bytes = 0u; bytes <= 20u; bytes++)
    {
        uint8_t buffer[MMGR_ACCURACY_BYTEIO_BUFFER];
        char message[128];

        accuracy_fill_guard(buffer);

        mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = buffer, .cap = sizeof buffer);

        EMBED_CALL(byteio.raw, OctetusCfg, .write_span = &span, .src = source, .bytes = bytes);
        EMBED_CALL(byteio.put, OctetusCfg, .write_span = &span, .byte = 0x5Au);

        (void)snprintf(message, sizeof message, "a raw append of %u bytes", bytes);
        TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)bytes + 1u, span.pos, message);
        TEST_ASSERT_FALSE_MESSAGE(span.overflow, "an append inside the span latched overflow");

        for (unsigned index = 0u; index < bytes; index++)
        {
            char byte_message[160];

            (void)snprintf(byte_message, sizeof byte_message, "%s: byte %u", message, index);
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(source[index], buffer[index], byte_message);
        }
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x5Au, buffer[bytes], "the byte after the run did not land at the cursor");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_BYTEIO_GUARD, buffer[bytes + 1u],
                                       "the appends reached past the bytes they were given");
    }
}

/**
 * @brief Checks that a length-prefixed read points at the run and advances past both parts.
 *
 * @note The run is not copied. The claim is that the pointer handed back names the first byte of the
 *       payload inside the span's own storage, which is checked as an address and not by comparing
 *       bytes alone.
 * @note Every payload length from none to sixteen is offered, and the length prefix is laid down by
 *       hand as four big endian bytes instead of through the module.
 * @note The cursor is checked to end past the length and the run together, which is what lets a
 *       caller read a sequence of these with nothing tracked between them.
 */
void test_a_length_prefixed_read_points_at_the_run_and_advances_past_both(void)
{
    for (unsigned payload = 0u; payload <= 16u; payload++)
    {
        uint8_t buffer[MMGR_ACCURACY_BYTEIO_BUFFER];
        const uint8_t *blob = NULL;
        size_t blob_bytes = 0u;
        char message[128];

        accuracy_fill_guard(buffer);
        for (unsigned index = 0u; index < 4u; index++)
        {
            // The four byte length, most significant first, laid down without the module's help
            buffer[index] = accuracy_big_endian_byte((uint64_t)payload, 4u, index);
        }
        for (unsigned index = 0u; index < payload; index++)
        {
            // Explicit cast narrows the mixed index to the byte the payload carries
            buffer[4u + index] = (uint8_t)(0xC0u + index);
        }

        mmgr_cspan span = EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = buffer, .cap = (size_t)payload + 4u);

        (void)snprintf(message, sizeof message, "a payload of %u bytes", payload);
        TEST_ASSERT_TRUE_MESSAGE(
            EMBED_CALL(byteio.rd_str, OctetusCfg, .read_span = &span, .blob = &blob, .blob_bytes = &blob_bytes),
            message);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(payload, blob_bytes, message);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(buffer + 4u, blob, "the run does not begin past the length prefix");
        TEST_ASSERT_EQUAL_size_t_MESSAGE((size_t)payload + 4u, span.pos, message);

        for (unsigned index = 0u; index < payload; index++)
        {
            char byte_message[160];

            (void)snprintf(byte_message, sizeof byte_message, "%s: byte %u", message, index);
            // Explicit cast narrows the mixed index to the byte the payload was built from
            TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)(0xC0u + index), blob[index], byte_message);
        }
    }
}

/**
 * @brief Checks that a run reaching past the end leaves the cursor where it started.
 *
 * @note A length read followed by a payload that is not there is not a read at all, and the cursor has
 *       to stay where it was. A cursor left between the two names a position in the middle of a field,
 *       which no later read can make sense of.
 * @note The length prefix names more bytes than the span holds, at every shortfall from one byte to
 *       several, so the failure is the payload's and not the prefix's.
 * @note Neither output is written on the failing path, which is checked by leaving both holding
 *       values no successful read would produce.
 */
void test_a_run_reaching_past_the_end_leaves_the_cursor_where_it_started(void)
{
    for (unsigned shortfall = 1u; shortfall <= 8u; shortfall++)
    {
        uint8_t buffer[MMGR_ACCURACY_BYTEIO_BUFFER];
        const uint8_t *blob = buffer;
        size_t blob_bytes = 0x5A5Au;
        const unsigned present = 8u;
        const unsigned claimed = present + shortfall;
        char message[128];

        accuracy_fill_guard(buffer);
        for (unsigned index = 0u; index < 4u; index++)
        {
            buffer[index] = accuracy_big_endian_byte((uint64_t)claimed, 4u, index);
        }

        mmgr_cspan span = EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = buffer, .cap = (size_t)present + 4u);

        (void)snprintf(message, sizeof message, "a run claiming %u bytes with %u present", claimed, present);
        TEST_ASSERT_FALSE_MESSAGE(
            EMBED_CALL(byteio.rd_str, OctetusCfg, .read_span = &span, .blob = &blob, .blob_bytes = &blob_bytes),
            message);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, span.pos, "a failed read moved the cursor");
        TEST_ASSERT_TRUE_MESSAGE(span.err, "a failed read did not set the error flag");
        TEST_ASSERT_EQUAL_size_t_MESSAGE(0x5A5Au, blob_bytes, "a failed read wrote through the length output");
        TEST_ASSERT_EQUAL_PTR_MESSAGE(buffer, blob, "a failed read wrote through the payload output");
    }
}

/**
 * @brief Checks that a fixed field right-aligns its integer and zero fills ahead of it.
 *
 * @note The field is the span's whole buffer, and the claim is that the value ends at its last byte
 *       with zeros in front. A value left-aligned or centred is a different number to any reader of
 *       the field.
 * @note Leading zero bytes of the source are skipped before the width is tested, so an integer
 *       carrying a sign byte still fits a field of its own size. That case is offered at every field
 *       width.
 * @note The cursor is checked to end at the field's cap, since the field is written whole and not
 *       appended to.
 */
void test_a_fixed_field_right_aligns_its_integer_and_zero_fills_ahead(void)
{
    static const uint8_t integer_of[5] = {0x00u, 0x00u, 0xDEu, 0xADu, 0xBEu};

    for (unsigned field_bytes = 3u; field_bytes <= 16u; field_bytes++)
    {
        uint8_t buffer[MMGR_ACCURACY_BYTEIO_BUFFER];
        char message[128];

        accuracy_fill_guard(buffer);

        mmgr_span field = EMBED_CALL(spat.from, SpatiumCfg, .buf = buffer, .cap = field_bytes);

        (void)snprintf(message, sizeof message, "a field of %u bytes", field_bytes);
        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(byteio.mpint_fixed, OctetusCfg, .write_span = &field, .src = integer_of,
                                            .bytes = sizeof integer_of),
                                 message);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(field_bytes, field.pos, message);

        // The three bytes past the leading zeros are what the field carries, right aligned
        for (unsigned index = 0u; index < (field_bytes - 3u); index++)
        {
            char byte_message[160];

            (void)snprintf(byte_message, sizeof byte_message, "%s: fill byte %u", message, index);
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, buffer[index], byte_message);
        }
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xDEu, buffer[field_bytes - 3u], "the value does not start where it should");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xADu, buffer[field_bytes - 2u], "the value's middle byte is wrong");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xBEu, buffer[field_bytes - 1u], "the value does not end at the field's end");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_BYTEIO_GUARD, buffer[field_bytes],
                                       "the field was written past its cap");
    }
}

/**
 * @brief Checks that an integer too wide for the field is refused without writing anything.
 *
 * @note This entry reports whether the integer fit instead of asserting the caller sized it right, so
 *       the refusal is a return value a case can check.
 * @note Nothing is written on the refusing path, which is what the guard bytes show. A field left
 *       half written is one a reader cannot tell from a complete one.
 * @note The span's overflow is checked to latch, which leaves the failure there for a caller that
 *       tests the span later.
 */
void test_an_integer_too_wide_for_the_field_is_refused_without_writing(void)
{
    static const uint8_t integer_of[6] = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u};

    for (unsigned field_bytes = 1u; field_bytes < 6u; field_bytes++)
    {
        uint8_t buffer[MMGR_ACCURACY_BYTEIO_BUFFER];
        char message[128];

        accuracy_fill_guard(buffer);

        mmgr_span field = EMBED_CALL(spat.from, SpatiumCfg, .buf = buffer, .cap = field_bytes);

        (void)snprintf(message, sizeof message, "a six byte integer in a field of %u", field_bytes);
        TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(byteio.mpint_fixed, OctetusCfg, .write_span = &field, .src = integer_of,
                                             .bytes = sizeof integer_of),
                                  message);
        TEST_ASSERT_TRUE_MESSAGE(field.overflow, "a refused field did not latch overflow");

        for (unsigned index = 0u; index < MMGR_ACCURACY_BYTEIO_BUFFER; index++)
        {
            char byte_message[160];

            (void)snprintf(byte_message, sizeof byte_message, "%s: byte %u", message, index);
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_BYTEIO_GUARD, buffer[index], byte_message);
        }
    }
}
