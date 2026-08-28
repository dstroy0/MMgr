/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Text and number formatting: the limits, and one table per kind of thing written.
 *
 * @note Five tables rather than one, each with the arguments its own entries read. A call carries the
 *       destination and the value it is placing, and nothing else: writing a character does not pass
 *       a double, a base and a column count it will never look at.
 * @note Every writing entry takes the offset to write at and returns the offset past what it wrote, so calls chain.
 * @note An entry that will not fit returns args->cap, which every later entry then reads as no room left.
 * @note finish stores the terminator and reports the length; nothing before it terminates the buffer.
 */
#ifndef MMGR_VERBA_SCRIBO_H
#define MMGR_VERBA_SCRIBO_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Expands to 18u, the most significant digits mmgr_verba_g will keep.
 *
 * @note A larger args->sig is held here; an args->sig of 0 is taken as 1.
 */
#define MMGR_G_MAX_SIG 18u

/**
 * @brief Expands to 18u, the most digits after the point mmgr_verba_fixed will write.
 *
 * @note A larger args->decimals is held here; an args->decimals of 0 writes no point at all.
 */
#define MMGR_FIXED_MAX_DECIMALS 18u

/**
 * @brief Arguments for the entries that write text written into a buffer.
 *
 * @param out      Destination buffer [BORROWS].
 * @param cap      Bytes available in out.
 * @param at       Offset to write at.
 * @param text     Text to write [BORROWS].
 * @param text_len Bytes of text put_n writes, and the length put takes when non-zero.
 */
typedef struct
{
    char *const out;        /**< Destination buffer [BORROWS]. */
    const size_t cap;       /**< Bytes available in out. */
    const size_t at;        /**< Offset to write at. */
    const char *const text; /**< Text to write [BORROWS]. */
    const size_t text_len;  /**< Bytes of text put_n writes, and the length put takes when non-zero. */
} VerbaTextusCfg;

/**
 * @brief Type of the verba_textus dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the five members sit at consecutive MMGR_FP_SIZE offsets.
 */
typedef struct
{
    size_t (*put_n)(const VerbaTextusCfg *args);    /**< Writes a counted run of text. */
    size_t (*put)(const VerbaTextusCfg *args);      /**< Writes a terminated string, measuring it first. */
    size_t (*put_clip)(const VerbaTextusCfg *args); /**< Writes as much of a string as fits. */
    size_t (*xml)(const VerbaTextusCfg *args);      /**< Writes text with the four XML entities substituted. */
    size_t (*json)(const VerbaTextusCfg *args);     /**< Writes text as a quoted JSON string. */
} VerbaScriboTextusNs;
MMGR_NS_LAYOUT(VerbaScriboTextusNs, put_n, put, put_clip, xml, json);

/**
 * @brief Arguments for the entries that write one character.
 *
 * @param out      Destination buffer [BORROWS].
 * @param cap      Bytes available in out.
 * @param at       Offset to write at.
 * @param ch       Character to write.
 */
typedef struct
{
    char *const out;  /**< Destination buffer [BORROWS]. */
    const size_t cap; /**< Bytes available in out. */
    const size_t at;  /**< Offset to write at. */
    const char ch;    /**< Character to write. */
} VerbaLitteraCfg;

/**
 * @brief Type of the verba_littera dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the one members sit at consecutive MMGR_FP_SIZE offsets.
 */
typedef struct
{
    size_t (*ch)(const VerbaLitteraCfg *args); /**< Writes one character. */
} VerbaScriboLitteraNs;
MMGR_NS_LAYOUT(VerbaScriboLitteraNs, ch);

/**
 * @brief Arguments for the entries that write an integer.
 *
 * @param out      Destination buffer [BORROWS].
 * @param cap      Bytes available in out.
 * @param at       Offset to write at.
 * @param val      Unsigned value the unsigned entries write.
 * @param sval     Signed value i64 writes.
 * @param base     Numeric base uint writes in: 8, 16, or anything else for ten.
 * @param min      Fewest digits to write, padded on the left with zeros.
 * @param columns  Fewest columns u64_clip fills, padded on the left with spaces.
 */
