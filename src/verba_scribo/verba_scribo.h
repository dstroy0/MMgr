/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Text and number formatting: the limits, the arguments, and the verba dispatch table.
 *
 * @note Every writing entry takes the offset to write at and returns the offset past what it wrote, so calls chain.
 * @note An entry that will not fit returns c->cap, which every later entry then reads as no room left.
 * @note finish stores the terminator and reports the length; nothing before it terminates the buffer.
 */
#ifndef MMGR_VERBA_SCRIBO_H
#define MMGR_VERBA_SCRIBO_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Expands to 18u, the most significant digits mmgr_verba_g will keep.
 *
 * @note A larger c->sig is held here; a c->sig of 0 is taken as 1.
 */
#define MMGR_G_MAX_SIG 18u

/**
 * @brief Expands to 18u, the most digits after the point mmgr_verba_fixed will write.
 *
 * @note A larger c->decimals is held here; a c->decimals of 0 writes no point at all.
 */
#define MMGR_FIXED_MAX_DECIMALS 18u

/**
 * @brief Arguments for the verba calls; each entry reads only the members it needs.
 *
 * @note Every writing entry reads out, cap and at; ok reads cap and at alone.
 * @note text is read by put_n, put, put_clip, xml and json; text_len by put_n and put.
 * @note val by u64_clip, uint, u32w, hex, u32 and u64; sval by i64; real by g, fixed and the three predicates.
 * @note base by uint alone; min by uint, u32w and hex; columns by u64_clip; sig by g; decimals by fixed.
 */
typedef struct
{
    char *const out;        /**< Destination buffer [BORROWS]. */
    const size_t cap;       /**< Bytes available in out. */
    const size_t at;        /**< Offset to write at. */
    const char *const text; /**< Text to write [BORROWS]. */
    const size_t text_len;  /**< Bytes of text put_n writes, and the length put takes when non-zero. */
    const char ch;          /**< Character ch writes. */
    const uint64_t val;     /**< Unsigned value the integer entries write. */
    const int64_t sval;     /**< Signed value i64 writes. */
    const double real;      /**< Value g, fixed and the three predicates read. */
    const uint8_t base;     /**< Numeric base uint writes in: 8, 16, or anything else for ten. */
    const uint8_t min;      /**< Fewest digits to write, padded on the left with zeros. */
    const uint8_t columns;  /**< Fewest columns u64_clip fills, padded on the left with spaces. */
    const uint8_t sig;      /**< Significant digits g keeps, held at MMGR_G_MAX_SIG. */
    const uint8_t decimals; /**< Digits after the point fixed writes, held at MMGR_FIXED_MAX_DECIMALS. */
} VerbaCfg;

/**
 * @brief Type of the verba dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the twenty members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note The first fifteen write; finish terminates; the last four report without writing anything.
 */
typedef struct
{
    size_t (*put_n)(const VerbaCfg *c);        /**< Writes a counted run of text. */
    size_t (*put)(const VerbaCfg *c);          /**< Writes a terminated string, measuring it first. */
    size_t (*put_clip)(const VerbaCfg *c);     /**< Writes as much of a string as fits. */
    size_t (*u64_clip)(const VerbaCfg *c);     /**< Writes a value right aligned, padded with spaces. */
    size_t (*xml)(const VerbaCfg *c);          /**< Writes text with the four XML entities substituted. */
    size_t (*ch)(const VerbaCfg *c);           /**< Writes one character. */
    size_t (*uint)(const VerbaCfg *c);         /**< Writes a value in the base the caller gives. */
    size_t (*u32w)(const VerbaCfg *c);         /**< Writes a value in base ten, padded to min digits. */
    size_t (*hex)(const VerbaCfg *c);          /**< Writes a value in lower case base sixteen. */
    size_t (*u32)(const VerbaCfg *c);          /**< Writes a value in base ten, unpadded. */
    size_t (*u64)(const VerbaCfg *c);          /**< Writes a value in base ten, unpadded. */
    size_t (*i64)(const VerbaCfg *c);          /**< Writes a signed value in base ten. */
    size_t (*g)(const VerbaCfg *c);            /**< Writes a double to a significant digit count. */
    size_t (*fixed)(const VerbaCfg *c);        /**< Writes a double to a decimal count. */
    size_t (*json)(const VerbaCfg *c);         /**< Writes text as a quoted JSON string. */
    size_t (*finish)(const VerbaCfg *c);       /**< Stores the terminator and reports the length. */
    mmgr_bool (*ok)(const VerbaCfg *c);        /**< Reports whether there is still room. */
    mmgr_bool (*sign_bit)(const VerbaCfg *c);  /**< Reports a double's sign bit. */
    mmgr_bool (*is_inf)(const VerbaCfg *c);    /**< Reports whether a double is an infinity. */
    mmgr_bool (*is_nan)(const VerbaCfg *c);    /**< Reports whether a double is a NaN. */
} VerbaScriboNs;
MMGR_NS_LAYOUT(VerbaScriboNs, put_n, put, put_clip, u64_clip, xml, ch, uint, u32w, hex, u32, u64, i64, g, fixed, json,
               finish, ok, sign_bit, is_inf, is_nan);

