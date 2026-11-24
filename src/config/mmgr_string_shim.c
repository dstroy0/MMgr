/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file mmgr_string_shim.c
 * @brief memset, memcpy, memmove and memcmp as symbols, over memor.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-23
 *
 * @note A compiler writes a call to memset, memcpy, memmove or memcmp where no source names one: for a copy of a
 *       struct, for the zero members of a literal, and for a loop that it recognizes. The macros of
 *       mmgr_string_shim.h do not reach such a call. These definitions do, and so a program with no C library links.
 * @note Each definition is one call of memor, as each mmgr_shim_ function of mmgr_string_shim.h is.
 * @note Under gcc and clang each definition is strong. GNU ld for PE-COFF leaves a call from another object to a
 *       weak definition of these names unresolved. Under MSVC each body has the name with _default after it, and the
 *       linker's alternate name points the name at it, as EMBED_WEAK_DEFAULT says.
 * @note The target of this file is opt in: mmgr_<env> does not link it. A suite that checks MMgr against the C
 *       library links the C library's names, and these definitions would take them first.
 * @note The target of this file has no LTO. The code generator writes its calls to these names after LTO chose
 *       what to keep, and an LTO that saw no call to them drops them from the image.
 * @note harness.py nolibc holds that the code of each definition calls none of the four names again.
 * @warning This file includes no <string.h> and not mmgr_string_shim.h. The macros of that header would rename
 *          the definitions here.
 */
#include "memoria_operor/memoria_operor.h"

EMBED_BEGIN_DECLS

/**
 * @brief Fills bytes of dest with the low byte of value, as the C standard's memset does.
 *
 * @param[out] dest  Destination [BORROWS].
 * @param[in]  value The fill, of which the low eight bits count.
 * @param[in]  bytes Bytes to write.
 * @return           dest [BORROWS].
 * @note The types are the C standard's, and not the project's, since a compiler calls this name with them.
 * @warning dest must be writable for bytes.
 */
void *EMBED_WEAK_NAME(memset)(void *dest, int value, size_t bytes);

/**
 * @brief Copies bytes from source to dest, as the C standard's memcpy does.
 *
 * @param[out] dest   Destination [BORROWS].
 * @param[in]  source Source [BORROWS].
 * @param[in]  bytes  Bytes to copy.
 * @return            dest [BORROWS].
 * @warning The two regions must not overlap.
 */
void *EMBED_WEAK_NAME(memcpy)(void *dest, const void *source, size_t bytes);

/**
 * @brief Copies bytes from source to dest, the regions free to overlap, as the C standard's memmove does.
 *
 * @param[out] dest   Destination [BORROWS].
 * @param[in]  source Source [BORROWS].
 * @param[in]  bytes  Bytes to copy.
 * @return            dest [BORROWS].
 * @note A destination at or below the source takes memor.move_down, and one above it takes memor.move_up, as
 *       mmgr_shim_move does.
 */
void *EMBED_WEAK_NAME(memmove)(void *dest, const void *source, size_t bytes);

/**
 * @brief Orders bytes of left against right, as the C standard's memcmp does.
 *
 * @param[in] left  First region [BORROWS].
 * @param[in] right Second region [BORROWS].
 * @param[in] bytes Bytes to compare.
 * @return          The difference of the first byte pair that differs, 0 where every byte matches.
 * @warning Both regions must be readable for bytes rounded up to a whole word, as memor.cmp reads them.
 */
int EMBED_WEAK_NAME(memcmp)(const void *left, const void *right, size_t bytes);

EMBED_WEAK_DEFAULT(memset)
void *EMBED_WEAK_NAME(memset)(void *dest, int value, size_t bytes)
{
    // Explicit cast narrows the fill to the byte that the standard says each write takes
    EMBED_CALL(memor.set, MemoriaCfg, .dst = dest, .val = (uint8_t)value, .bytes = bytes);
    return dest;
}

EMBED_WEAK_DEFAULT(memcpy)
void *EMBED_WEAK_NAME(memcpy)(void *dest, const void *source, size_t bytes)
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = dest, .src = source, .bytes = bytes);
    return dest;
}

EMBED_WEAK_DEFAULT(memmove)
void *EMBED_WEAK_NAME(memmove)(void *dest, const void *source, size_t bytes)
{
    // Explicit casts give both addresses a byte type, since a compare of two pointers needs a complete type
    if ((const uint8_t *)dest <= (const uint8_t *)source)
    {
        EMBED_CALL(memor.move_down, MemoriaCfg, .dst = dest, .src = source, .bytes = bytes);
    }
    else
    {
        EMBED_CALL(memor.move_up, MemoriaCfg, .dst = dest, .src = source, .bytes = bytes);
    }
    return dest;
}

EMBED_WEAK_DEFAULT(memcmp)
int EMBED_WEAK_NAME(memcmp)(const void *left, const void *right, size_t bytes)
{
    // Explicit cast narrows the difference of two bytes, which lies from -255 to 255, to the int of the standard
    return (int)EMBED_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right, .bytes = bytes);
}

EMBED_END_DECLS