typedef struct
{
    char *const out;       /**< Destination buffer [BORROWS]. */
    const size_t cap;      /**< Bytes available in out. */
    const size_t at;       /**< Offset to write at. */
    const uint64_t val;    /**< Unsigned value the unsigned entries write. */
    const int64_t sval;    /**< Signed value i64 writes. */
    const uint8_t base;    /**< Numeric base uint writes in: 8, 16, or anything else for ten. */
    const uint8_t min;     /**< Fewest digits to write, padded on the left with zeros. */
    const uint8_t columns; /**< Fewest columns u64_clip fills, padded on the left with spaces. */
} VerbaNumerusCfg;

/**
 * @brief Type of the verba_numerus dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the seven members sit at consecutive MMGR_FP_SIZE offsets.
 */
typedef struct
{
    size_t (*u64_clip)(const VerbaNumerusCfg *args); /**< Writes a value right aligned, padded with spaces. */
    size_t (*uint)(const VerbaNumerusCfg *args);     /**< Writes a value in the base the caller gives. */
    size_t (*u32w)(const VerbaNumerusCfg *args);     /**< Writes a value in base ten, padded to min digits. */
    size_t (*hex)(const VerbaNumerusCfg *args);      /**< Writes a value in lower case base sixteen. */
    size_t (*u32)(const VerbaNumerusCfg *args);      /**< Writes a value in base ten, unpadded. */
    size_t (*u64)(const VerbaNumerusCfg *args);      /**< Writes a value in base ten, unpadded. */
    size_t (*i64)(const VerbaNumerusCfg *args);      /**< Writes a signed value in base ten. */
} VerbaScriboNumerusNs;
MMGR_NS_LAYOUT(VerbaScriboNumerusNs, u64_clip, uint, u32w, hex, u32, u64, i64);

/**
 * @brief Arguments for the entries that write a double.
 *
 * @param out      Destination buffer [BORROWS]. The three predicates leave it unset.
 * @param cap      Bytes available in out.
 * @param at       Offset to write at.
 * @param real     The value to write or classify.
 * @param sig      Significant digits g keeps, held at MMGR_G_MAX_SIG.
 * @param decimals Digits after the point fixed writes, held at MMGR_FIXED_MAX_DECIMALS.
 */
typedef struct
{
    char *const out;        /**< Destination buffer [BORROWS]. The three predicates leave it unset. */
    const size_t cap;       /**< Bytes available in out. */
    const size_t at;        /**< Offset to write at. */
    const double real;      /**< The value to write or classify. */
    const uint8_t sig;      /**< Significant digits g keeps, held at MMGR_G_MAX_SIG. */
    const uint8_t decimals; /**< Digits after the point fixed writes, held at MMGR_FIXED_MAX_DECIMALS. */
} VerbaFractioCfg;

/**
 * @brief Type of the verba_fractio dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the five members sit at consecutive MMGR_FP_SIZE offsets.
 */
typedef struct
{
    size_t (*g)(const VerbaFractioCfg *args);           /**< Writes a double to a significant digit count. */
    size_t (*fixed)(const VerbaFractioCfg *args);       /**< Writes a double to a decimal count. */
    mmgr_bool (*sign_bit)(const VerbaFractioCfg *args); /**< Reports a double's sign bit. */
    mmgr_bool (*is_inf)(const VerbaFractioCfg *args);   /**< Reports whether a double is an infinity. */
    mmgr_bool (*is_nan)(const VerbaFractioCfg *args);   /**< Reports whether a double is a NaN. */
} VerbaScriboFractioNs;
MMGR_NS_LAYOUT(VerbaScriboFractioNs, g, fixed, sign_bit, is_inf, is_nan);

/**
 * @brief Arguments for the entries that write the buffer itself, with no value to place in it.
 *
 * @param out      Destination buffer [BORROWS]. ok leaves it unset.
 * @param cap      Bytes available in out.
 * @param at       Offset reached.
 */
typedef struct
{
    char *const out;  /**< Destination buffer [BORROWS]. ok leaves it unset. */
    const size_t cap; /**< Bytes available in out. */
    const size_t at;  /**< Offset reached. */
} VerbaFinisCfg;

/**
 * @brief Type of the verba_finis dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the two members sit at consecutive MMGR_FP_SIZE offsets.
 */
typedef struct
{
    size_t (*finish)(const VerbaFinisCfg *args); /**< Stores the terminator and reports the length. */
    mmgr_bool (*ok)(const VerbaFinisCfg *args);  /**< Reports whether there is still room. */
} VerbaScriboFinisNs;
MMGR_NS_LAYOUT(VerbaScriboFinisNs, finish, ok);

