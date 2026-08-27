/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief ASCII class membership, read from the 128-bit s_class bitmaps.
 */
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

/**
 * @brief One 128-bit membership bitmap per MmgrAsciiClass value.
 *
 * @note Indexed by MmgrAsciiClass; code point n is bit (n & 7) of byte (n >> 3).
 * @note Worked through, MMGR_ASCII_NUM holds 0xFF at byte 6 and 0x03 at byte 7. Byte 6 carries code
 *       points 48 through 55, which is '0' to '7', and the low two bits of byte 7 carry 56 and 57,
 *       which is '8' and '9'. Every row below reads the same way, so none of them has to be taken
 *       on trust.
 * @note Sixteen bytes reach code point 127 and no further, which is what leaves a byte at 0x80 or
 *       above with no row it could be found in.
 */
static const MmgrAsciiMask s_class[MMGR_ASCII_CLASSES] = {
    [MMGR_ASCII_NUM] = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00}},
    [MMGR_ASCII_ALPHA] = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0xFF, 0x07, 0xFE, 0xFF, 0xFF,
                           0x07}},
    [MMGR_ASCII_ALNUM] = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03, 0xFE, 0xFF, 0xFF, 0x07, 0xFE, 0xFF, 0xFF,
                           0x07}},
    [MMGR_ASCII_UPPER] = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0xFF, 0x07, 0x00, 0x00, 0x00,
                           0x00}},
    [MMGR_ASCII_LOWER] = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0xFF,
                           0x07}},
    [MMGR_ASCII_HEX] = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03, 0x7E, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00,
                         0x00}},
    [MMGR_ASCII_PUNCT] = {{0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0x00, 0xFC, 0x01, 0x00, 0x00, 0xF8, 0x01, 0x00, 0x00,
                           0x78}},
    [MMGR_ASCII_SPACE] = {{0x00, 0x3E, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00}},
    [MMGR_ASCII_CTRL] = {{0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x80}},
    [MMGR_ASCII_PRINT] = {{0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                           0x7F}},
};

/**
 * @brief Argument type built by MMGR_CALL in mmgr_ascii_in.
 *
 * @note Fields match AsciiCfg, without its const qualifiers.
 */
typedef struct
{
    MmgrAsciiClass kind;
    uint8_t byte;
} AsciiCtx;

/**
 * @brief Returns whether c->byte has its bit set in s_class[c->kind].
 *
 * @param[in] c Class and byte to test [BORROWS].
 * @return      MMGR_TRUE when the bit is set, MMGR_FALSE otherwise.
 * @note Bytes 0x80 and above return MMGR_FALSE without reading s_class.
 * @warning c->kind must be below MMGR_ASCII_CLASSES.
 */
MMGR_INLINE mmgr_bool ascii_in(const AsciiCtx *c)
{
    MMGR_ASSERT(c->kind < MMGR_ASCII_CLASSES, "no such character class");

    const MmgrAsciiMask *const entry = &s_class[c->kind];

    // Explicit cast narrows the int result of && to the mmgr_bool container
    return (mmgr_bool)((c->byte < 0x80u) && (((entry->b[c->byte >> 3] >> (c->byte & 7u)) & 1u) != 0u));
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_ascii_ and ascii_ prefixes, which the two share.
 */
#define ASCII_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_ascii_, ascii_, AsciiCtx, AsciiCfg, ret, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in ascii_persona_bitorum.h.
 */
ASCII_ENTRY(mmgr_bool, in, .kind = c->kind, .byte = c->byte)
