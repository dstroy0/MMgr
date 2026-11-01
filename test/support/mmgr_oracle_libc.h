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

EMBED_INLINE void mmgr_oracle_cpy(const MemoriaCfg *args)
{
    memcpy(args->dst, args->src, args->bytes);
}

EMBED_INLINE void mmgr_oracle_move(const MemoriaCfg *args)
{
    memmove(args->dst, args->src, args->bytes);
}

EMBED_INLINE embed_iword mmgr_oracle_cmp(const MemoriaCfg *args)
{
    const int order = memcmp(args->src, args->other, args->bytes);

    return (order < 0) ? -1 : ((order > 0) ? 1 : 0);
}

EMBED_INLINE const void *mmgr_oracle_chr(const MemoriaCfg *args)
{
    return memchr(args->src, (int)args->val, args->bytes);
}

EMBED_INLINE void mmgr_oracle_set(const MemoriaCfg *args)
{
    memset(args->dst, (int)args->val, args->bytes);
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
    return mmgr_oracle_len_raw(c->src + c->at, c->cap - c->at);
}

EMBED_INLINE const char *mmgr_oracle_find(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_find_raw(c->src, c->cap, c->other, c->other_cap, c->ci);
}

EMBED_INLINE embed_bool mmgr_oracle_has(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_has_raw(c->src, c->cap, c->other, c->other_cap, c->ci);
}

EMBED_INLINE embed_bool mmgr_oracle_eq(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_eq_raw(c->src, c->other, c->cap, c->ci);
}

EMBED_INLINE embed_bool mmgr_oracle_starts(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_starts_raw(c->src, c->other, c->cap, c->ci);
}

EMBED_INLINE const char *mmgr_oracle_strchr(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_strchr_raw(c->src, c->cap, c->byte);
}

EMBED_INLINE size_t mmgr_oracle_copy(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_copy_raw(c->dst, c->src, c->cap);
}

EMBED_INLINE embed_bool mmgr_oracle_ws(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_ws_raw(c->src[c->at]);
}

EMBED_INLINE embed_bool mmgr_oracle_digit(const CatenaFinitaCfg *c)
{
    return mmgr_oracle_digit_raw(c->src[c->at]);
}

EMBED_INLINE long mmgr_oracle_to_long(const TransfiguroCfg *c)
{
    return mmgr_oracle_to_long_raw(c->src, c->end);
}

EMBED_INLINE unsigned long mmgr_oracle_to_ulong(const TransfiguroCfg *c)
{
    return mmgr_oracle_to_ulong_raw(c->src, c->end);
}

EMBED_INLINE double mmgr_oracle_to_double(const TransfiguroCfg *c)
{
    return mmgr_oracle_to_double_raw(c->src, c->end);
}

EMBED_INLINE float mmgr_oracle_to_float(const TransfiguroCfg *c)
{
    return mmgr_oracle_to_float_raw(c->src, c->end);
}

EMBED_END_DECLS

#include <math.h>
#include <stdio.h>

EMBED_INLINE size_t mmgr_oracle_verba_written(const VerbaFractioCfg *args, const char *text, int length)
{
    return EMBED_CALL(verba_textus.put_n, VerbaTextusCfg, .out = args->out, .cap = args->cap, .at = args->at,
                      .text = text, .text_len = (size_t)length);
}

EMBED_INLINE size_t mmgr_oracle_verba_g(const VerbaFractioCfg *args)
{
    char tmp[512];
    const unsigned sig = (args->sig == 0u) ? 1u : ((args->sig > MMGR_G_MAX_SIG) ? MMGR_G_MAX_SIG : args->sig);

    return mmgr_oracle_verba_written(args, tmp, snprintf(tmp, sizeof tmp, "%.*g", (int)sig, args->real));
}

EMBED_INLINE size_t mmgr_oracle_verba_fixed(const VerbaFractioCfg *args)
{
    char tmp[512];
    const unsigned decimals = (args->decimals > MMGR_FIXED_MAX_DECIMALS) ? MMGR_FIXED_MAX_DECIMALS : args->decimals;

    return mmgr_oracle_verba_written(args, tmp, snprintf(tmp, sizeof tmp, "%.*f", (int)decimals, args->real));
}

MMGR_DIAG_PUSH
MMGR_DIAG_IGNORE("-Wfloat-conversion")
EMBED_INLINE embed_bool mmgr_oracle_signbit(const VerbaFractioCfg *args)
{
    return (embed_bool)(signbit(args->real) != 0);
}

EMBED_INLINE embed_bool mmgr_oracle_isinf(const VerbaFractioCfg *args)
{
    return (embed_bool)(isinf(args->real) != 0);
}

EMBED_INLINE embed_bool mmgr_oracle_isnan(const VerbaFractioCfg *args)
{
    return (embed_bool)(isnan(args->real) != 0);
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
    .move_down = mmgr_oracle_move,
    .move_up = mmgr_oracle_move,
    .cmp = mmgr_oracle_cmp,
    .chr = mmgr_oracle_chr,
    .set = mmgr_oracle_set,
};

EMBED_TABLE_STORAGE VerbaScriboFractioNs verba_fractio_oracle EMBED_UNUSED = {
    .g = mmgr_oracle_verba_g,
    .fixed = mmgr_oracle_verba_fixed,
    .sign_bit = mmgr_oracle_signbit,
    .is_inf = mmgr_oracle_isinf,
    .is_nan = mmgr_oracle_isnan,
};

#define cellul cellul_oracle
#define memor memor_oracle
#define verba_fractio verba_fractio_oracle

#endif
