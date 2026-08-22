// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "cellularum_laboro/cellularum_laboro.h"
#include "impensa_ancorae_acus/impensa_ancorae_acus.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "transformo/transformo.h"
#include "fractio/fractio.h"
#include "verbum_scrutor/verbum_scrutor.h"
#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"

typedef struct
{
    const char *const s;
    const size_t cap;
    const char *const t;
    const size_t t_cap;
    char *const dst;
    const size_t at;
    const uint8_t byte;
    const mmgr_bool ci;
    const int end_wins;
    const uint8_t **const out;
    uint32_t *const slen;

    const mmgr_scrut_word wa;
    const mmgr_scrut_word wb;
    const unsigned char ca;
    const unsigned char cb;

    const size_t nlen;
    size_t *const rows;
    const size_t k;
    const unsigned fmask;
    size_t *const off;

    const char **const end;
    const char **const cur;
    int *const exp;

    const uint8_t *const m;
    const uint32_t mlen;
    uint8_t *const field;
    const size_t fieldlen;
} CellulCtx;

/* --------------------------------------------------------------- family B: one loaded step */

MMGR_INLINE int cellul_step_word_cs(const CellulCtx *c)
{
    const mmgr_scrut_word x = c->wa ^ c->wb;
    const mmgr_scrut_word z = scrut.has_zero(c->wa);

    if ((x | z) == 0)
    {
        return MMGR_SWAR_GO;
    }

    size_t dl = MMGR_SWAR_BYTES;
    if (x != 0)
    {
        dl = scrut.zero_lane(MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(x));
    }
    size_t el = MMGR_SWAR_BYTES;
    if (z != 0)
    {
        el = scrut.zero_lane(z);
    }
    if (c->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

MMGR_INLINE int cellul_step_word_ci(const CellulCtx *c)
{
    const mmgr_scrut_word x = scrut.xor_(c->wa, c->wb, MMGR_TRUE);
    const mmgr_scrut_word z = scrut.has_zero(c->wa);

    if ((x | z) == 0)
    {
        return MMGR_SWAR_GO;
    }

    size_t dl = MMGR_SWAR_BYTES;
    if (x != 0)
    {
        dl = scrut.zero_lane(MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(x));
    }
    size_t el = MMGR_SWAR_BYTES;
    if (z != 0)
    {
        el = scrut.zero_lane(z);
    }
    if (c->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

MMGR_INLINE int cellul_step_byte_cs(const CellulCtx *c)
{
    if (c->ca == 0)
    {
        if (c->ca == c->cb)
        {
            return MMGR_SWAR_YES;
        }
        return (c->end_wins != 0) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    if (c->ca != c->cb)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

MMGR_INLINE int cellul_step_byte_ci(const CellulCtx *c)
{
    const mmgr_scrut_word d = scrut.xor_((mmgr_scrut_word)c->ca, (mmgr_scrut_word)c->cb, MMGR_TRUE);

    if (c->ca == 0)
    {
        if (d == 0)
        {
            return MMGR_SWAR_YES;
        }
        return (c->end_wins != 0) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    if (d != 0)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

/* ------------------------------------------------------------------ family A: bounded reads */

MMGR_INLINE mmgr_bool cellul_is_ws(char ch)
{
    return (ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r') || (ch == '\f') || (ch == '\v');
}

MMGR_INLINE mmgr_bool cellul_is_digit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

MMGR_INLINE size_t cellul_len(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word m = scrut.has_zero(scrut.load(c->s + at)) & mmgr_scrut_tail_mask(c->cap, wi);
        if (m != 0)
        {
            return at + scrut.zero_lane(m);
        }
    }
    return c->cap;
}

MMGR_INLINE const char *cellul_chr(const CellulCtx *c)
{
    if (c->byte == 0u)
    {
        return c->s + cellul_len(c);
    }

    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word w = scrut.load(c->s + at);
        const mmgr_scrut_word keep = mmgr_scrut_tail_mask(c->cap, wi);
        const mmgr_scrut_word end = scrut.has_zero(w) & keep;
        const mmgr_scrut_word hit = scrut.eq(w, c->byte, MMGR_FALSE) & keep & mmgr_scrut_lanes_before(end);

        if (hit != 0)
        {
            return c->s + at + scrut.zero_lane(hit);
        }
        if (end != 0)
        {
            return NULL;
        }
    }
    return NULL;
}

MMGR_INLINE size_t cellul_diff_cs(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word d = scrut.load(c->s + at) ^ scrut.load(c->t + at);
        const mmgr_scrut_word m =
            (MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(d)) & mmgr_scrut_tail_mask(c->cap, wi);
        if (m != 0)
        {
            return at + scrut.zero_lane(m);
        }
    }
    return c->cap;
}

MMGR_INLINE size_t cellul_diff_ci(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word d = scrut.xor_(scrut.load(c->s + at), scrut.load(c->t + at), MMGR_TRUE);
        const mmgr_scrut_word m =
            (MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(d)) & mmgr_scrut_tail_mask(c->cap, wi);
        if (m != 0)
        {
            return at + scrut.zero_lane(m);
        }
    }
    return c->cap;
}

MMGR_INLINE mmgr_bool cellul_agree_cs(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word keep = mmgr_scrut_tail_mask(c->cap, wi);
        const mmgr_scrut_word wa = scrut.load(c->s + at);
        const mmgr_scrut_word wb = scrut.load(c->t + at);
        const mmgr_scrut_word z = scrut.has_zero(wa) & keep;
        const mmgr_scrut_word x = (MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(wa ^ wb)) & keep;

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? scrut.zero_lane(z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? scrut.zero_lane(x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(c->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return (mmgr_bool)(c->end_wins != 0);
}

MMGR_INLINE mmgr_bool cellul_agree_ci(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word keep = mmgr_scrut_tail_mask(c->cap, wi);
        const mmgr_scrut_word wa = scrut.load(c->s + at);
        const mmgr_scrut_word wb = scrut.load(c->t + at);
        const mmgr_scrut_word z = scrut.has_zero(wa) & keep;
        const mmgr_scrut_word x =
            (MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(scrut.xor_(wa, wb, MMGR_TRUE))) & keep;

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? scrut.zero_lane(z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? scrut.zero_lane(x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(c->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return (mmgr_bool)(c->end_wins != 0);
}

MMGR_INLINE uint8_t cellul_ancorae_fold(const CellulCtx *c)
{
    const uint8_t b = (uint8_t)c->t[c->k];

    if (c->ci && (b >= (uint8_t)'A') && (b <= (uint8_t)'Z'))
    {
        return (uint8_t)(b | 0x20u);
    }
    return b;
}

MMGR_INLINE size_t cellul_pick_rows(const CellulCtx *c)
{
    const size_t limit = (c->nlen > MMGR_SWAR_BYTES) ? MMGR_SWAR_BYTES : c->nlen;
    const size_t want = (limit > MMGR_SIEVE_ROWS) ? MMGR_SIEVE_ROWS : limit;

    for (size_t r = 0; r < want; ++r)
    {
        size_t best = 0;
        uint8_t best_cost = 255;

        for (size_t k = 0; k < limit; ++k)
        {
            size_t taken = 0;

            for (size_t q = 0; q < r; ++q)
            {
                if (c->rows[q] == k)
                {
                    taken = 1;
                }
            }

            const uint8_t cost =
                mmgr_ancorae_impensa(cellul_ancorae_fold(&(CellulCtx){.t = c->t, .k = k, .ci = c->ci}));
            if (!taken && (cost < best_cost))
            {
                best_cost = cost;
                best = k;
            }
        }
        c->rows[r] = best;
    }
    return want;
}

MMGR_INLINE const char *cellul_find_core(const CellulCtx *c, mmgr_bool ci)
{
    const char *const hay = c->s;
    const char *const needle = c->t;
    const size_t read_cap = c->cap;

    const size_t nlen = cellul_len(&(CellulCtx){.s = needle, .cap = c->t_cap});

    if (nlen == 0u)
    {
        return hay;
    }
    if (nlen > read_cap)
    {
        return NULL;
    }

    size_t rows[MMGR_SIEVE_ROWS];
    const size_t nrows = cellul_pick_rows(&(CellulCtx){.t = needle, .nlen = nlen, .rows = rows, .ci = ci});

    const size_t take = (nlen > MMGR_SWAR_BYTES) ? MMGR_SWAR_BYTES : nlen;
    const mmgr_scrut_word nmask = mmgr_scrut_bytes_below(take);
    const mmgr_scrut_word nraw = scrut.load(needle) & nmask;
    const mmgr_scrut_word nword = ci ? (mmgr_scrut_fold_lower(nraw) & nmask) : nraw;

    const size_t starts = read_cap - nlen + 1u;

    size_t maxrow = rows[0];

    for (size_t r = 1; r < nrows; ++r)
    {
        if (rows[r] > maxrow)
        {
            maxrow = rows[r];
        }
    }

    const size_t tail = (nlen > take) ? (mmgr_scrut_words(nlen - take) * MMGR_SWAR_BYTES) : 0u;
    const size_t verify_reach = (MMGR_SWAR_BYTES - 1u) + take + tail;
    const size_t ancorae_reach = maxrow + MMGR_SWAR_BYTES;
    const size_t reach = (ancorae_reach > verify_reach) ? ancorae_reach : verify_reach;

    size_t safe = (read_cap >= reach) ? ((read_cap - reach) + 1u) : 0u;

    if (safe > starts)
    {
        safe = starts;
    }

    const size_t nw = safe / MMGR_SWAR_BYTES;

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word end = scrut.has_zero(scrut.load(hay + at));
        mmgr_scrut_word m;

        m = scrut.eq(scrut.load(hay + at + rows[0]), (uint8_t)needle[rows[0]], ci);

        for (size_t r = 1; r < nrows; ++r)
        {
            m &= scrut.eq(scrut.load(hay + at + rows[r]), (uint8_t)needle[rows[r]], ci);
        }

        if (end != 0)
        {
            m &= mmgr_scrut_lanes_before(end);
        }

        while (m != 0)
        {
            const size_t k = at + scrut.zero_lane(m);
            const mmgr_scrut_word cw = scrut.load(hay + k);

            const mmgr_scrut_word syn =
                ((!ci || (mmgr_scrut_any_upper(cw) == 0)) ? (cw ^ nword) : scrut.xor_(cw, nword, MMGR_TRUE)) &
                nmask;

            if (syn == 0)
            {
                if (take == nlen)
                {
                    return hay + k;
                }

                const CellulCtx v = {.s = hay + k + take, .t = needle + take, .cap = nlen - take};
                const size_t d = ci ? cellul_diff_ci(&v) : cellul_diff_cs(&v);

                if (d == (nlen - take))
                {
                    return hay + k;
                }
            }
            m = mmgr_scrut_drop_first(m);
        }
        if (end != 0)
        {
            return NULL;
        }
    }

    for (size_t k = nw * MMGR_SWAR_BYTES; k < starts; ++k)
    {
        if (hay[k] == '\0')
        {
            return NULL;
        }

        size_t i = 0;
        while (i < nlen)
        {
            const unsigned char h = (unsigned char)hay[k + i];
            const CellulCtx b = {.ca = (unsigned char)needle[i], .cb = h, .end_wins = 0};

            if ((h == 0u) || ((ci ? cellul_step_byte_ci(&b) : cellul_step_byte_cs(&b)) == MMGR_SWAR_NO))
            {
                break;
            }
            ++i;
        }
        if (i == nlen)
        {
            return hay + k;
        }
    }
    return NULL;
}

MMGR_INLINE size_t cellul_copy(const CellulCtx *c)
{
    if (c->cap == 0u)
    {
        return 0u;
    }

    const size_t n = cellul_len(&(CellulCtx){.s = c->s, .cap = c->cap - 1u});

    proxim.read(c->dst, c->s, n);
    c->dst[n] = '\0';
    return n;
}

MMGR_INLINE mmgr_bool cellul_rd_str(const CellulCtx *c)
{
    const uint8_t *const buf = (const uint8_t *)c->s;
    size_t at = c->at;
    if ((at > c->cap) || ((c->cap - at) < 4u))
    {
        return MMGR_FALSE;
    }

    const uint32_t n = (uint32_t)magna_extremitas.rd(&(EndianCfg){0, buf + at, 0, MMGR_ENDIAN_32});
    at += 4u;

    if (n > (c->cap - at))
    {
        return MMGR_FALSE;
    }
    *c->out = buf + at;
    *c->slen = n;
    return MMGR_TRUE;
}

/* ---------------------------------------------------------------- family C: one conversion */

MMGR_INLINE long cellul_to_long(const CellulCtx *c)
{
    const char *p = c->s;

    while (cellul_is_ws(*p))
    {
        p++;
    }

    mmgr_bool neg = MMGR_FALSE;
    if ((*p == '+') || (*p == '-'))
    {
        neg = (*p++ == '-');
    }

    const char *const ds = p;
    unsigned long v = 0;
    while (cellul_is_digit(*p))
    {
        v = (v * 10UL) + (unsigned long)(*p++ - '0');
    }

    if (c->end != NULL)
    {
        *c->end = (p != ds) ? p : c->s;
    }
    if (neg)
    {
        return (long)(0UL - v);
    }
    return (long)v;
}

MMGR_INLINE unsigned long cellul_to_ulong(const CellulCtx *c)
{
    const char *p = c->s;

    while (cellul_is_ws(*p))
    {
        p++;
    }
    if (*p == '+')
    {
        p++;
    }

    const char *const ds = p;
    unsigned long v = 0;
    while (cellul_is_digit(*p))
    {
        v = (v * 10UL) + (unsigned long)(*p++ - '0');
    }

    if (c->end != NULL)
    {
        *c->end = (p != ds) ? p : c->s;
    }
    return v;
}

MMGR_INLINE void cellul_expo(const CellulCtx *c)
{
    const char *const mark = *c->cur;

    (*c->cur)++;

    mmgr_bool eneg = MMGR_FALSE;
    if ((**c->cur == '+') || (**c->cur == '-'))
    {
        eneg = (*(*c->cur)++ == '-');
    }
    if (!cellul_is_digit(**c->cur))
    {
        *c->cur = mark;
        return;
    }

    int ex = 0;
    while (cellul_is_digit(**c->cur))
    {
        if (ex < MMGR_MUTO_EXP_LIMIT)
        {
            ex = (ex * 10) + (**c->cur - '0');
        }
        (*c->cur)++;
    }
    *c->exp = eneg ? -ex : ex;
}

MMGR_INLINE double cellul_to_double(const CellulCtx *c)
{
    const char *p = c->s;

    while (cellul_is_ws(*p))
    {
        p++;
    }

    mmgr_bool neg = MMGR_FALSE;
    if ((*p == '+') || (*p == '-'))
    {
        neg = (*p++ == '-');
    }

    mmgr_bool any = MMGR_FALSE;
    mmgr_u64 mant = 0;
    int drop = 0;
    int over = 0;
    int lost = 0;

    while (cellul_is_digit(*p))
    {
        if (!mmgr_muto_take(mant, *p))
        {
            ++over;
            lost |= (*p != '0') ? 1 : 0;
        }
        any = MMGR_TRUE;
        ++p;
    }
    if (*p == '.')
    {
        ++p;
        while (cellul_is_digit(*p))
        {
            if (mmgr_muto_take(mant, *p))
            {
                ++drop;
            }
            else
            {
                lost |= (*p != '0') ? 1 : 0;
            }
            any = MMGR_TRUE;
            ++p;
        }
    }

    int ex = 0;
    if (any && ((*p == 'e') || (*p == 'E')))
    {
        cellul_expo(&(CellulCtx){.cur = &p, .exp = &ex});
    }

    const double val = mmgr_muto_scale(mant, ex + over - drop, lost, neg);

    if (c->end != NULL)
    {
        *c->end = any ? p : c->s;
    }
    return val;
}

MMGR_INLINE float cellul_to_float(const CellulCtx *c)
{
    return (float)cellul_to_double(c);
}

MMGR_INLINE mmgr_bool cellul_mpint_fixed(const CellulCtx *c)
{
    uint32_t off = 0;

    while ((off < c->mlen) && (c->m[off] == 0))
    {
        off++;
    }

    const uint32_t vlen = c->mlen - off;
    if (vlen > c->fieldlen)
    {
        return MMGR_FALSE;
    }
    memor.set(c->field, 0, c->fieldlen);
    memor.cpy(c->field + (c->fieldlen - vlen), c->m + off, vlen);
    return MMGR_TRUE;
}

/* ------------------------------------------------------------------------------- the entries */

CatenaFinitaCfg (mmgr_cellul_init)(const CatenaFinitaCfg *c)
{
    return *c;
}

size_t (mmgr_cellul_len)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_len, CellulCtx, .s = c->s + c->at, .cap = c->cap - c->at);
}

size_t (mmgr_cellul_diff)(const CatenaFinitaCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_diff_ci, CellulCtx, .s = c->s, .t = c->t, .cap = c->cap);
    }
    return MMGR_CALL(cellul_diff_cs, CellulCtx, .s = c->s, .t = c->t, .cap = c->cap);
}

mmgr_bool (mmgr_cellul_eq)(const CatenaFinitaCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_agree_ci, CellulCtx, .s = c->s, .t = c->t, .cap = c->cap, .end_wins = 0);
    }
    return MMGR_CALL(cellul_agree_cs, CellulCtx, .s = c->s, .t = c->t, .cap = c->cap, .end_wins = 0);
}

mmgr_bool (mmgr_cellul_starts)(const CatenaFinitaCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_agree_ci, CellulCtx, .s = c->t, .t = c->s, .cap = c->cap, .end_wins = 1);
    }
    return MMGR_CALL(cellul_agree_cs, CellulCtx, .s = c->t, .t = c->s, .cap = c->cap, .end_wins = 1);
}

const char *(mmgr_cellul_find)(const CatenaFinitaCfg *c)
{
    const CellulCtx x = {.s = c->s, .cap = c->cap, .t = c->t, .t_cap = c->t_cap};

    if (c->ci)
    {
        return cellul_find_core(&x, MMGR_TRUE);
    }
    return cellul_find_core(&x, MMGR_FALSE);
}

mmgr_bool (mmgr_cellul_has)(const CatenaFinitaCfg *c)
{
    return (mmgr_bool)((mmgr_cellul_find)(c) != NULL);
}

const char *(mmgr_cellul_chr)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_chr, CellulCtx, .s = c->s, .cap = c->cap, .byte = c->byte);
}

