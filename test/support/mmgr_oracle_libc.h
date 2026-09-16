#ifndef MMGR_ORACLE_LIBC_H
#define MMGR_ORACLE_LIBC_H

#if !defined(MMGR_TEST_ORACLE) || !MMGR_TEST_ORACLE
#error "mmgr_oracle_libc.h is only for a build that set MMGR_TEST_ORACLE"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config/mmgr_config.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_scribo.h"

EMBED_BEGIN_DECLS

EMBED_INLINE void mmgr_oracle_cpy(void *dst, const void *src, size_t n)
{
    memcpy(dst, src, n);
}

EMBED_INLINE void mmgr_oracle_move(void *dst, const void *src, size_t n)
{
    memmove(dst, src, n);
}

EMBED_INLINE const void *mmgr_oracle_chr(const void *p, size_t n, uint8_t c)
{
    return memchr(p, (int)c, n);
}

EMBED_INLINE void mmgr_oracle_set(void *dst, unsigned char v, size_t n)
{
    memset(dst, (int)v, n);
}

EMBED_INLINE void mmgr_oracle_zero(void *dst, size_t n)
{
    memset(dst, 0, n);
}

EMBED_INLINE size_t mmgr_oracle_len_raw(const char *s, size_t nul_cap)
{
    return strnlen(s, nul_cap);
}

EMBED_INLINE int mmgr_oracle_fold_eq(unsigned char a, unsigned char b)
{
    return tolower(a) == tolower(b);
}

EMBED_INLINE const char *mmgr_oracle_find_raw(const char *hay, size_t read_cap, const char *needle, size_t needle_cap,
                                              embed_bool ci)
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

EMBED_INLINE embed_bool mmgr_oracle_has_raw(const char *hay, size_t read_cap, const char *needle, size_t needle_cap,
                                            embed_bool ci)
{
    return (embed_bool)(mmgr_oracle_find_raw(hay, read_cap, needle, needle_cap, ci) != NULL);
}

EMBED_INLINE embed_bool mmgr_oracle_eq_raw(const char *a, const char *b, size_t read_cap, embed_bool ci)
{
    const size_t la = strnlen(a, read_cap);
    const size_t lb = strnlen(b, read_cap);

    if (la != lb)
    {
        return EMBED_FALSE;
    }
    if (!ci)
    {
        return (embed_bool)(memcmp(a, b, la) == 0);
    }
    for (size_t i = 0; i < la; i++)
    {
        if (!mmgr_oracle_fold_eq((unsigned char)a[i], (unsigned char)b[i]))
        {
            return EMBED_FALSE;
        }
    }
    return EMBED_TRUE;
}

EMBED_INLINE embed_bool mmgr_oracle_starts_raw(const char *s, const char *pre, size_t read_cap, embed_bool ci)
{
    const size_t lp = strnlen(pre, read_cap);
    const size_t ls = strnlen(s, read_cap);

    if (lp > ls)
    {
        return EMBED_FALSE;
    }
    if (!ci)
    {
        return (embed_bool)(memcmp(s, pre, lp) == 0);
    }
    for (size_t i = 0; i < lp; i++)
    {
        if (!mmgr_oracle_fold_eq((unsigned char)s[i], (unsigned char)pre[i]))
        {
            return EMBED_FALSE;
        }
    }
    return EMBED_TRUE;
}

EMBED_INLINE const char *mmgr_oracle_strchr_raw(const char *s, size_t nul_cap, uint8_t c)
{
    (void)nul_cap;
    return strchr(s, (int)c);
}

EMBED_INLINE size_t mmgr_oracle_copy_raw(char *dst, const char *src, size_t dst_cap)
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

EMBED_INLINE embed_bool mmgr_oracle_ws_raw(char c)
{
    return (embed_bool)(isspace((unsigned char)c) != 0);
}

EMBED_INLINE embed_bool mmgr_oracle_digit_raw(char c)
{
    return (embed_bool)(isdigit((unsigned char)c) != 0);
}

EMBED_INLINE long mmgr_oracle_to_long_raw(const char *s, const char **end)
{
    char *e = NULL;
    const long v = strtol(s, &e, 10);
    if (end != NULL)
    {
        *end = e;
    }
    return v;
}