/**
 * @brief Writes c->text_len bytes of c->text at c->at.
 *
 * @param[in] c Buffer, capacity, offset, the text and its length [BORROWS].
 * @return      The offset past the text, or c->cap when it does not fit.
 * @note Writes nothing at all when it does not fit, rather than writing what it can.
 * @note Does not stop at a terminator, so this writes bytes rather than a string.
 * @warning c->text must be readable for c->text_len bytes.
 */
size_t mmgr_verba_put_n(const VerbaCfg *c);

/**
 * @brief Writes the whole of c->text at c->at.
 *
 * @param[in] c Buffer, capacity, offset, the text and optionally its length [BORROWS].
 * @return      The offset past the text, or c->cap when it does not fit.
 * @note Writes nothing at all when it does not fit, where mmgr_verba_put_clip writes what it can.
 * @note Takes c->text_len when it is non-zero and measures from the terminator only when it is not.
 *       Pass the length wherever it is known, which for a literal it always is.
 * @warning c->text must not be NULL. It must be terminated within c->cap bytes when c->text_len is 0,
 *          and readable for c->text_len bytes when it is not.
 */
size_t mmgr_verba_put(const VerbaCfg *c);

/**
 * @brief Writes as much of c->text as fits at c->at, cutting it short rather than refusing.
 *
 * @param[in] c Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past what was written, which is c->at when nothing was.
 * @note Returns c->at rather than c->cap when it writes nothing, so a later call can still write.
 * @note A NULL c->text writes nothing and reports c->at, where mmgr_verba_put would not accept one.
 */
size_t mmgr_verba_put_clip(const VerbaCfg *c);

/**
 * @brief Writes c->val in base ten at c->at, right aligned in at least c->columns columns.
 *
 * @param[in] c Buffer, capacity, offset, the value and the column count [BORROWS].
 * @return      The offset past the field, which is c->at when there was no room.
 * @note Pads on the left with spaces, where the min based entries pad with leading zeros.
 * @note c->columns is a floor, so a value needing more digits widens the field rather than being cut.
 */
size_t mmgr_verba_u64_clip(const VerbaCfg *c);

/**
 * @brief Writes c->text at c->at, replacing the four characters XML gives entities.
 *
 * @param[in] c Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past what was written, which is c->at for a NULL c->text.
 * @note Replaces &amp; with &amp;amp;, &lt; with &amp;lt;, &gt; with &amp;gt; and the double quote with &amp;quot;.
 * @note The apostrophe is written as it stands, so this suits element text and double quoted attributes.
 * @warning c->text is bounded by its own terminator here, not by c->cap.
 */
size_t mmgr_verba_xml(const VerbaCfg *c);

/**
 * @brief Writes c->ch at c->at.
 *
 * @param[in] c Buffer, capacity, offset and the character [BORROWS].
 * @return      c->at plus one, or c->cap when there is no room.
 */
size_t mmgr_verba_ch(const VerbaCfg *c);

