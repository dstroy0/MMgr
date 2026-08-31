// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_proximus_operor_accuracy.c
 * @brief Checks every typed load and store against a byte-at-a-time reference built from the order
 *        the compiler states this host uses, at every alignment the entry accepts.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note A load through a typed pointer is one instruction on a machine that has it and a sequence of
 *       byte loads and shifts on one that does not. Which of those the compiler emitted is invisible
 *       from the return value, and both have to arrive at the same number.
 * @note The reference assembles a value one byte at a time, placing each at the position the host's
 *       byte order gives it. The compiler states that order in __BYTE_ORDER__, so nothing here works
 *       it out and nothing reads the library's own idea of it.
 * @note Every unaligned entry is offered every offset from a word boundary. That is the whole point
 *       of the unaligned family, and an entry that quietly required alignment would still pass at
 *       offset zero.
 * @note The four aligned entries are offered only aligned addresses, which is what they document. What
 *       is checked of them is that they reach the same bytes their unaligned counterparts do.
 * @note Contract checks live in test_proximus_operor. This file asks which bytes a load reads and a
 *       store writes.
 */
#include <stdint.h>
#include <stdio.h>

#include "proximus_operor/proximus_operor.h"

#include "unity.h"

/**
 * @brief Expands to 8u, the offsets from a word boundary each unaligned entry is offered.
 *
 * @note One more than the widest word any environment uses, so every offset is reached whatever
 *       embed_word is on this build.
 */
#define MMGR_ACCURACY_PROXIM_OFFSETS 8u

/**
 * @brief Expands to 96u, the bytes each working buffer holds.
 *
 * @note The longest copy any case makes, plus the largest offset it can start at, plus room for the
 *       guard bytes past it.
 */
#define MMGR_ACCURACY_PROXIM_BUFFER 96u

/**
 * @brief Expands to 0xA5, the byte a buffer is filled with before a store.
 *
 * @note A byte no case stores on purpose. A position still holding it is one the store did not
 *       reach, and a position past the width holding anything else is a store that ran long.
 */
#define MMGR_ACCURACY_PROXIM_GUARD 0xA5u

/**
 * @brief Set to 1 where this host puts a value's least significant byte at the lowest address.
 *
 * @note The compiler states the order in __BYTE_ORDER__, so nothing here works it out. Reading the
 *       library's own idea of the order would put one value on both sides of every comparison.
 * @note A compile-time constant, so every expectation below folds and no case carries a run-time
 *       branch on the order.
 * @warning The #error arm fires on a toolchain that predefines neither. Guessing an order there would
 *          make every case pass against whatever this file assumed.
 */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#define MMGR_ACCURACY_PROXIM_HOST_LITTLE (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#else
#error "this suite needs __BYTE_ORDER__ to know which byte the host puts first"
#endif

/**
 * @brief Returns the value a run of bytes stands for under the host's own byte order.
 *
 * @param[in] at    First byte of the run [BORROWS].
 * @param[in] width Bytes in the run.
 * @return          The value those bytes carry.
 * @note One shift per byte. On a little endian host the byte at the lowest address is the least
 *       significant, and on a big endian host it is the most significant of the width being read.
 * @note This is the whole reference for every load below.
 */
static uint64_t accuracy_host_value(const uint8_t *at, unsigned width)
{
    uint64_t value = 0u;

    for (unsigned index = 0u; index < width; index++)
    {
        const unsigned position = MMGR_ACCURACY_PROXIM_HOST_LITTLE ? index : (width - 1u - index);

        // Explicit cast widens each byte to the uint64_t the value is assembled in before the shift
        value |= (uint64_t)at[index] << (8u * position);
    }
    return value;
}

/**
 * @brief Returns the byte a value carries at a given address offset under the host's own byte order.
 *
 * @param[in] value  Value being stored.
 * @param[in] width  Bytes the store writes.
 * @param[in] offset Offset from the store's first byte.
 * @return           The byte that offset receives.
 * @note The inverse of accuracy_host_value, and the whole reference for every store below.
 */
