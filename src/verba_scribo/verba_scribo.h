// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_VERBA_SCRIBO_H
#define MMGR_VERBA_SCRIBO_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

/**
 * @file verba_scribo.h
 * @brief Text building over caller memory. No allocation and no format string.
 *
 * Nothing is parsed at run time. Each value goes through the entry for its own type, so a wrong
 * type is a compile error rather than a crash.
 */

/** @brief Append a string literal, length from sizeof. */
#define mmgr_verba_lit(b, s) mmgr_verba_put_n((b), (s), sizeof(s) - 1)

/**
 * @brief Text builder over caller memory.
 *
 * @c ok latches false once a write runs out of room, so a run of appends is checked once at the
 * end. The buffer is always terminated.
 */
typedef struct
{
    char *p;
    size_t cap;
    size_t len;
    mmgr_bool ok;
} mmgr_verba;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    void (*put_n)(mmgr_verba *b, const char *s, size_t sl);
    void (*put)(mmgr_verba *b, const char *s);
    void (*put_clip)(mmgr_verba *b, const char *s);
    void (*u64_clip)(mmgr_verba *b, uint64_t v, uint8_t columns);
    void (*xml)(mmgr_verba *b, const char *s);
    void (*ch)(mmgr_verba *b, char c);
    void (*uint)(mmgr_verba *b, uint64_t v, unsigned base, unsigned min_digits);
    void (*u32w)(mmgr_verba *b, uint32_t v, unsigned min_digits);
    void (*hex)(mmgr_verba *b, uint64_t v, unsigned min_digits);
    void (*u32)(mmgr_verba *b, uint32_t v);
    void (*u64)(mmgr_verba *b, uint64_t v);
    void (*i64)(mmgr_verba *b, int64_t v);
    mmgr_bool (*sign_bit)(double v);
    mmgr_bool (*is_inf)(double v);
    mmgr_bool (*is_nan)(double v);
    void (*g)(mmgr_verba *b, double v, unsigned sig);
    void (*fixed)(mmgr_verba *b, double v, unsigned decimals);
    void (*json)(mmgr_verba *b, const char *s);
    size_t (*finish)(mmgr_verba *b);
} VerbaScriboNs;
MMGR_NS_LAYOUT(VerbaScriboNs, put_n, put, put_clip, u64_clip, xml, ch, uint, u32w, hex, u32, u64, i64, sign_bit, is_inf, is_nan, g, fixed, json, finish);

/**
 * @brief Append @p sl bytes.
 * @param b Builder.
 * @param s Bytes.
 * @param sl Byte count.
 */
void mmgr_verba_put_n(mmgr_verba *b, const char *s, size_t sl);
/**
 * @brief Append a string.
 * @param b Builder.
 * @param s String.
 */
void mmgr_verba_put(mmgr_verba *b, const char *s);
/**
 * @brief Append a string, truncating rather than latching an overflow.
 * @param b Builder.
 * @param s String.
 */
void mmgr_verba_put_clip(mmgr_verba *b, const char *s);
/**
 * @brief Append an unsigned value in a fixed column width, truncating if it does not fit.
 * @param b Builder.
 * @param v Value.
 * @param columns Field width.
 */
void mmgr_verba_u64_clip(mmgr_verba *b, uint64_t v, uint8_t columns);
/**
 * @brief Append a string with XML metacharacters escaped.
 * @param b Builder.
 * @param s String.
 */
void mmgr_verba_xml(mmgr_verba *b, const char *s);
/**
 * @brief Append one character.
 * @param b Builder.
 * @param c Character.
 */
void mmgr_verba_ch(mmgr_verba *b, char c);
/**
 * @brief Append an unsigned value in @p base.
 * @param b Builder.
 * @param v Value.
 * @param base 2 through 16.
 * @param min_digits Zero pad to at least this many.
 */
void mmgr_verba_uint(mmgr_verba *b, uint64_t v, unsigned base, unsigned min_digits);
/**
 * @brief Append a uint32 in decimal, zero padded.
 * @param b Builder.
 * @param v Value.
 * @param min_digits Pad to at least this many.
 */
void mmgr_verba_u32w(mmgr_verba *b, uint32_t v, unsigned min_digits);
/**
 * @brief Append a value in lower case hex.
 * @param b Builder.
 * @param v Value.
 * @param min_digits Zero pad to at least this many.
 */
void mmgr_verba_hex(mmgr_verba *b, uint64_t v, unsigned min_digits);
/**
 * @brief Append a uint32 in decimal.
 * @param b Builder.
 * @param v Value.
 */
void mmgr_verba_u32(mmgr_verba *b, uint32_t v);
/**
 * @brief Append a uint64 in decimal.
 * @param b Builder.
 * @param v Value.
 */
void mmgr_verba_u64(mmgr_verba *b, uint64_t v);
/**
 * @brief Append an int64 in decimal.
 * @param b Builder.
 * @param v Value.
 */
void mmgr_verba_i64(mmgr_verba *b, int64_t v);
/**
 * @brief Is the sign bit set. Answers for negative zero too.
 * @param v Value.
 * @return MMGR_TRUE if set.
 */
mmgr_bool mmgr_signbit(double v);
/**
 * @brief Is @p v an infinity.
 * @param v Value.
 * @return MMGR_TRUE if it is.
 */
mmgr_bool mmgr_isinf(double v);
/**
 * @brief Is @p v a NaN.
 * @param v Value.
 * @return MMGR_TRUE if it is.
 */
mmgr_bool mmgr_isnan(double v);
/**
 * @brief Append a double in the shorter of fixed and exponent form.
 * @param b Builder.
 * @param v Value.
 * @param sig Significant digits.
 */
void mmgr_verba_g(mmgr_verba *b, double v, unsigned sig);
/**
 * @brief Append a double in fixed form.
 * @param b Builder.
 * @param v Value.
 * @param decimals Digits after the point.
 */
void mmgr_verba_fixed(mmgr_verba *b, double v, unsigned decimals);
/**
 * @brief Append a string with JSON metacharacters escaped.
 * @param b Builder.
 * @param s String.
 */
void mmgr_verba_json(mmgr_verba *b, const char *s);
/**
 * @brief Terminate and report the length.
 * @param b Builder.
 * @return Length written, or 0 if the builder overflowed.
 */
size_t mmgr_verba_finish(mmgr_verba *b);

/** @brief Module namespace. */
MMGR_NS VerbaScriboNs verba MMGR_UNUSED = {.put_n = mmgr_verba_put_n,
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
                                                            .sign_bit = mmgr_signbit,
                                                            .is_inf = mmgr_isinf,
                                                            .is_nan = mmgr_isnan,
                                                            .g = mmgr_verba_g,
                                                            .fixed = mmgr_verba_fixed,
                                                            .json = mmgr_verba_json,
                                                            .finish = mmgr_verba_finish};

MMGR_END_DECLS

#endif
