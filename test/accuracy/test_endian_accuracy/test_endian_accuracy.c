// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_endian_accuracy.c
 * @brief Checks which byte each endian call puts where, against a byte-at-a-time reference built
 *        from shifts, at all three widths and over values whose bytes are all different.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note Byte order is a statement about which byte of a value lands at which address, so the
 *       reference is one shift per byte. It shares no code with the module, which moves whole words
 *       through proximus_operor.
 * @note endian.h documents parva_extremitas as the host's own order and magna_extremitas as the
 *       reverse of it, neither one pinned to little or big. The compiler states which order this
 *       host uses in __BYTE_ORDER__, and both expectations are built from that, so the checks hold
 *       on a host of either order.
 * @note Every value below has eight different bytes. A value with repeated bytes passes a reversal
 *       that moved them to the wrong places, which is the defect this file is looking for.
 * @note Every integer type comes from stdint.h. A wrong width alias would resize the shifts each
 *       expectation is built from.
 * @note Contract checks on a width outside the enumerators, and on the shift that a width of zero
 *       makes undefined, live in test_endian. This file asks where the bytes go.
 */
#include <stdint.h>

#include "endian/endian.h"

#include "unity.h"

/**
 * @brief Expands to 8, the widest endian move and the size of the staging buffers below.
 *
 * @note MMGR_ENDIAN_64 is the widest enumerator. The buffers carry a guard byte past it. A write
 *       that ran long is caught at the byte after the one it was allowed.
 */
#define MMGR_ACCURACY_ENDIAN_MAX 8u

/**
 * @brief Expands to 0xA5, the byte every staging buffer is filled with before a write.
 *
 * @note Neither 0x00 nor 0xFF, and not a byte of any test value. A byte left untouched is then
 *       distinguishable from one the call wrote.
 */
#define MMGR_ACCURACY_ENDIAN_GUARD 0xA5u

/**
 * @brief Set to 1 where this host puts a value's least significant byte at the lowest address.
 *
 * @note The compiler states the order in __BYTE_ORDER__, so nothing here works it out. That is the
 *       toolchain's answer and not the library's, which is what keeps it usable as a reference:
 *       MMGR_HW_BIG_ENDIAN is what the module branches on, and reading it would put the same answer
 *       on both sides of every comparison below.
 * @note A compile-time constant, so every expectation below folds and no case carries a run-time
 *       branch on the order.
 * @warning The #error arm fires on a toolchain that predefines neither. Guessing an order there
 *          would make every placement check pass against whatever this file assumed.
 */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#define MMGR_ACCURACY_HOST_LITTLE (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#else
#error "this suite needs __BYTE_ORDER__ to know which byte the host puts first"
#endif

/**
 * @brief Returns the byte a value carries at a given position, counting from its least significant.
 *
 * @param[in] value    Value to take a byte out of.
 * @param[in] position Byte position, 0 being the least significant.
 * @return             That byte.
 * @note One shift and one mask. This is the whole of the reference every expectation is built from.
 */
static uint8_t accuracy_byte_at(uint64_t value, unsigned position)
{
    // Explicit cast narrows the shifted value to the byte this returns. The mask keeps eight bits,
    // which is what a uint8_t holds
    return (uint8_t)((value >> (position * 8u)) & 0xFFu);
}

/**
 * @brief Fills a buffer with the guard byte.
 *
 * @param[out] buffer Bytes to fill [BORROWS].
 * @note Every write test starts here. A byte the call did not write still holds the guard, which is
 *       what catches a write that ran past its width.
 */
static void accuracy_fill_guard(uint8_t *buffer)
{
    for (unsigned position = 0u; position <= MMGR_ACCURACY_ENDIAN_MAX; position++)
    {
        buffer[position] = MMGR_ACCURACY_ENDIAN_GUARD;
    }
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every buffer here has automatic storage inside the case that builds it, and there is no
 *       shared state to prepare.
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
 * @brief Checks the byte extraction this suite rests on.
 *
 * @note Exists to catch a defect in the helper as itself. Without this case a broken
 *       accuracy_byte_at would surface as an endian mismatch, and the module would be blamed.
 * @note The expectations are literals a reader can check. 0x0123456789ABCDEF holds 0xEF at position
 *       0 and 0x01 at position 7, counting from the least significant byte.
 */
void test_the_exact_arithmetic_this_suite_relies_on_is_itself_right(void)
{
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xEFu, accuracy_byte_at(0x0123456789ABCDEFull, 0u), "position 0 is the low byte");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xCDu, accuracy_byte_at(0x0123456789ABCDEFull, 1u),
                                   "position 1 is the next byte up");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x01u, accuracy_byte_at(0x0123456789ABCDEFull, 7u), "position 7 is the high byte");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, accuracy_byte_at(0u, 3u), "every byte of zero is zero");
}

/**
 * @brief Checks that the host-order write puts each byte of the value where the host puts it.
 *
 * @note parva_extremitas moves bytes as they lie. On a little endian host the byte at the lowest
 *       address is the value's least significant, and on a big endian host it is the most
 *       significant of the width being written.
 * @note The guard byte past the width is checked on every case. A write that moved eight bytes for a
 *       width of two is caught there.
 */
