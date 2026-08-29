/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file mmgr_string_shim.h
 * @brief Redirects the <string.h> names onto MMgr's bounded implementations.
 *
 * @note Defines the usual <string.h> include guards, so a later #include <string.h> contributes nothing.
 * @warning strcmp, strcasecmp, strncmp and strncasecmp report equality only and never order.
 * @warning Including this header changes the meaning of those names for the whole translation unit.
 */
#ifndef MMGR_STRING_SHIM_H
#define MMGR_STRING_SHIM_H

#include "cellularum_laboro/cellularum_laboro.h"
#include "config/mmgr_config.h"
#include "memoria_operor/memoria_operor.h"

/**
 * @brief Read bound applied to the <string.h> names that take no length of their own.
 *
 * @note Defaults to MMGR_CARCER_MAX, the largest confinium a string can occupy.
 * @warning The six shims below that take no limit of their own - strlen, strstr, strcasestr, strcmp,
 *          strcasecmp and strchr - stop at this many bytes even without a terminator.
 */
#ifndef MMGR_STR_MAX

#define MMGR_STR_MAX MMGR_CARCER_MAX
#endif

/**
 * @brief Claims the include guards several C libraries use for <string.h>.
 *
 * @note A later #include <string.h> then expands to nothing, leaving these macros in place.
 * @warning A library whose guard is not among these five will still declare the real functions.
 */
#ifndef _STRING_H
#define _STRING_H 1
#endif
#ifndef _STRING_H_
#define _STRING_H_ 1
#endif
#ifndef __STRING_H__
#define __STRING_H__ 1
#endif
#ifndef _STRING_H_INCLUDED
#define _STRING_H_INCLUDED 1
#endif
#ifndef _INC_STRING
#define _INC_STRING 1
#endif

MMGR_INCIPE_DECLS

/**
 * @brief Copies bytes from source to dest and returns dest.
 *
 * @param[out] dest   Destination to write [BORROWS].
 * @param[in]  source Bytes to read [BORROWS].
 * @param[in]  bytes  Number of bytes to copy.
 * @return            dest [BORROWS].
 * @warning The two regions must not overlap; use mmgr_shim_move when they might.
 * @warning dest must be writable for bytes and source readable for the same.
 */
MMGR_INLINE void *mmgr_shim_cpy(void *dest, const void *source, size_t bytes)
{
    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = dest, .src = source, .bytes = bytes);
    return dest;
}

/**
 * @brief Copies bytes from source to dest, tolerating overlap, and returns dest.
 *
 * @param[out] dest   Destination to write [BORROWS].
 * @param[in]  source Bytes to read [BORROWS].
 * @param[in]  bytes  Number of bytes to copy.
 * @return            dest [BORROWS].
 * @note Compares the two addresses and walks upwards or downwards so overlapping bytes are read
 *       before they are written.
 * @warning dest must be writable for bytes and source readable for the same.
 */
MMGR_INLINE void *mmgr_shim_move(void *dest, const void *source, size_t bytes)
{
    // a relational test wants both sides to point at a complete object type, and void is not one, so
    // both are cast to a byte address to pick the direction the copy walks
    if ((const uint8_t *)dest <= (const uint8_t *)source)
    {
        MMGR_CALL(memor.move_down, MemoriaCfg, .dst = dest, .src = source, .bytes = bytes);
    }
    else
    {
        MMGR_CALL(memor.move_up, MemoriaCfg, .dst = dest, .src = source, .bytes = bytes);
    }
    return dest;
}

/**
 * @brief Fills bytes of dest with the low byte of value and returns dest.
 *
 * @param[out] dest  Destination to write [BORROWS].
 * @param[in]  value Fill value; only its low eight bits are used.
 * @param[in]  bytes Number of bytes to write.
 * @return           dest [BORROWS].
 * @note Explicit cast narrows value to uint8_t, matching the byte the fill writes.
 * @warning dest must be writable for bytes.
 */
MMGR_INLINE void *mmgr_shim_set(void *dest, mmgr_iword value, size_t bytes)
{
    MMGR_CALL(memor.set, MemoriaCfg, .dst = dest, .val = (uint8_t)value, .bytes = bytes);
    return dest;
}

