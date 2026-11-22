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
 * @note The calls are a length, an equal test, a prefix test and two searches. A copy and two number reads follow
 *       them. Two counts of a span and a search for the last place of a byte come next. Ten class tests and two case
 *       folds of one byte, which ascii gives, come last.
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

/**
 * @brief Reads an unsigned integer in a base from a text, as strtoul reads it.
 *
 * @param[in]  text The text [BORROWS].
 * @param[out] end  Where the call stores the address past the last digit, or text where no digit came, or NULL
 *                  [BORROWS].
 * @param[in]  base 2 to 36, or 0 for the base that the number's prefix names: 0x for 16, 0 for 8, and 10 for any other.
 * @return          The value, negated in its word after a '-'. All ones where the digits pass the word. 0 for any
 *                  other base.
 * @note cellul.to_ulong_in with src, end and base. It skips leading whitespace and takes one optional sign. A base of
 *       16 takes one optional 0x.
 * @warning The read stops at the first byte that is not a digit of the base. No length bounds it.
 */
embed_word mmgr_text_unsigned_in(const char *text, const char **end, unsigned int base);

/**
 * @brief Counts the bytes at the start of a text that a set holds, as strspn counts them.
 *
 * @param[in] text The text [BORROWS].
 * @param[in] set  The bytes counted, a terminated text [BORROWS].
 * @param[in] cap  Bytes readable from text.
 * @return         The count of bytes before the first byte the set does not hold, the terminator or cap.
 * @note cellul.span with src, cap and other.
 */
size_t mmgr_text_span(const char *text, const char *set, size_t cap);

/**
 * @brief Counts the bytes at the start of a text that a set does not hold, as strcspn counts them.
 *
 * @param[in] text The text [BORROWS].
 * @param[in] set  The bytes that stop the count, a terminated text [BORROWS].
 * @param[in] cap  Bytes readable from text.
 * @return         The count of bytes before the first byte the set holds, the terminator or cap.
 * @note cellul.span_not with src, cap and other.
 */
size_t mmgr_text_span_not(const char *text, const char *set, size_t cap);

/**
 * @brief Finds the last place of a byte in a text, as strrchr finds it.
 *
 * @param[in] text  The text [BORROWS].
 * @param[in] value The byte sought.
 * @param[in] cap   Bytes readable from text.
 * @return          The address of the last match, or NULL where it does not occur [BORROWS]. A value of 0 answers
 *                  the terminator, or text plus cap where no terminator lies in range.
 * @note cellul.chr_last with src, cap and byte.
 */
const char *mmgr_text_char_last(const char *text, uint8_t value, size_t cap);

/**
 * @brief Tests whether a byte is '0' to '9', as the is calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for '0' to '9'. A byte of 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_NUM.
 */
embed_bool mmgr_text_is_num(uint8_t value);

/**
 * @brief Tests whether a byte is 'A' to 'Z' and 'a' to 'z', as the is calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for 'A' to 'Z' and 'a' to 'z'. A byte of 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_ALPHA.
 */
embed_bool mmgr_text_is_alpha(uint8_t value);

/**
 * @brief Tests whether a byte is '0' to '9', 'A' to 'Z' and 'a' to 'z', as the is calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for '0' to '9', 'A' to 'Z' and 'a' to 'z'. A byte of 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_ALNUM.
 */
embed_bool mmgr_text_is_alnum(uint8_t value);

/**
 * @brief Tests whether a byte is 'A' to 'Z', as the is calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for 'A' to 'Z'. A byte of 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_UPPER.
 */
embed_bool mmgr_text_is_upper(uint8_t value);

/**
 * @brief Tests whether a byte is 'a' to 'z', as the is calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for 'a' to 'z'. A byte of 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_LOWER.
 */
embed_bool mmgr_text_is_lower(uint8_t value);

/**
 * @brief Tests whether a byte is '0' to '9', 'A' to 'F' and 'a' to 'f', as the is calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for '0' to '9', 'A' to 'F' and 'a' to 'f'. A byte of 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_HEX.
 */
embed_bool mmgr_text_is_hex(uint8_t value);

/**
 * @brief Tests whether a byte is the ASCII punctuation, '!' to '/', ':' to '@', '[' to '\`' and '{' to '~', as the is
 * calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for the ASCII punctuation, '!' to '/', ':' to '@', '[' to '\`' and '{' to '~'. A byte of
 * 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_PUNCT.
 */
embed_bool mmgr_text_is_punct(uint8_t value);

/**
 * @brief Tests whether a byte is the bytes 9 to 13, and 32, as the is calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for the bytes 9 to 13, and 32. A byte of 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_SPACE.
 */
embed_bool mmgr_text_is_space(uint8_t value);

/**
 * @brief Tests whether a byte is the bytes 0 to 31, and 127, as the is calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for the bytes 0 to 31, and 127. A byte of 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_CTRL.
 */
embed_bool mmgr_text_is_ctrl(uint8_t value);

/**
 * @brief Tests whether a byte is the bytes 32 to 126, as the is calls of ctype.h test it.
 *
 * @param[in] value The byte.
 * @return          EMBED_TRUE for the bytes 32 to 126. A byte of 0x80 or above is in no class.
 * @note ascii.in with byte and MMGR_ASCII_PRINT.
 */
embed_bool mmgr_text_is_print(uint8_t value);

/**
 * @brief Folds one byte to its lower case, as tolower folds it.
 *
 * @param[in] value The byte.
 * @return          'a' to 'z' for 'A' to 'Z', and any other byte as it is.
 * @note ascii.lower with byte.
 */
uint8_t mmgr_text_lower(uint8_t value);

/**
 * @brief Folds one byte to its upper case, as toupper folds it.
 *
 * @param[in] value The byte.
 * @return          'A' to 'Z' for 'a' to 'z', and any other byte as it is.
 * @note ascii.upper with byte.
 */
uint8_t mmgr_text_upper(uint8_t value);

EMBED_END_DECLS

/** @} */

#endif