void test_the_host_order_write_places_every_byte(void)
{
    static const uint64_t value_of[] = {0x0123456789ABCDEFull, 0ull, 0xFFFFFFFFFFFFFFFFull, 0x00FF00FF00FF00FFull};
    static const mmgr_endian_width width_of[] = {MMGR_ENDIAN_16, MMGR_ENDIAN_32, MMGR_ENDIAN_64};
    // Explicit casts narrow the sizeof quotients to the unsigned the loops count in. Both arrays are
    // a handful of entries, far inside what an unsigned carries
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);
    const unsigned width_count = (unsigned)(sizeof width_of / sizeof width_of[0]);

    for (unsigned value_index = 0u; value_index < value_count; value_index++)
    {
        for (unsigned width_index = 0u; width_index < width_count; width_index++)
        {
            const unsigned width = (unsigned)width_of[width_index];
            uint8_t written[MMGR_ACCURACY_ENDIAN_MAX + 1u];

            accuracy_fill_guard(written);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(width,
                                             EMBED_CALL(parva_extremitas.wr, EndianCfg, .dst = written,
                                                        .val = value_of[value_index], .width = width_of[width_index]),
                                             "the write did not report the width it was given");

            for (unsigned position = 0u; position < width; position++)
            {
                const unsigned source = MMGR_ACCURACY_HOST_LITTLE ? position : (width - 1u - position);

                TEST_ASSERT_EQUAL_HEX8_MESSAGE(accuracy_byte_at(value_of[value_index], source), written[position],
                                               "the host order write put a byte at the wrong address");
            }
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_ENDIAN_GUARD, written[width],
                                           "the write ran past the width it was given");
        }
    }
}

/**
 * @brief Checks that the reversed write is the host-order write with its bytes turned around.
 *
 * @note magna_extremitas is documented as the reverse of the host's order. The expectation is built
 *       from the same byte extraction with the position mirrored across the width.
 * @note Comparing against the mirrored reference and not against the other table keeps this
 *       independent of whether parva_extremitas is itself correct.
 */
void test_the_reversed_write_places_every_byte(void)
{
    static const uint64_t value_of[] = {0x0123456789ABCDEFull, 0ull, 0xFFFFFFFFFFFFFFFFull, 0x00FF00FF00FF00FFull};
    static const mmgr_endian_width width_of[] = {MMGR_ENDIAN_16, MMGR_ENDIAN_32, MMGR_ENDIAN_64};
    // Explicit casts narrow the sizeof quotients to the unsigned the loops count in, as above
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);
    const unsigned width_count = (unsigned)(sizeof width_of / sizeof width_of[0]);

    for (unsigned value_index = 0u; value_index < value_count; value_index++)
    {
        for (unsigned width_index = 0u; width_index < width_count; width_index++)
        {
            const unsigned width = (unsigned)width_of[width_index];
            uint8_t written[MMGR_ACCURACY_ENDIAN_MAX + 1u];

            accuracy_fill_guard(written);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(width,
                                             EMBED_CALL(magna_extremitas.wr, EndianCfg, .dst = written,
                                                        .val = value_of[value_index], .width = width_of[width_index]),
                                             "the write did not report the width it was given");

            for (unsigned position = 0u; position < width; position++)
            {
                const unsigned source = MMGR_ACCURACY_HOST_LITTLE ? (width - 1u - position) : position;

                TEST_ASSERT_EQUAL_HEX8_MESSAGE(accuracy_byte_at(value_of[value_index], source), written[position],
                                               "the reversed write put a byte at the wrong address");
            }
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(MMGR_ACCURACY_ENDIAN_GUARD, written[width],
                                           "the write ran past the width it was given");
        }
    }
}

/**
 * @brief Checks that each read returns the value its bytes stand for under that table's order.
 *
 * @note The bytes are laid out here one at a time and the expectation is assembled from them with
 *       shifts, so nothing about the answer comes from the module's own loads.
 * @note Both tables are read from the same buffer. The two answers are the value and its reversal at
 *       that width, which is what makes a read that ignored its order visible.
 */