/**
 * @brief Compares bytes of left against right.
 *
 * @param[in] left  First region [BORROWS].
 * @param[in] right Second region [BORROWS].
 * @param[in] bytes Number of bytes to compare.
 * @return          Difference of the first unequal byte pair, or 0 when all bytes match.
 * @note Unlike the strcmp shims, this one does order: the sign follows the differing bytes.
 * @warning Both left and right must be readable for bytes.
 */
MMGR_INLINE mmgr_iword mmgr_shim_cmp(const void *left, const void *right, size_t bytes)
{
    return MMGR_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right, .bytes = bytes);
}

/**
 * @brief Finds the first byte in region equal to the low byte of value.
 *
 * @param[in] region Bytes to search [BORROWS].
 * @param[in] value  Byte sought; only its low eight bits are used.
 * @param[in] bytes  Number of bytes to search.
 * @return           Address of the match, or NULL when the byte does not occur [BORROWS].
 * @note The cast through size_t drops the const the backend returns, matching the memchr signature.
 * @warning region must be readable for bytes.
 */
MMGR_INLINE void *mmgr_shim_chr(const void *region, mmgr_iword value, size_t bytes)
{
    return (void *)(size_t)MMGR_CALL(memor.chr, MemoriaCfg, .src = region, .bytes = bytes, .val = (uint8_t)value);
}

/**
 * @brief Replaces memcpy with mmgr_shim_cpy.
 *
 * @note Every argument is parenthesized, so any expression may be passed.
 */
#define memcpy(dest, source, bytes) mmgr_shim_cpy((dest), (source), (bytes))

/**
 * @brief Replaces memmove with mmgr_shim_move.
 */
#define memmove(dest, source, bytes) mmgr_shim_move((dest), (source), (bytes))

/**
 * @brief Replaces memset with mmgr_shim_set.
 */
#define memset(dest, value, bytes) mmgr_shim_set((dest), (value), (bytes))

/**
 * @brief Replaces memcmp with mmgr_shim_cmp.
 *
 * @note Keeps the ordering the standard requires; the sign follows the first differing byte pair.
 */
#define memcmp(left, right, bytes) mmgr_shim_cmp((left), (right), (bytes))

/**
 * @brief Replaces memchr with mmgr_shim_chr.
 */
#define memchr(region, value, bytes) mmgr_shim_chr((region), (value), (bytes))

/**
 * @brief Replaces strlen with a scan bounded at MMGR_STR_MAX.
 *
 * @return Bytes before the terminator, at most MMGR_STR_MAX.
 * @warning Returns MMGR_STR_MAX when no terminator appears within it, rather than scanning further.
 * @warning text must be readable until its terminator, or for MMGR_STR_MAX bytes when it has none,
 *          with the tail word read whole as mmgr_cellul_len describes.
 */
#define strlen(text) MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = (text), .cap = MMGR_STR_MAX)

/**
 * @brief Replaces strnlen with a scan bounded at limit.
 *
 * @return Bytes before the terminator, at most limit.
 * @note Returns limit when no terminator appears within it, as strnlen does.
 * @warning text must be readable until its terminator, or for limit bytes when it has none, with the
 *          tail word read whole as mmgr_cellul_len describes.
 */
#define strnlen(text, limit) MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = (text), .cap = (limit))

/**
 * @brief Replaces strstr with a case-sensitive search bounded at MMGR_STR_MAX.
 *
 * @return Address inside haystack where needle begins, or NULL when it does not occur [BORROWS].
 * @note The cast through size_t drops the const the backend returns, matching the strstr signature.
 * @warning Both operands must be terminated within MMGR_STR_MAX; without a terminator the search
 *          reads that many bytes from them.
 */
#define strstr(haystack, needle)                                                                                       \
    ((char *)(size_t)MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = (haystack), .cap = MMGR_STR_MAX,                   \
                               .other = (needle), .other_cap = MMGR_STR_MAX, .ci = MMGR_FALSE))

/**
 * @brief Replaces strcasestr with a case-folded search bounded at MMGR_STR_MAX.
 *
 * @return Address inside haystack where needle begins, or NULL when it does not occur [BORROWS].
 * @note Differs from the strstr shim only in passing ci as MMGR_TRUE.
 * @warning Both operands must be terminated within MMGR_STR_MAX; without a terminator the search
 *          reads that many bytes from them.
 */