static uint8_t accuracy_host_byte(uint64_t value, unsigned width, unsigned offset)
{
    const unsigned position = MMGR_ACCURACY_PROXIM_HOST_LITTLE ? offset : (width - 1u - offset);

    // Explicit cast narrows the shifted value to the byte an address holds
    return (uint8_t)((value >> (8u * position)) & 0xFFu);
}

/**
 * @brief Fills a buffer with the guard byte.
 *
 * @param[out] buffer Bytes to fill [BORROWS].
 * @note Every store case starts here, which tells a byte the store wrote from one it left alone.
 */
static void accuracy_fill_guard(uint8_t *buffer)
{
    for (size_t index = 0u; index < MMGR_ACCURACY_PROXIM_BUFFER; index++)
    {
        buffer[index] = MMGR_ACCURACY_PROXIM_GUARD;
    }
}

/**
 * @brief Returns the byte a source region carries at a given position.
 *
 * @param[in] position Position within the region.
 * @return             The byte that position holds.
 * @note Every byte differs from its neighbors along the run, which is what makes a load or a copy
 *       that shifted by one visible. Never the guard byte either.
 */
static uint8_t accuracy_source_byte(size_t position)
{
    // Explicit cast narrows the mixed position to the byte a region holds
    const uint8_t value = (uint8_t)(((position * 11u) + 5u) | 1u);

    return (value == MMGR_ACCURACY_PROXIM_GUARD) ? (uint8_t)(MMGR_ACCURACY_PROXIM_GUARD ^ 1u) : value;
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note Every buffer here has automatic or file scope storage that the case fills itself, so there
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
 * @brief Checks the byte order arithmetic this suite rests on against values worked out by hand.
 *
 * @note Exists to catch a defect in the reference as itself. A reference that placed the bytes in the
 *       other order would report every entry as wrong, and the module would be blamed for it.
 * @note The expectations are built from the same order the compiler states, so they read the same on
 *       a host of either order. What is checked is that the two helpers are each other's inverse and
 *       that a run of distinct bytes assembles to a value with those bytes in it.
 * @note The lowest addressed byte is checked directly against the order, which is the one fact every
 *       other expectation in this file rests on.
 */
void test_the_byte_order_arithmetic_this_suite_relies_on_is_itself_right(void)
{
    static const uint8_t laid_out[8] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u};

    if (MMGR_ACCURACY_PROXIM_HOST_LITTLE)
    {
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x2211), accuracy_host_value(laid_out, 2u),
                                        "two bytes do not assemble least significant first");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x8877665544332211), accuracy_host_value(laid_out, 8u),
                                        "eight bytes do not assemble least significant first");
    }
    else
    {
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x1122), accuracy_host_value(laid_out, 2u),
                                        "two bytes do not assemble most significant first");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(UINT64_C(0x1122334455667788), accuracy_host_value(laid_out, 8u),
                                        "eight bytes do not assemble most significant first");
    }

    for (unsigned width = 1u; width <= 8u; width++)
    {
        uint8_t rebuilt[8];
        char message[96];

        for (unsigned offset = 0u; offset < width; offset++)
        {
            rebuilt[offset] = accuracy_host_byte(accuracy_host_value(laid_out, width), width, offset);
        }
        (void)snprintf(message, sizeof message, "the two helpers disagree at a width of %u", width);
        for (unsigned offset = 0u; offset < width; offset++)
        {
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(laid_out[offset], rebuilt[offset], message);
        }
    }
}

/**
 * @brief Checks every load against the reference at every offset from a word boundary.
 *
 * @note This is the case the file exists for. The unaligned family lets a caller read at any address,
 *       and an entry that read the right bytes only at offset zero is exactly what it is meant to
 *       prevent.
 * @note The word-width load is included alongside the fixed widths, so the environments that carry a
 *       narrower embed_word check it at their own width.
 * @note The bytes differ along the run. A load that read one byte early or late then arrives at a
 *       different number instead of the same one.
 */