size_t (mmgr_cellul_copy)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_copy, CellulCtx, .dst = c->dst, .s = c->s, .cap = c->cap);
}

mmgr_bool (mmgr_cellul_ws)(const CatenaFinitaCfg *c)
{
    return cellul_is_ws(c->s[c->at]);
}

mmgr_bool (mmgr_cellul_digit)(const CatenaFinitaCfg *c)
{
    return cellul_is_digit(c->s[c->at]);
}

mmgr_bool (mmgr_cellul_rd_str)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_rd_str, CellulCtx, .s = c->s, .cap = c->cap, .at = c->at, .out = c->out,
                     .slen = c->slen);
}

int (mmgr_cellul_step_word)(const VerboProgrediorCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_step_word_ci, CellulCtx, .wa = c->wa, .wb = c->wb, .end_wins = c->end_wins);
    }
    return MMGR_CALL(cellul_step_word_cs, CellulCtx, .wa = c->wa, .wb = c->wb, .end_wins = c->end_wins);
}

int (mmgr_cellul_step_byte)(const VerboProgrediorCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_step_byte_ci, CellulCtx, .ca = c->ca, .cb = c->cb, .end_wins = c->end_wins);
    }
    return MMGR_CALL(cellul_step_byte_cs, CellulCtx, .ca = c->ca, .cb = c->cb, .end_wins = c->end_wins);
}

long (mmgr_cellul_to_long)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_long, CellulCtx, .s = c->s, .end = c->end);
}

unsigned long (mmgr_cellul_to_ulong)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_ulong, CellulCtx, .s = c->s, .end = c->end);
}

double (mmgr_cellul_to_double)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_double, CellulCtx, .s = c->s, .end = c->end);
}

float (mmgr_cellul_to_float)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_float, CellulCtx, .s = c->s, .end = c->end);
}

mmgr_bool (mmgr_cellul_mpint_fixed)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_mpint_fixed, CellulCtx, .m = c->m, .mlen = c->mlen, .field = c->field,
                     .fieldlen = c->fieldlen);
}
