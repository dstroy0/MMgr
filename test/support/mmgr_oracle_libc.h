// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ORACLE_LIBC_H
#define MMGR_ORACLE_LIBC_H

/**
 * @file mmgr_oracle_libc.h
 * @brief Test only. Point the namespaces at libc instead of at this library.
 *
 * Include this after the library headers in a test translation unit and every entry that has a
 * libc equivalent becomes that equivalent, for that file only. Nothing else changes: same headers,
 * same namespaces, same call sites, same suites.
 *
 * It lives here and not in src because a header that can replace the library with something else
 * has no business shipping with the library. Nothing in src knows this exists, and the substitution
 * is a macro over the namespace name - so a suite that includes it is oracled and a suite that does
 * not is not, with no build option and no second configure of the whole tree.
 *
 * That turns the whole existing suite into a differential test at no cost. A case that passes both
 * ways is testing the contract. A case that passes with mmgr and fails with libc is either encoding
 * a quirk of ours or catching a place where we differ from the thing everyone else agrees with.
 * libc is the oracle - not because this library is untrusted, but because a str or mem entry that
 * has shipped for decades has been beaten on harder than anything written here.
 *
 * These are drop-in aliases. Where a signature differs it is a cap or a fold flag that libc spells
 * as a separate function, and the adapter below is the whole difference.
 *
 * This is test scaffolding and it lives in test/. It used to sit in src/ behind a library build
 * option, which meant anything that shipped the library also shipped a switch that replaced the
 * library's string, memory and render surface with libc's. Nothing in src knows this file exists
 * now, and it pulls in headers - ctype, math, stdio, string - that the library itself is written to
 * do without, which is the point: a target with no libc still gets these entries from src.
 *
 * NOT ORACLED, because libc has no equivalent to compare against:
 *   diff        libc reports which string sorts first, not where they first differ
 *   step_word   a resumable word at a time compare has no libc counterpart
 *   step_byte   likewise
 * Those keep the library's own implementation under this flag, so a suite that only drives them is
 * testing the same code either way.
 */

#if !defined(MMGR_TEST_ORACLE) || !MMGR_TEST_ORACLE
#error "mmgr_oracle_libc.h is only for a build that set MMGR_TEST_ORACLE"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/* ---------------------------------------------------------------------------------------------
 * memoria_operor
 * ------------------------------------------------------------------------------------------- */

MMGR_INLINE void mmgr_oracle_cpy(void *dst, const void *src, size_t n)
{
    memcpy(dst, src, n);
}

MMGR_INLINE void mmgr_oracle_move(void *dst, const void *src, size_t n)
{
    memmove(dst, src, n);
}

/*
 * cmp needs no adapter at all. memcmp is int (const void *, const void *, size_t), which is the
 * entry's signature exactly, so the namespace points straight at it - see the table wiring.
 */

MMGR_INLINE const void *mmgr_oracle_chr(const void *p, size_t n, uint8_t c)
{
    return memchr(p, (int)c, n);
}

MMGR_INLINE void mmgr_oracle_set(void *dst, unsigned char v, size_t n)
{
    memset(dst, (int)v, n);
}

MMGR_INLINE void mmgr_oracle_zero(void *dst, size_t n)
{
    memset(dst, 0, n);
}

/* ---------------------------------------------------------------------------------------------
 * cellularum_laboro
 * ------------------------------------------------------------------------------------------- */

MMGR_INLINE size_t mmgr_oracle_len(const char *s, size_t nul_cap)
{
    return strnlen(s, nul_cap);
}

/** @brief Case folded compare of one byte, the way the C library spells it. */
MMGR_INLINE int mmgr_oracle_fold_eq(unsigned char a, unsigned char b)
{
    return tolower(a) == tolower(b);
}

/**
 * @brief strstr, or a folded search built from the same primitives.
 *
 * strcasestr is a GNU and BSD extension rather than C, so it is not assumed. The folded path is
 * built out of tolower and memcmp so the oracle stays libc even where the one-call spelling is
 * missing.
 */
MMGR_INLINE const char *mmgr_oracle_find(const char *hay, size_t read_cap, const char *needle, size_t needle_cap,
                                         mmgr_bool ci)
{
    const size_t hlen = strnlen(hay, read_cap);
    const size_t nlen = strnlen(needle, needle_cap);

    if (nlen == 0u)
    {
        return hay;
    }
    if (nlen > hlen)
    {
        return NULL;
    }
    if (!ci)
    {
        return strstr(hay, needle);
    }
    for (size_t i = 0; i + nlen <= hlen; i++)
    {
        size_t k = 0;
        while (k < nlen && mmgr_oracle_fold_eq((unsigned char)hay[i + k], (unsigned char)needle[k]))
        {
            k++;
        }
        if (k == nlen)
        {
            return hay + i;
        }
    }
    return NULL;
}

