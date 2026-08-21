// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

/**
 * @file ascii_persona_bitorum.c
 * @brief The class masks, and the one question asked of them.
 *
 * The masks are file local: this is the only translation unit that holds them and the only one that
 * can name them.
 *
 * No context. An index and a byte are already two registers, and a struct to carry them would be a
 * store and a load each to get back what was passed in.
 */

/** @brief The classes, in the order MmgrAsciiClass names them. */
static const MmgrAsciiMask s_class[MMGR_ASCII_CLASSES] = {
    [MMGR_ASCII_NUM] = MMGR_ASCII_NUM_INIT,
    [MMGR_ASCII_ALPHA] = MMGR_ASCII_ALPHA_INIT,
    [MMGR_ASCII_ALNUM] = MMGR_ASCII_ALNUM_INIT,
    [MMGR_ASCII_UPPER] = MMGR_ASCII_UPPER_INIT,
    [MMGR_ASCII_LOWER] = MMGR_ASCII_LOWER_INIT,
    [MMGR_ASCII_HEX] = MMGR_ASCII_HEX_INIT,
    [MMGR_ASCII_PUNCT] = MMGR_ASCII_PUNCT_INIT,
    [MMGR_ASCII_SPACE] = MMGR_ASCII_SPACE_INIT,
    [MMGR_ASCII_CTRL] = MMGR_ASCII_CTRL_INIT,
    [MMGR_ASCII_PRINT] = MMGR_ASCII_PRINT_INIT,
};

/* One load, one shift and one and, on every width. Bytes at or above 0x80 are in no class, and an
   index past the last class is a caller that has not decided what it is asking. */
mmgr_bool mmgr_ascii_in(MmgrAsciiClass k, uint8_t c)
{
    MMGR_ASSERT(k < MMGR_ASCII_CLASSES, "no such character class");

    const MmgrAsciiMask *const m = &s_class[k];
    return (mmgr_bool)((c < 0x80u) && (((m->b[c >> 3] >> (c & 7u)) & 1u) != 0u));
}
