// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ORACLE_LIBC_H
#define MMGR_ORACLE_LIBC_H

/**
 * @file mmgr_oracle_libc.h
 * @brief Test only. Point the namespaces at libc instead of at this library.
 *
 * Build with -DMMGR_ORACLE_LIBC=1 and every entry that has a libc equivalent becomes that
 * equivalent. Nothing else changes: same headers, same namespaces, same call sites, same suites.
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
 * @warning Never define MMGR_ORACLE_LIBC in a shipped build. It is the only thing in src/ that
 *          includes a header outside the budget, and the library exists precisely so a target
 *          without a libc can still have these entries.
 *
 * NOT ORACLED, because libc has no equivalent to compare against:
 *   diff        libc reports which string sorts first, not where they first differ
 *   step_word   a resumable word at a time compare has no libc counterpart
 *   step_byte   likewise
 * Those keep the library's own implementation under this flag, so a suite that only drives them is
 * testing the same code either way.
 */

#if !defined(MMGR_ORACLE_LIBC) || !MMGR_ORACLE_LIBC
#error "mmgr_oracle_libc.h is only for a build that set MMGR_ORACLE_LIBC"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config/mmgr_config.h"

MMGR_BEGIN_DECLS

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

MMGR_END_DECLS

#endif