void test_every_load_reads_the_bytes_at_its_address(void)
{
    static uint8_t region[MMGR_ACCURACY_PROXIM_BUFFER];

    for (size_t index = 0u; index < sizeof region; index++)
    {
        region[index] = accuracy_source_byte(index);
    }

    for (unsigned offset = 0u; offset < MMGR_ACCURACY_PROXIM_OFFSETS; offset++)
    {
        const uint8_t *const at = region + offset;
        char message[96];

        (void)snprintf(message, sizeof message, "at an offset of %u", offset);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE((uint16_t)accuracy_host_value(at, 2u),
                                        EMBED_CALL(proxim.load16, ProximusCfg, .at = at), message);
        TEST_ASSERT_EQUAL_HEX32_MESSAGE((uint32_t)accuracy_host_value(at, 4u),
                                        EMBED_CALL(proxim.load32, ProximusCfg, .at = at), message);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_host_value(at, 8u), EMBED_CALL(proxim.load64, ProximusCfg, .at = at),
                                        message);
        // Explicit cast narrows the reference to the embed_word the word load returns, at whatever
        // width this environment carries
        TEST_ASSERT_EQUAL_MESSAGE((embed_word)accuracy_host_value(at, (unsigned)sizeof(embed_word)),
                                  EMBED_CALL(proxim.load, ProximusCfg, .at = at), message);
    }
}

/**
 * @brief Checks every store against the reference at every offset from a word boundary.
 *
 * @note Each store is checked byte by byte over the whole buffer. A store that wrote one byte too
 *       many lands on a guard, and one that wrote a byte too few leaves a guard standing where a
 *       value byte belongs.
 * @note The values carry different bytes in every position. A store that placed its bytes in the
 *       wrong order is then visible as the wrong byte at a named offset.
 * @note The upper bytes of the value take no part in the narrower stores, which the header states.
 *       The same value is offered to all four widths, and each is checked to write only its own.
 */
void test_every_store_writes_its_bytes_at_its_address(void)
{
    static const uint64_t value_of[] = {UINT64_C(0x1122334455667788), UINT64_C(0xFEDCBA9876543210), 0uLL, UINT64_MAX};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned value_index = 0u; value_index < value_count; value_index++)
    {
        for (unsigned offset = 0u; offset < MMGR_ACCURACY_PROXIM_OFFSETS; offset++)
        {
            // Explicit cast narrows the word size to the unsigned the reference counts in
            const unsigned width_of[] = {2u, 4u, 8u, (unsigned)sizeof(embed_word)};
            // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
            const unsigned width_count = (unsigned)(sizeof width_of / sizeof width_of[0]);

            for (unsigned width_index = 0u; width_index < width_count; width_index++)
            {
                const unsigned width = width_of[width_index];
                uint8_t buffer[MMGR_ACCURACY_PROXIM_BUFFER];
                char message[128];

                accuracy_fill_guard(buffer);

                if (width_index == 0u)
                {
                    EMBED_CALL(proxim.put16, ProximusCfg, .dst = buffer + offset, .val = value_of[value_index]);
                }
                else if (width_index == 1u)
                {
                    EMBED_CALL(proxim.put32, ProximusCfg, .dst = buffer + offset, .val = value_of[value_index]);
                }
                else if (width_index == 2u)
                {
                    EMBED_CALL(proxim.put64, ProximusCfg, .dst = buffer + offset, .val = value_of[value_index]);
                }
                else
                {
                    EMBED_CALL(proxim.put, ProximusCfg, .dst = buffer + offset, .val = value_of[value_index]);
                }

                (void)snprintf(message, sizeof message, "a store of %u bytes at an offset of %u carrying 0x%016llX",
                               width, offset, (unsigned long long)value_of[value_index]);

                for (size_t index = 0u; index < sizeof buffer; index++)
                {
                    const embed_bool inside = (embed_bool)((index >= offset) && (index < ((size_t)offset + width)));
                    // Explicit cast narrows the position within the store to the unsigned the
                    // reference takes
                    const uint8_t want =
                        inside ? accuracy_host_byte(value_of[value_index], width, (unsigned)(index - offset))
                               : (uint8_t)MMGR_ACCURACY_PROXIM_GUARD;

                    if (buffer[index] != want)
                    {
                        char byte_message[192];

                        (void)snprintf(byte_message, sizeof byte_message,
                                       "%s: byte %u holds 0x%02X where 0x%02X belongs", message, (unsigned)index,
                                       buffer[index], want);
                        TEST_FAIL_MESSAGE(byte_message);
                    }
                }
            }
        }
    }
}