/**
 * @brief Writes c->val in base c->base at c->at, in at least c->min digits.
 *
 * @param[in] c Buffer, capacity, offset, the value, the base and the least digit count [BORROWS].
 * @return      The offset past the digits, or c->cap when they do not fit.
 * @note The only entry that takes the base from the caller; base 16 writes lower case letters.
 * @note Padding to c->min is done with leading zeros, where mmgr_verba_u64_clip pads with spaces.
 * @warning Any c->base other than 8 or 16 is written in base ten, whatever value it holds.
 */
size_t mmgr_verba_uint(const VerbaCfg *c);

/**
 * @brief Writes c->val in base ten at c->at, in at least c->min digits.
 *
 * @param[in] c Buffer, capacity, offset, the value and the least digit count [BORROWS].
 * @return      The offset past the digits, or c->cap when they do not fit.
 * @note Fixes the base at ten but honors c->min, which is what separates it from mmgr_verba_u32.
 * @note Pads with leading zeros.
 */
size_t mmgr_verba_u32w(const VerbaCfg *c);

/**
 * @brief Writes c->val in base sixteen at c->at, in at least c->min digits.
 *
 * @param[in] c Buffer, capacity, offset, the value and the least digit count [BORROWS].
 * @return      The offset past the digits, or c->cap when they do not fit.
 * @note Writes lower case letters, and no 0x prefix.
 * @note Pads with leading zeros, so c->min gives a fixed width field.
 */
size_t mmgr_verba_hex(const VerbaCfg *c);

/**
 * @brief Writes c->val in base ten at c->at, with no padding.
 *
 * @param[in] c Buffer, capacity, offset and the value [BORROWS].
 * @return      The offset past the digits, or c->cap when they do not fit.
 * @note Fixes the base at ten and the least digit count at one, so c->base and c->min take no part.
 * @note c->val is 64 bits here as everywhere, so this behaves the same as mmgr_verba_u64.
 */
size_t mmgr_verba_u32(const VerbaCfg *c);

/**
 * @brief Writes c->val in base ten at c->at, with no padding.
 *
 * @param[in] c Buffer, capacity, offset and the value [BORROWS].
 * @return      The offset past the digits, or c->cap when they do not fit.
 * @note Fixes the base at ten and the least digit count at one, so c->base and c->min take no part.
 * @note Behaves the same as mmgr_verba_u32; the two names differ only in what a caller means to say.
 */
size_t mmgr_verba_u64(const VerbaCfg *c);

/**
 * @brief Writes c->sval in base ten at c->at, with a leading minus when it is negative.
 *
 * @param[in] c Buffer, capacity, offset and the signed value [BORROWS].
 * @return      The offset past the digits, or c->cap when they do not fit.
 * @note The only entry that reads c->sval; the unsigned entries all read c->val.
 * @note The most negative value is written correctly, since the magnitude is taken without negating it directly.
 */
size_t mmgr_verba_i64(const VerbaCfg *c);

/**
 * @brief Writes c->real at c->at to c->sig significant digits, in whichever form is shorter.
 *
 * @param[in] c Buffer, capacity, offset, the value and the significant digit count [BORROWS].
 * @return      The offset past what was written, or c->cap once something did not fit.
 * @note A c->sig of 0 is taken as 1, and anything above MMGR_G_MAX_SIG is held there.
 * @note Trailing zeros are dropped, so a value needing fewer digits than c->sig is written short.
 * @note An exponential form is used when the decimal exponent falls below -4 or reaches c->sig.
 * @note An infinity writes inf with its sign, and a NaN writes nan without one; neither is quoted.
 */
size_t mmgr_verba_g(const VerbaCfg *c);

/**
 * @brief Writes c->real at c->at with exactly c->decimals digits after the point.
 *
 * @param[in] c Buffer, capacity, offset, the value and the decimal count [BORROWS].
 * @return      The offset past what was written, or c->cap once something did not fit.
 * @note A c->decimals of 0 writes no point, and anything above MMGR_FIXED_MAX_DECIMALS is held there.
 * @note Ties round to even, and a fraction that rounds up to one carries into the integer part.
 * @note A magnitude too large for a 64-bit integer part falls back to mmgr_verba_g at ten significant digits.
 * @note An infinity writes inf with its sign, and a NaN writes nan without one; neither is quoted.
 */