#define strcasestr(haystack, needle)                                                                                   \
    ((char *)(size_t)MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = (haystack), .cap = MMGR_STR_MAX,                   \
                               .other = (needle), .other_cap = MMGR_STR_MAX, .ci = MMGR_TRUE))

/**
 * @brief Replaces strcmp with an equality test bounded at MMGR_STR_MAX.
 *
 * @return 0 when left and right hold the same string, 1 when they differ.
 * @note Both == 0 and ! therefore test equality, as they do with the real strcmp.
 * @warning Never negative and never above 1, so it cannot be used to order strings.
 * @warning Both operands must be terminated within MMGR_STR_MAX; without a terminator the compare
 *          reads that many bytes from them.
 */
#define strcmp(left, right)                                                                                            \
    (!MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = (left), .other = (right), .cap = MMGR_STR_MAX, .ci = MMGR_FALSE))

/**
 * @brief Replaces strcasecmp with a case-folded equality test bounded at MMGR_STR_MAX.
 *
 * @return 0 when left and right hold the same string ignoring case, 1 when they differ.
 * @warning Never negative and never above 1, so it cannot be used to order strings.
 * @warning Both operands must be terminated within MMGR_STR_MAX; without a terminator the compare
 *          reads that many bytes from them.
 */
#define strcasecmp(left, right)                                                                                        \
    (!MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = (left), .other = (right), .cap = MMGR_STR_MAX, .ci = MMGR_TRUE))

/**
 * @brief Replaces strncmp with an equality test over at most limit bytes.
 *
 * @return 0 when left and right agree through limit bytes, 1 when they differ before it.
 * @warning Never negative and never above 1, so it cannot be used to order strings.
 * @warning A terminator does not end the comparison; all limit bytes must be readable in both
 *          operands, with the tail word read whole as mmgr_cellul_diff describes.
 * @warning limit appears twice in the expansion, so an argument with a side effect is evaluated twice.
 */
#define strncmp(left, right, limit)                                                                                    \
    (MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = (left), .other = (right), .cap = (limit), .ci = MMGR_FALSE) <      \
     (limit))

/**
 * @brief Replaces strncasecmp with a case-folded equality test over at most limit bytes.
 *
 * @return 0 when left and right agree through limit bytes ignoring case, 1 when they differ before it.
 * @warning Never negative and never above 1, so it cannot be used to order strings.
 * @warning A terminator does not end the comparison; all limit bytes must be readable in both
 *          operands, with the tail word read whole as mmgr_cellul_diff describes.
 * @warning limit appears twice in the expansion, so an argument with a side effect is evaluated twice.
 */
#define strncasecmp(left, right, limit)                                                                                \
    (MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = (left), .other = (right), .cap = (limit), .ci = MMGR_TRUE) <       \
     (limit))

/**
 * @brief Replaces strlcpy with a bounded copy that always terminates unless limit is 0.
 *
 * @return Bytes copied, not counting the terminator.
 * @warning Not the source length the real strlcpy returns, so a caller cannot detect truncation by
 *          comparing the result against limit.
 * @warning dest must be writable for limit bytes and source readable for limit minus one.
 */
#define strlcpy(dest, source, limit)                                                                                   \
    MMGR_CALL(cellul.copy, CatenaFinitaCfg, .dst = (dest), .src = (source), .cap = (limit))

/**
 * @brief Replaces strchr with a search bounded at MMGR_STR_MAX.
 *
 * @return Address inside text where the byte occurs, or NULL when it does not [BORROWS].
 * @note A value of 0 returns the address of the terminator, as strchr does.
 * @note The cast through size_t drops the const the backend returns, matching the strchr signature,
 *       and value is narrowed to uint8_t, the byte the search compares against.
 * @warning text must be terminated within MMGR_STR_MAX; without a terminator the search reads that
 *          many bytes from it.
 */
#define strchr(text, value)                                                                                            \
    ((char *)(size_t)MMGR_CALL(cellul.chr, CatenaFinitaCfg, .src = (text), .cap = MMGR_STR_MAX,                        \
                               .byte = (uint8_t)(value)))

MMGR_FINIS_DECLS

#endif