void test_each_read_returns_the_value_its_bytes_stand_for(void)
{
    static const uint8_t laid_out[MMGR_ACCURACY_ENDIAN_MAX] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u};
    static const mmgr_endian_width width_of[] = {MMGR_ENDIAN_16, MMGR_ENDIAN_32, MMGR_ENDIAN_64};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in, as above
    const unsigned width_count = (unsigned)(sizeof width_of / sizeof width_of[0]);

    for (unsigned width_index = 0u; width_index < width_count; width_index++)
    {
        const unsigned width = (unsigned)width_of[width_index];
        uint64_t host_order = 0ull;
        uint64_t reversed = 0ull;

        for (unsigned position = 0u; position < width; position++)
        {
            // Explicit casts widen each byte to the uint64_t the expectation is assembled in. The
            // shift places it at the position that order gives it
            const unsigned host_shift = MMGR_ACCURACY_HOST_LITTLE ? position : (width - 1u - position);
            const unsigned reverse_shift = MMGR_ACCURACY_HOST_LITTLE ? (width - 1u - position) : position;

            host_order |= (uint64_t)laid_out[position] << (host_shift * 8u);
            reversed |= (uint64_t)laid_out[position] << (reverse_shift * 8u);
        }

        TEST_ASSERT_EQUAL_HEX64_MESSAGE(
            host_order, EMBED_CALL(parva_extremitas.rd, EndianCfg, .src = laid_out, .width = width_of[width_index]),
            "the host order read did not return the value its bytes stand for");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(
            reversed, EMBED_CALL(magna_extremitas.rd, EndianCfg, .src = laid_out, .width = width_of[width_index]),
            "the reversed read did not return the value its bytes stand for");
    }
}

/**
 * @brief Checks that the reversal moves each byte to the position mirroring it across the width.
 *
 * @note rev takes no buffer and reads no memory, so the expectation is built from the value alone.
 *       Byte at position i of the result is byte at width minus one minus i of the input.
 * @note The result is right-aligned into the low width bytes, so the positions above the width are
 *       checked to be zero.
 */
void test_the_reversal_mirrors_every_byte_across_the_width(void)
{
    static const uint64_t value_of[] = {0x0123456789ABCDEFull, 0ull, 0xFFFFFFFFFFFFFFFFull, 0x00FF00FF00FF00FFull};
    static const mmgr_endian_width width_of[] = {MMGR_ENDIAN_16, MMGR_ENDIAN_32, MMGR_ENDIAN_64};
    // Explicit casts narrow the sizeof quotients to the unsigned the loops count in, as above
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);
    const unsigned width_count = (unsigned)(sizeof width_of / sizeof width_of[0]);

    for (unsigned value_index = 0u; value_index < value_count; value_index++)
    {
        for (unsigned width_index = 0u; width_index < width_count; width_index++)
        {
            const unsigned width = (unsigned)width_of[width_index];
            const uint64_t produced = EMBED_CALL(parva_extremitas.rev, EndianCfg, .val = value_of[value_index],
                                                 .width = width_of[width_index]);
            uint64_t expected = 0ull;

            for (unsigned position = 0u; position < width; position++)
            {
                // Explicit cast widens the mirrored byte to the uint64_t the expectation is built in
                expected |= (uint64_t)accuracy_byte_at(value_of[value_index], width - 1u - position) << (position * 8u);
            }
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(expected, produced, "the reversal put a byte at the wrong position");
        }
    }
}

/**
 * @brief Checks that a write followed by a read of the same order returns what was written.
 *
 * @note A round trip through one table holds whether or not that table's order is the one intended,
 *       so this alone proves nothing about placement. It is here because a pair that disagreed with
 *       each other would pass every placement check above and still lose a caller's value.
 * @note The value is masked to the width first. Only the low width bytes survive a write of that
 *       width, and the rest are not part of what comes back.
 */
void test_a_write_then_a_read_of_the_same_order_returns_the_value(void)
{
    static const uint64_t value_of[] = {0x0123456789ABCDEFull, 0ull, 0xFFFFFFFFFFFFFFFFull, 0x00FF00FF00FF00FFull};
    static const mmgr_endian_width width_of[] = {MMGR_ENDIAN_16, MMGR_ENDIAN_32, MMGR_ENDIAN_64};
    // Explicit casts narrow the sizeof quotients to the unsigned the loops count in, as above
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);
    const unsigned width_count = (unsigned)(sizeof width_of / sizeof width_of[0]);

    for (unsigned value_index = 0u; value_index < value_count; value_index++)
    {
        for (unsigned width_index = 0u; width_index < width_count; width_index++)
        {
            const unsigned width = (unsigned)width_of[width_index];
            // A width of eight would shift a 64-bit value by 64, which C leaves undefined, so the
            // full-width mask is written out instead of computed
            const uint64_t mask =
                (width == MMGR_ACCURACY_ENDIAN_MAX) ? 0xFFFFFFFFFFFFFFFFull : ((1ull << (width * 8u)) - 1ull);
            const uint64_t expected = value_of[value_index] & mask;
            uint8_t written[MMGR_ACCURACY_ENDIAN_MAX + 1u];

            accuracy_fill_guard(written);
            (void)EMBED_CALL(parva_extremitas.wr, EndianCfg, .dst = written, .val = value_of[value_index],
                             .width = width_of[width_index]);
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(
                expected, EMBED_CALL(parva_extremitas.rd, EndianCfg, .src = written, .width = width_of[width_index]),
                "a host order write and read did not return the value");

            accuracy_fill_guard(written);
            (void)EMBED_CALL(magna_extremitas.wr, EndianCfg, .dst = written, .val = value_of[value_index],
                             .width = width_of[width_index]);
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(
                expected, EMBED_CALL(magna_extremitas.rd, EndianCfg, .src = written, .width = width_of[width_index]),
                "a reversed write and read did not return the value");
        }
    }
}