/**
 * @brief Checks that the aligned entries reach the same bytes their unaligned counterparts do.
 *
 * @note The aligned family exists to emit a single instruction where the unaligned one compiles to a
 *       sequence. The claim is that the two are the same access and differ only in what the compiler
 *       is allowed to assume. A value stored through one is read back by the other.
 * @note Only aligned addresses are offered, which is what these entries document. The buffer is
 *       declared with the alignment MMGR_ALIGN_BYTES states, and the offsets step by whole words.
 * @note Both directions are checked at both widths, so an aligned store read by an unaligned load and
 *       an unaligned store read by an aligned load both have to agree.
 */
void test_the_aligned_entries_reach_the_same_bytes_as_the_unaligned_ones(void)
{
    static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t buffer[MMGR_ACCURACY_PROXIM_BUFFER];
    static const uint64_t value_of[] = {UINT64_C(0x1122334455667788), UINT64_C(0xFEDCBA9876543210), 0uLL, UINT64_MAX};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned index = 0u; index < value_count; index++)
    {
        for (size_t at = 0u; (at + 8u) <= sizeof buffer; at += 8u)
        {
            char message[128];

            (void)snprintf(message, sizeof message, "at offset %u carrying 0x%016llX", (unsigned)at,
                           (unsigned long long)value_of[index]);

            accuracy_fill_guard(buffer);
            EMBED_CALL(proxim.al_put64, ProximusCfg, .dst = buffer + at, .val = value_of[index]);
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_host_value(buffer + at, 8u),
                                            EMBED_CALL(proxim.al_load64, ProximusCfg, .at = buffer + at), message);
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(EMBED_CALL(proxim.load64, ProximusCfg, .at = buffer + at),
                                            EMBED_CALL(proxim.al_load64, ProximusCfg, .at = buffer + at), message);

            accuracy_fill_guard(buffer);
            EMBED_CALL(proxim.put64, ProximusCfg, .dst = buffer + at, .val = value_of[index]);
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(accuracy_host_value(buffer + at, 8u),
                                            EMBED_CALL(proxim.al_load64, ProximusCfg, .at = buffer + at), message);

            accuracy_fill_guard(buffer);
            EMBED_CALL(proxim.al_put, ProximusCfg, .dst = buffer + at, .val = value_of[index]);
            // Explicit cast narrows the reference to the embed_word the word load returns
            TEST_ASSERT_EQUAL_MESSAGE((embed_word)accuracy_host_value(buffer + at, (unsigned)sizeof(embed_word)),
                                      EMBED_CALL(proxim.al_load, ProximusCfg, .at = buffer + at), message);
            TEST_ASSERT_EQUAL_MESSAGE(EMBED_CALL(proxim.load, ProximusCfg, .at = buffer + at),
                                      EMBED_CALL(proxim.al_load, ProximusCfg, .at = buffer + at), message);
        }
    }
}

/**
 * @brief Checks that a value stored and loaded back at the same width is the value that went in.
 *
 * @note The two cases above pin each direction against the reference. This one puts them together,
 *       because a pair that disagreed with each other would still lose a caller's value.
 * @note The value is masked to the width first, since only the low bytes of it are stored.
 * @note Every offset from a word boundary is covered, so the round trip is exercised on the paths a
 *       machine with no unaligned access assembles out of byte moves.
 */