size_t mmgr_verba_fixed(const VerbaCfg *c);

/**
 * @brief Writes c->text at c->at as a quoted JSON string.
 *
 * @param[in] c Buffer, capacity, offset and the text [BORROWS].
 * @return      The offset past the closing quote, or c->cap once something did not fit.
 * @note Writes both quotes itself, so the result is a complete JSON string rather than its contents.
 * @note Escapes the double quote and the backslash, and every byte below 0x20 as a letter or as \\u00xx.
 * @note A NULL c->text writes an empty pair of quotes, where mmgr_verba_xml writes nothing.
 * @warning c->text is bounded by its own terminator here, not by c->cap.
 */
size_t mmgr_verba_json(const VerbaCfg *c);

/**
 * @brief Stores the terminator at c->at and reports the length.
 *
 * @param[in] c Buffer, capacity and the offset reached [BORROWS].
 * @return      c->at, or 0 when c->at already reached c->cap.
 * @note The only entry that writes a terminator, so c->out is not a string until this has run.
 * @note A return of 0 covers both an empty result and one that ran out of room; mmgr_verba_ok tells them apart.
 */
size_t mmgr_verba_finish(const VerbaCfg *c);

/**
 * @brief Returns whether c->at is still below c->cap.
 *
 * @param[in] c Capacity and the offset reached [BORROWS].
 * @return      MMGR_TRUE while there is still room, MMGR_FALSE once an entry returned c->cap.
 * @note Reads neither c->out nor any value member, so it touches no memory.
 */
mmgr_bool mmgr_verba_ok(const VerbaCfg *c);

/**
 * @brief Returns whether c->real has its sign bit set.
 *
 * @param[in] c The value to test [BORROWS].
 * @return      MMGR_TRUE when the sign bit is set.
 * @note Reads the bit rather than comparing against zero, so a negative zero returns MMGR_TRUE.
 * @note Writes nothing, so c->out, c->cap and c->at take no part.
 */
mmgr_bool mmgr_verba_sign_bit(const VerbaCfg *c);

/**
 * @brief Returns whether c->real is an infinity.
 *
 * @param[in] c The value to test [BORROWS].
 * @return      MMGR_TRUE for either infinity.
 * @note Says nothing about the sign, so a negative infinity returns MMGR_TRUE too.
 * @note Writes nothing, so c->out, c->cap and c->at take no part.
 */
mmgr_bool mmgr_verba_is_inf(const VerbaCfg *c);

/**
 * @brief Returns whether c->real is a NaN.
 *
 * @param[in] c The value to test [BORROWS].
 * @return      MMGR_TRUE for any NaN, quiet or signaling.
 * @note Wants the exponent field all ones and the mantissa non-zero, where mmgr_verba_is_inf wants it zero.
 * @note Writes nothing, so c->out, c->cap and c->at take no part.
 */
mmgr_bool mmgr_verba_is_nan(const VerbaCfg *c);

/**
 * @brief Dispatch table instance named verba; each member calls the matching mmgr_verba_ function.
 */
MMGR_NS VerbaScriboNs verba MMGR_UNUSED = {
    .put_n = mmgr_verba_put_n,
    .put = mmgr_verba_put,
    .put_clip = mmgr_verba_put_clip,
    .u64_clip = mmgr_verba_u64_clip,
    .xml = mmgr_verba_xml,
    .ch = mmgr_verba_ch,
    .uint = mmgr_verba_uint,
    .u32w = mmgr_verba_u32w,
    .hex = mmgr_verba_hex,
    .u32 = mmgr_verba_u32,
    .u64 = mmgr_verba_u64,
    .i64 = mmgr_verba_i64,
    .g = mmgr_verba_g,
    .fixed = mmgr_verba_fixed,
    .json = mmgr_verba_json,
    .finish = mmgr_verba_finish,
    .ok = mmgr_verba_ok,
    .sign_bit = mmgr_verba_sign_bit,
    .is_inf = mmgr_verba_is_inf,
    .is_nan = mmgr_verba_is_nan,
};

MMGR_FINIS_DECLS

#endif