MMGR_INLINE mmgr_bool mmgr_oracle_has(const char *hay, size_t read_cap, const char *needle, size_t needle_cap,
                                      mmgr_bool ci)
{
    return (mmgr_bool)(mmgr_oracle_find(hay, read_cap, needle, needle_cap, ci) != NULL);
}

MMGR_INLINE mmgr_bool mmgr_oracle_eq(const char *a, const char *b, size_t read_cap, mmgr_bool ci)
{
    const size_t la = strnlen(a, read_cap);
    const size_t lb = strnlen(b, read_cap);

    if (la != lb)
    {
        return MMGR_FALSE;
    }
    if (!ci)
    {
        return (mmgr_bool)(memcmp(a, b, la) == 0);
    }
    for (size_t i = 0; i < la; i++)
    {
        if (!mmgr_oracle_fold_eq((unsigned char)a[i], (unsigned char)b[i]))
        {
            return MMGR_FALSE;
        }
    }
    return MMGR_TRUE;
}

MMGR_INLINE mmgr_bool mmgr_oracle_starts(const char *s, const char *pre, size_t read_cap, mmgr_bool ci)
{
    const size_t lp = strnlen(pre, read_cap);
    const size_t ls = strnlen(s, read_cap);

    if (lp > ls)
    {
        return MMGR_FALSE;
    }
    if (!ci)
    {
        return (mmgr_bool)(memcmp(s, pre, lp) == 0);
    }
    for (size_t i = 0; i < lp; i++)
    {
        if (!mmgr_oracle_fold_eq((unsigned char)s[i], (unsigned char)pre[i]))
        {
            return MMGR_FALSE;
        }
    }
    return MMGR_TRUE;
}

MMGR_INLINE const char *mmgr_oracle_strchr(const char *s, size_t nul_cap, uint8_t c)
{
    (void)nul_cap;
    return strchr(s, (int)c);
}

/** @brief strlcpy's contract out of libc pieces: bounded, terminated, returns what it wrote. */
MMGR_INLINE size_t mmgr_oracle_copy(char *dst, const char *src, size_t dst_cap)
{
    if (dst_cap == 0u)
    {
        return 0u;
    }
    const size_t n = strnlen(src, dst_cap - 1u);
    memcpy(dst, src, n);
    dst[n] = '\0';
    return n;
}

MMGR_INLINE mmgr_bool mmgr_oracle_ws(char c)
{
    return (mmgr_bool)(isspace((unsigned char)c) != 0);
}

MMGR_INLINE mmgr_bool mmgr_oracle_digit(char c)
{
    return (mmgr_bool)(isdigit((unsigned char)c) != 0);
}

/*
 * The strto family writes back a char *, and these entries hand out a const char *. Casting the
 * out-parameter would discard const on the pointer being written through, which is what -Wcast-qual
 * objects to and it is right to. A local of libc's type takes the write and the const is added on
 * the way out, which is the direction that is always sound.
 */
MMGR_INLINE long mmgr_oracle_to_long(const char *s, const char **end)
{
    char *e = NULL;
    const long v = strtol(s, &e, 10);
    if (end != NULL)
    {
        *end = e;
    }
    return v;
}

MMGR_INLINE unsigned long mmgr_oracle_to_ulong(const char *s, const char **end)
{
    char *e = NULL;
    const unsigned long v = strtoul(s, &e, 10);
    if (end != NULL)
    {
        *end = e;
    }
    return v;
}

MMGR_INLINE double mmgr_oracle_to_double(const char *s, const char **end)
{
    char *e = NULL;
    const double v = strtod(s, &e);
    if (end != NULL)
    {
        *end = e;
    }
    return v;
}

MMGR_INLINE float mmgr_oracle_to_float(const char *s, const char **end)
{
    char *e = NULL;
    const float v = strtof(s, &e);
    if (end != NULL)
    {
        *end = e;
    }
    return v;
}

MMGR_FINIS_DECLS


/* ---------------------------------------------------------------------------------------------
 * The substitution
 *
 * Each table is a second instance beside the library's own, and the macro takes the name over for
 * whatever includes this. The library's instance is still there and still correct; a call site
 * that says cellul in this translation unit reaches the one below instead.
 * ------------------------------------------------------------------------------------------- */

#include <math.h>
#include <stdio.h>


/*
 * Test only. The parse side of this library was already oracled through strtod and strtof; these
 * are the render side and the three predicates, which is the last real numeric logic that was only
 * ever tested against itself.
 *
 * g and fixed take a builder rather than a buffer, so the adapter is a stack buffer and a put_n.
 * The formatting itself - every digit, every rounding decision - is libc's.
 */