EMBED_INLINE unsigned long mmgr_oracle_to_ulong_raw(const char *s, const char **end)
{
    char *e = NULL;
    const unsigned long v = strtoul(s, &e, 10);
    if (end != NULL)
    {
        *end = e;
    }
    return v;
}

EMBED_INLINE double mmgr_oracle_to_double_raw(const char *s, const char **end)
{
    char *e = NULL;
    const double v = strtod(s, &e);
    if (end != NULL)
    {
        *end = e;
    }
    return v;
}

EMBED_INLINE float mmgr_oracle_to_float_raw(const char *s, const char **end)
{
    char *e = NULL;
    const float v = strtof(s, &e);
    if (end != NULL)
    {
        *end = e;
    }
    return v;
}

EMBED_INLINE CatenaFinitaCfg mmgr_oracle_init(const CatenaFinitaCfg *c)
{
    return *c;
}

EMBED_INLINE size_t mmgr_oracle_len(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_len_raw(c->s + c->at, c->cap - c->at);
}

EMBED_INLINE const char *mmgr_oracle_find(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_find_raw(c->s, c->cap, c->t, c->t_cap, c->ci);
}

EMBED_INLINE embed_bool mmgr_oracle_has(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_has_raw(c->s, c->cap, c->t, c->t_cap, c->ci);
}

EMBED_INLINE embed_bool mmgr_oracle_eq(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_eq_raw(c->s, c->t, c->cap, c->ci);
}

EMBED_INLINE embed_bool mmgr_oracle_starts(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_starts_raw(c->s, c->t, c->cap, c->ci);
}

EMBED_INLINE const char *mmgr_oracle_strchr(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_strchr_raw(c->s, c->cap, c->byte);
}

EMBED_INLINE size_t mmgr_oracle_copy(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_copy_raw(c->dst, c->s, c->cap);
}

EMBED_INLINE embed_bool mmgr_oracle_ws(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_ws_raw(c->s[c->at]);
}

EMBED_INLINE embed_bool mmgr_oracle_digit(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_digit_raw(c->s[c->at]);
}

EMBED_INLINE long mmgr_oracle_to_long(const TransfiguroCfg *c)
{
    return mmgr_oracle_to_long_raw(c->s, c->end);
}

EMBED_INLINE unsigned long mmgr_oracle_to_ulong(const TransfiguroCfg *c)
{
    return mmgr_oracle_to_ulong_raw(c->s, c->end);
}

EMBED_INLINE double mmgr_oracle_to_double(const TransfiguroCfg *c)
{
    return mmgr_oracle_to_double_raw(c->s, c->end);
}

EMBED_INLINE float mmgr_oracle_to_float(const TransfiguroCfg *c)
{
    return mmgr_oracle_to_float_raw(c->s, c->end);
}

EMBED_END_DECLS

#include <math.h>
#include <stdio.h>

EMBED_INLINE void mmgr_oracle_verba_g(mmgr_verba *b, double v, unsigned sig)
{
    char tmp[64];
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

EMBED_INLINE void mmgr_oracle_verba_fixed(mmgr_verba *b, double v, unsigned decimals)
{
    char tmp[64];
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

MMGR_DIAG_PUSH
MMGR_DIAG_IGNORE("-Wfloat-conversion")
EMBED_INLINE embed_bool mmgr_oracle_signbit(double v)
{
    return (embed_bool)(signbit(v) != 0);
}

EMBED_INLINE embed_bool mmgr_oracle_isinf(double v)
{
    return (embed_bool)(isinf(v) != 0);
}

EMBED_INLINE embed_bool mmgr_oracle_isnan(double v)
{
    return (embed_bool)(isnan(v) != 0);
}

MMGR_DIAG_POP

EMBED_TABLE_STORAGE CellularumLaboroNs cellul_oracle EMBED_UNUSED = {
    .init = mmgr_oracle_init,
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

EMBED_TABLE_STORAGE MemoriaOperorNs memor_oracle EMBED_UNUSED = {
    .cpy = mmgr_oracle_cpy,
    .move = mmgr_oracle_move,
    .cmp = memcmp,
    .chr = mmgr_oracle_chr,
    .set = mmgr_oracle_set,
    .zero = mmgr_oracle_zero,
};

EMBED_TABLE_STORAGE VerbaScriboNs verba_oracle EMBED_UNUSED = {.put_n = mmgr_verba_put_n,
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

#define cellul cellul_oracle
#define memor memor_oracle
#define verba verba_oracle

#endif
