/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief ASCII class membership: mask type, class list, and the ascii dispatch table.
 */
#ifndef MMGR_ASCII_PERSONA_BITORUM_H
#define MMGR_ASCII_PERSONA_BITORUM_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Sixteen bytes holding one bit for each of the code points 0 to 127.
 *
 * @note Code point n is bit (n & 7) of b[n >> 3].
 */
typedef struct
{
    uint8_t b[16];
} MmgrAsciiMask;

MMGR_STATIC_ASSERT(sizeof(MmgrAsciiMask) == 16u, "an ASCII class mask is exactly 128 bits");

/**
 * @brief Character class selector, numbered from 0.
 *
 * @note MMGR_ASCII_CLASSES is the enumerator count, not a class. Passing it is out of range.
 */
typedef enum
{
    MMGR_ASCII_NUM = 0,   /**< '0' to '9'. */
    MMGR_ASCII_ALPHA,     /**< 'A' to 'Z' and 'a' to 'z'. */
    MMGR_ASCII_ALNUM,     /**< '0' to '9', 'A' to 'Z' and 'a' to 'z'. */
    MMGR_ASCII_UPPER,     /**< 'A' to 'Z'. */
    MMGR_ASCII_LOWER,     /**< 'a' to 'z'. */
    MMGR_ASCII_HEX,       /**< '0' to '9', 'A' to 'F' and 'a' to 'f'. */
    MMGR_ASCII_PUNCT,     /**< '!' to '/', ':' to '@', '[' to '`' and '{' to '~'. */
    MMGR_ASCII_SPACE,     /**< 9 to 13, and 32. */
    MMGR_ASCII_CTRL,      /**< 0 to 31, and 127. */
    MMGR_ASCII_PRINT,     /**< 32 to 126. */
    MMGR_ASCII_CLASSES    /**< Enumerator count, not a class. */
} MmgrAsciiClass;

/**
 * @brief Arguments to mmgr_ascii_in: the class and the byte to test.
 */
typedef struct
{
    const MmgrAsciiClass kind; /**< Class to test against, below MMGR_ASCII_CLASSES. */
    const uint8_t byte;        /**< Code point to look up; 0x80 and above are in no class. */
} AsciiCfg;

/**
 * @brief Type of the ascii dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the in member is at offset 0 and that the struct holds nothing else.
 */
typedef struct
{
    mmgr_bool (*in)(const AsciiCfg *c); /**< Whether a byte belongs to a class. */
} AsciiPersonaBitorumNs;
MMGR_NS_LAYOUT(AsciiPersonaBitorumNs, in);

/**
 * @brief Returns whether c->byte has its bit set in the c->kind bitmap.
 *
 * @param[in] c Class and byte to test [BORROWS].
 * @return      MMGR_TRUE when the bit is set, MMGR_FALSE otherwise.
 * @note Bytes 0x80 and above return MMGR_FALSE.
 * @warning c->kind must be below MMGR_ASCII_CLASSES.
 */
mmgr_bool mmgr_ascii_in(const AsciiCfg *c);

/**
 * @brief Dispatch table instance named ascii; its in member calls mmgr_ascii_in.
 */
MMGR_NS AsciiPersonaBitorumNs ascii MMGR_UNUSED = {
    .in = mmgr_ascii_in,
};

MMGR_FINIS_DECLS

#endif