MMGR_INLINE void mmgr_oracle_verba_g(mmgr_verba *b, double v, unsigned sig)
{
    char tmp[64];
    /* Mirror the entry's behavior, not printf's. How many digits get asked for is this
       library's contract - no digits means one, and the count stops at MMGR_G_MAX_SIG - so the
       adapter normalizes the same way before handing over. What happens to those digits after
       that is printf's business, which is the whole reason the oracle exists. */
    if (sig == 0u)
    {
        sig = 1u;
    }
    if (sig > MMGR_G_MAX_SIG)
    {
        sig = MMGR_G_MAX_SIG;
    }
    const int n = snprintf(tmp, sizeof tmp, "%.*g", (int)sig, v);
    if (n > 0)
    {
        mmgr_verba_put_n(b, tmp, (size_t)n);
    }
}

MMGR_INLINE void mmgr_oracle_verba_fixed(mmgr_verba *b, double v, unsigned decimals)
{
    char tmp[64];
    /* Same rule as g: the count is normalized the library's way, and the digits are printf's. */
    if (decimals > MMGR_FIXED_MAX_DECIMALS)
    {
        decimals = MMGR_FIXED_MAX_DECIMALS;
    }
    const int n = snprintf(tmp, sizeof tmp, "%.*f", (int)decimals, v);
    if (n > 0)
    {
        mmgr_verba_put_n(b, tmp, (size_t)n);
    }
}

/*
 * The three predicates are type generic macros. Their dispatch narrows in an arm this call
 * never takes, and the warning is about libc's expansion rather than about anything here.
 */
MMGR_DIAG_PUSH
MMGR_DIAG_IGNORE("-Wfloat-conversion")
MMGR_INLINE mmgr_bool mmgr_oracle_signbit(double v)
{
    return (mmgr_bool)(signbit(v) != 0);
}

MMGR_INLINE mmgr_bool mmgr_oracle_isinf(double v)
{
    return (mmgr_bool)(isinf(v) != 0);
}

MMGR_INLINE mmgr_bool mmgr_oracle_isnan(double v)
{
    return (mmgr_bool)(isnan(v) != 0);
}

MMGR_DIAG_POP

/** @brief Module namespace, with the float entries pointed at libc. */

/** @brief cellularum_laboro, pointed at libc where libc has an equivalent.
 *
 * diff, step_word and step_byte keep this library's own implementation because libc has nothing to
 * compare them against. */
MMGR_NS CellularumLaboroNs cellul_oracle MMGR_UNUSED = {
    .len = mmgr_oracle_len,
    .diff = mmgr_cellul_diff,
    .eq = mmgr_oracle_eq,
    .starts = mmgr_oracle_starts,
    .find = mmgr_oracle_find,
    .has = mmgr_oracle_has,
    .chr = mmgr_oracle_strchr,
    .copy = mmgr_oracle_copy,
    .step_word = mmgr_cellul_step_word,
    .step_byte = mmgr_cellul_step_byte,
    .ws = mmgr_oracle_ws,
    .digit = mmgr_oracle_digit,
    .to_long = mmgr_oracle_to_long,
    .to_ulong = mmgr_oracle_to_ulong,
    .to_double = mmgr_oracle_to_double,
    .to_float = mmgr_oracle_to_float,
};

/** @brief memoria_operor, pointed at libc. Every entry here has a direct equivalent. */
MMGR_NS MemoriaOperorNs memor_oracle MMGR_UNUSED = {
    .cpy = mmgr_oracle_cpy,
    .move = mmgr_oracle_move,
    .cmp = memcmp,
    .chr = mmgr_oracle_chr,
    .set = mmgr_oracle_set,
    .zero = mmgr_oracle_zero,
};

/** @brief verba_scribo's render side and its three predicates, pointed at printf and math.h.
 *
 * The parse side was already oracled through strtod and strtof. These are the last of the numeric
 * logic that was only ever tested against itself. The append entries stay this library's own:
 * printf has no equivalent for appending into a caller's buffer with a latch. */
MMGR_NS VerbaScriboNs verba_oracle MMGR_UNUSED = {.put_n = mmgr_verba_put_n,
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
                                                  .sign_bit = mmgr_oracle_signbit,
                                                  .is_inf = mmgr_oracle_isinf,
                                                  .is_nan = mmgr_oracle_isnan,
                                                  .g = mmgr_oracle_verba_g,
                                                  .fixed = mmgr_oracle_verba_fixed,
                                                  .json = mmgr_verba_json,
                                                  .finish = mmgr_verba_finish};

/** @name Take the names over for this translation unit.
 *  @brief After this, cellul, memor and verba reach the tables above. A file that does not include
 *         this header is not affected in any way.
 *  @{ */
#define cellul cellul_oracle
#define memor memor_oracle
#define verba verba_oracle
/** @} */

#endif