/**
 * @brief Writes a counted run of text.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_put_n(const VerbaTextusCfg *args);

/**
 * @brief Writes a terminated string, measuring it first.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_put(const VerbaTextusCfg *args);

/**
 * @brief Writes as much of a string as fits.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_put_clip(const VerbaTextusCfg *args);

/**
 * @brief Writes text with the four XML entities substituted.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_xml(const VerbaTextusCfg *args);

/**
 * @brief Writes text as a quoted JSON string.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_json(const VerbaTextusCfg *args);

/**
 * @brief Writes one character.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_ch(const VerbaLitteraCfg *args);

/**
 * @brief Writes a value right aligned, padded with spaces.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_u64_clip(const VerbaNumerusCfg *args);

/**
 * @brief Writes a value in the base the caller gives.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_uint(const VerbaNumerusCfg *args);

/**
 * @brief Writes a value in base ten, padded to min digits.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_u32w(const VerbaNumerusCfg *args);

/**
 * @brief Writes a value in lower case base sixteen.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_hex(const VerbaNumerusCfg *args);

/**
 * @brief Writes a value in base ten, unpadded.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_u32(const VerbaNumerusCfg *args);

/**
 * @brief Writes a value in base ten, unpadded.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_u64(const VerbaNumerusCfg *args);

/**
 * @brief Writes a signed value in base ten.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_i64(const VerbaNumerusCfg *args);

/**
 * @brief Writes a double to a significant digit count.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_g(const VerbaFractioCfg *args);

/**
 * @brief Writes a double to a decimal count.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_fixed(const VerbaFractioCfg *args);

/**
 * @brief Reports a double's sign bit.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The answer; nothing is written.
 */
mmgr_bool mmgr_verba_sign_bit(const VerbaFractioCfg *args);

/**
 * @brief Reports whether a double is an infinity.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The answer; nothing is written.
 */
mmgr_bool mmgr_verba_is_inf(const VerbaFractioCfg *args);

/**
 * @brief Reports whether a double is a NaN.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The answer; nothing is written.
 */
mmgr_bool mmgr_verba_is_nan(const VerbaFractioCfg *args);

/**
 * @brief Stores the terminator and reports the length.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The offset past what was written, or args->cap when it did not fit.
 */
size_t mmgr_verba_finish(const VerbaFinisCfg *args);

/**
 * @brief Reports whether there is still room.
 *
 * @param[in] args The arguments this entry reads [BORROWS].
 * @return      The answer; nothing is written.
 */
mmgr_bool mmgr_verba_ok(const VerbaFinisCfg *args);

/**
 * @brief Dispatch table instance named verba_textus.
 */
MMGR_NS VerbaScriboTextusNs verba_textus MMGR_UNUSED = {
    .put_n = mmgr_verba_put_n,
    .put = mmgr_verba_put,
    .put_clip = mmgr_verba_put_clip,
    .xml = mmgr_verba_xml,
    .json = mmgr_verba_json,
};

/**
 * @brief Dispatch table instance named verba_littera.
 */
MMGR_NS VerbaScriboLitteraNs verba_littera MMGR_UNUSED = {
    .ch = mmgr_verba_ch,
};

/**
 * @brief Dispatch table instance named verba_numerus.
 */
MMGR_NS VerbaScriboNumerusNs verba_numerus MMGR_UNUSED = {
    .u64_clip = mmgr_verba_u64_clip,
    .uint = mmgr_verba_uint,
    .u32w = mmgr_verba_u32w,
    .hex = mmgr_verba_hex,
    .u32 = mmgr_verba_u32,
    .u64 = mmgr_verba_u64,
    .i64 = mmgr_verba_i64,
};

/**
 * @brief Dispatch table instance named verba_fractio.
 */
MMGR_NS VerbaScriboFractioNs verba_fractio MMGR_UNUSED = {
    .g = mmgr_verba_g,
    .fixed = mmgr_verba_fixed,
    .sign_bit = mmgr_verba_sign_bit,
    .is_inf = mmgr_verba_is_inf,
    .is_nan = mmgr_verba_is_nan,
};

/**
 * @brief Dispatch table instance named verba_finis.
 */
MMGR_NS VerbaScriboFinisNs verba_finis MMGR_UNUSED = {
    .finish = mmgr_verba_finish,
    .ok = mmgr_verba_ok,
};

MMGR_FINIS_DECLS

#endif
