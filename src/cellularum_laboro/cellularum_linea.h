/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file cellularum_linea.h
 * @brief The calls of cellul with plain arguments.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note The calls are a length, an equal test, a prefix test and two searches. A copy and a number read follow them.
 * @note Every entry takes its arguments by value, and therefore a C++ translation unit calls them as they are. Each
 *       entry fills the members of CatenaFinitaCfg or TransfiguroCfg that its call reads. It leaves every other
 *       member zero, as a compound literal that names those members does. The compares fold no case.
 * @note theory/linea proves each entry against the contract of its call. No call reads a member outside its own
 *       set, and each answer holds what its contract states.
 */
#ifndef MMGR_CELLULARUM_LINEA_H
#define MMGR_CELLULARUM_LINEA_H

/** @addtogroup mod_cellul
 *  @{ */

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief The bytes of a text before its terminator, at max cap.
 *
 * @param[in] text The text [BORROWS].
 * @param[in] cap  Bytes readable from text.
 * @return         Bytes before the terminator, or cap where no terminator lies in range.
 * @note cellul.len with src and cap.
 * @warning text must be readable for cap bytes, with the tail word read whole as mmgr_cellul_len describes.
 */
size_t mmgr_text_length(const char *text, size_t cap);

/**
 * @brief Whether two texts hold the same terminated string.
 *
 * @param[in] left  First text [BORROWS].
 * @param[in] right Second text [BORROWS].
 * @param[in] cap   Bytes readable from each.
 * @return          EMBED_TRUE where both reach a terminator with no difference before it, inside cap.
 * @note cellul.eq with src, other and cap, and ci false.
 * @warning Both texts must be readable for cap bytes, as mmgr_cellul_eq reads them.
 */
embed_bool mmgr_text_equal(const char *left, const char *right, size_t cap);

/**
 * @brief The order of two texts, byte by byte, as strcmp orders them.
 *
 * @param[in] left  First text [BORROWS].
 * @param[in] right Second text [BORROWS].
 * @param[in] cap   Bytes readable from each.
 * @return          Below 0 where left comes first, 0 where the two are the same, above 0 where right comes first.
 * @note The first byte that differs decides, read as unsigned, and a text that ends first comes first. Each text is
 *       measured inside cap, and the compare reads no further than the shorter text and its terminator.
 * @warning Both texts must be readable for cap bytes, or up to their terminators, as mmgr_cellul_diff reads them.
 */
int mmgr_text_order(const char *left, const char *right, size_t cap);

/**
 * @brief Whether a text begins with a prefix.
 *
 * @param[in] text   The text [BORROWS].
 * @param[in] prefix The prefix [BORROWS].
 * @param[in] cap    Bytes readable from each.
 * @return           EMBED_TRUE where prefix reaches its terminator with no difference from text before it.
 * @note cellul.starts with src, other and cap, and ci false. An empty prefix matches any text.
 * @warning Both texts must be readable for cap bytes, as mmgr_cellul_starts reads them.
 */
embed_bool mmgr_text_starts(const char *text, const char *prefix, size_t cap);

/**
 * @brief Finds the first byte equal to value in a text, before its terminator.
 *
 * @param[in] text  The text [BORROWS].
 * @param[in] value The byte sought.
 * @param[in] cap   Bytes readable from text.
 * @return          The address of the first match, or NULL where it does not occur [BORROWS]. A value of 0 answers
 *                  the terminator, or text plus cap where no terminator lies in range.
 * @note cellul.chr with src, cap and byte.
 * @warning text must be readable for cap bytes, as mmgr_cellul_chr reads it.
 */
const char *mmgr_text_char(const char *text, uint8_t value, size_t cap);

/**
 * @brief Finds the first place of a needle in a haystack.
 *
 * @param[in] haystack The text searched [BORROWS].
 * @param[in] needle   The text sought [BORROWS].
 * @param[in] cap      Bytes readable from each.
 * @return             The address in haystack where needle begins, or NULL where it does not occur [BORROWS].
 * @note cellul.find with src, cap, other and other_cap at cap, and ci false. An empty needle answers haystack.
 * @warning Both texts must be readable for cap bytes, with the needle read as mmgr_cellul_find describes.
 */
const char *mmgr_text_find(const char *haystack, const char *needle, size_t cap);

/**
 * @brief Copies a text and its terminator into dst, at max cap bytes.
 *
 * @param[out] dst The destination [BORROWS].
 * @param[in]  src The text [BORROWS].
 * @param[in]  cap Bytes writable at dst.
 * @return         Bytes copied, with the terminator not in the count.
 * @note cellul.copy with dst, src and cap. A cap of 0 writes nothing at all.
 * @warning dst must be writable for cap bytes and src readable for cap minus one. The two must not overlap.
 */
size_t mmgr_text_copy(char *dst, const char *src, size_t cap);

/**
 * @brief Reads an unsigned decimal integer from a text.
 *
 * @param[in]  text The text [BORROWS].
 * @param[out] end  Where the call stores the address past the last digit, or text where no digit came, or NULL
 *                  [BORROWS].
 * @return          The value, in a word that wraps on a longer run of digits.
 * @note cellul.to_ulong with src and end. It skips leading whitespace and takes one optional '+'.
 * @warning The read stops at the first byte that is not part of the number. No length bounds it.
 */
embed_word mmgr_text_unsigned(const char *text, const char **end);

EMBED_END_DECLS

/** @} */

#endif