void test_a_value_stored_and_loaded_at_the_same_width_comes_back(void)
{
    static const uint64_t value_of[] = {UINT64_C(0x1122334455667788), UINT64_C(0xFEDCBA9876543210), 1uLL, UINT64_MAX};
    // Explicit cast narrows the sizeof quotient to the unsigned the loop counts in
    const unsigned value_count = (unsigned)(sizeof value_of / sizeof value_of[0]);

    for (unsigned index = 0u; index < value_count; index++)
    {
        for (unsigned offset = 0u; offset < MMGR_ACCURACY_PROXIM_OFFSETS; offset++)
        {
            uint8_t buffer[MMGR_ACCURACY_PROXIM_BUFFER];
            char message[128];

            (void)snprintf(message, sizeof message, "at an offset of %u carrying 0x%016llX", offset,
                           (unsigned long long)value_of[index]);

            accuracy_fill_guard(buffer);
            EMBED_CALL(proxim.put16, ProximusCfg, .dst = buffer + offset, .val = value_of[index]);
            // Explicit cast narrows the value to the two bytes this store carries
            TEST_ASSERT_EQUAL_HEX16_MESSAGE((uint16_t)value_of[index],
                                            EMBED_CALL(proxim.load16, ProximusCfg, .at = buffer + offset), message);

            accuracy_fill_guard(buffer);
            EMBED_CALL(proxim.put32, ProximusCfg, .dst = buffer + offset, .val = value_of[index]);
            // Explicit cast narrows the value to the four bytes this store carries
            TEST_ASSERT_EQUAL_HEX32_MESSAGE((uint32_t)value_of[index],
                                            EMBED_CALL(proxim.load32, ProximusCfg, .at = buffer + offset), message);

            accuracy_fill_guard(buffer);
            EMBED_CALL(proxim.put64, ProximusCfg, .dst = buffer + offset, .val = value_of[index]);
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(value_of[index],
                                            EMBED_CALL(proxim.load64, ProximusCfg, .at = buffer + offset), message);

            accuracy_fill_guard(buffer);
            EMBED_CALL(proxim.put, ProximusCfg, .dst = buffer + offset, .val = value_of[index]);
            // Explicit cast narrows the value to the word this store carries
            TEST_ASSERT_EQUAL_MESSAGE((embed_word)value_of[index],
                                      EMBED_CALL(proxim.load, ProximusCfg, .at = buffer + offset), message);
        }
    }
}

/**
 * @brief Checks that the copy moves every byte at every pairing of source and destination alignment.
 *
 * @note The copy walks bytes up to a destination boundary, then whole words, then the odd bytes left,
 *       and it picks a different load depending on whether the source came to rest on a boundary too.
 * @note Both alignments and the length are the three axes those branches divide on, and every case
 *       below sweeps all three instead of testing a convenient size.
 * @note The destination is checked over its whole extent. A copy that wrote one byte too many lands
 *       on a guard.
 * @note A length of zero is included, since each of the three stages is written as a do-while behind
 *       its own test and a missing test runs the body once.
 */
void test_the_copy_moves_every_byte_at_every_alignment(void)
{
    static uint8_t source[MMGR_ACCURACY_PROXIM_BUFFER];

    for (size_t index = 0u; index < sizeof source; index++)
    {
        source[index] = accuracy_source_byte(index);
    }

    for (size_t bytes = 0u; bytes <= 40u; bytes++)
    {
        for (unsigned src_at = 0u; src_at < MMGR_ACCURACY_PROXIM_OFFSETS; src_at++)
        {
            for (unsigned dst_at = 0u; dst_at < MMGR_ACCURACY_PROXIM_OFFSETS; dst_at++)
            {
                uint8_t destination[MMGR_ACCURACY_PROXIM_BUFFER];
                char message[128];

                accuracy_fill_guard(destination);
                EMBED_CALL(proxim.read, ProximusCfg, .dst = destination + dst_at, .at = source + src_at, .size = bytes);

                (void)snprintf(message, sizeof message, "a copy of %u bytes from offset %u to offset %u",
                               (unsigned)bytes, src_at, dst_at);

                for (size_t index = 0u; index < sizeof destination; index++)
                {
                    const embed_bool inside = (embed_bool)((index >= dst_at) && (index < ((size_t)dst_at + bytes)));
                    const uint8_t want =
                        inside ? source[src_at + (index - dst_at)] : (uint8_t)MMGR_ACCURACY_PROXIM_GUARD;

                    if (destination[index] != want)
                    {
                        char byte_message[192];

                        (void)snprintf(byte_message, sizeof byte_message,
                                       "%s: byte %u holds 0x%02X where 0x%02X belongs", message, (unsigned)index,
                                       destination[index], want);
                        TEST_FAIL_MESSAGE(byte_message);
                    }
                }
            }
        }
    }
}
