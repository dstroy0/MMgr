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
    const char *const src;
    const size_t cap;
    const char *const other;
    const size_t other_cap;
    char *const dst;
    const size_t at;
    const uint8_t byte;
    const mmgr_bool ci;
    const mmgr_bool end_wins;
    const uint8_t **const out;
    uint32_t *const slen;

    const mmgr_word wa;
    const mmgr_word wb;
    const uint8_t ca;
    const uint8_t cb;

    const size_t nlen;
    size_t *const rows;
    const size_t k;
    const mmgr_word fmask;
    size_t *const off;

    const char **const end;
    const char **const cur;
    mmgr_iword *const exp;

    const uint8_t *const mpint;
    const uint32_t mlen;
    uint8_t *const field;
    const size_t fieldlen;
} CellulCtx;


MMGR_INLINE mmgr_iword cellul_step_word_cs(const CellulCtx *c)
{
    const mmgr_word x = c->wa ^ c->wb;
    const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = c->wa);

    if ((x | z) == 0)
    {
        return MMGR_SWAR_GO;
    }

    size_t dl = MMGR_SWAR_BYTES;
    if (x != 0)
    {
        dl = MMGR_CALL(lane.first, ScrutLaneCfg,
                       .mask = MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = x));
    }
    size_t el = MMGR_SWAR_BYTES;
    if (z != 0)
    {
        el = MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z);
    }
    if (c->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

MMGR_INLINE mmgr_iword cellul_step_word_ci(const CellulCtx *c)
{
    const mmgr_word x = MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = c->wa, .val = c->wb, .ci = MMGR_TRUE);
    const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = c->wa);

    if ((x | z) == 0)
    {
        return MMGR_SWAR_GO;
    }

    size_t dl = MMGR_SWAR_BYTES;
    if (x != 0)
    {
        dl = MMGR_CALL(lane.first, ScrutLaneCfg,
                       .mask = MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = x));
    }
    size_t el = MMGR_SWAR_BYTES;
    if (z != 0)
    {
        el = MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z);
    }
    if (c->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

MMGR_INLINE mmgr_iword cellul_step_byte_cs(const CellulCtx *c)
{
    if (c->ca == 0)
    {
        if (c->ca == c->cb)
        {
            return MMGR_SWAR_YES;
        }
        return c->end_wins ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    if (c->ca != c->cb)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

MMGR_INLINE mmgr_iword cellul_step_byte_ci(const CellulCtx *c)
{
    const mmgr_word d =
        MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = (mmgr_word)c->ca, .val = (mmgr_word)c->cb, .ci = MMGR_TRUE);

    if (c->ca == 0)
    {
        if (d == 0)
        {
            return MMGR_SWAR_YES;
        }
        return c->end_wins ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    if (d != 0)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}


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
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word m =
            MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at)) &
            MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        if (m != 0)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return c->cap;
}

MMGR_INLINE const char *cellul_chr(const CellulCtx *c)
{
    if (c->byte == 0u)
    {
        return c->src + cellul_len(c);
    }

    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word w = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at);
        const mmgr_word keep = MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        const mmgr_word end = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w) & keep;
        const mmgr_word hit = MMGR_CALL(lane.eq, ScrutLaneCfg, .word = w, .byte = c->byte, .ci = MMGR_FALSE) &
                                    keep & MMGR_CALL(mask.before, ScrutMaskCfg, .mask = end);

        if (hit != 0)
        {
            return c->src + at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = hit);
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
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word d = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at) ^
                                  MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at);
        const mmgr_word m = (MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = d)) &
                                  MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        if (m != 0)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return c->cap;
}

MMGR_INLINE size_t cellul_diff_ci(const CellulCtx *c)
{
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word d = MMGR_CALL(lane.xor_, ScrutLaneCfg,
                                            .word = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at),
                                            .val = MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at), .ci = MMGR_TRUE);
        const mmgr_word m = (MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = d)) &
                                  MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        if (m != 0)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return c->cap;
}

MMGR_INLINE mmgr_bool cellul_agree_cs(const CellulCtx *c)
{
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word keep = MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word x =
            (MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa ^ wb)) & keep;

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(c->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return c->end_wins;
}

MMGR_INLINE mmgr_bool cellul_agree_ci(const CellulCtx *c)
{
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word keep = MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word fold = MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = wa, .val = wb, .ci = MMGR_TRUE);
        const mmgr_word x =
            (MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = fold)) & keep;

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(c->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return c->end_wins;
}

MMGR_INLINE uint8_t cellul_ancorae_fold(const CellulCtx *c)
{
    const uint8_t b = (uint8_t)c->other[c->k];

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

            const uint8_t cost = MMGR_CALL(ancorae.impensa, AncoraeCfg,
                                           .byte = cellul_ancorae_fold(&(CellulCtx){.other = c->other, .k = k, .ci = c->ci}));
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
    const char *const hay = c->src;
    const char *const needle = c->other;
    const size_t read_cap = c->cap;

    const size_t nlen = cellul_len(&(CellulCtx){.src = needle, .cap = c->other_cap});

    if (nlen == 0u)
    {
        return hay;
    }
    if (nlen > read_cap)
    {
        return NULL;
    }

    size_t rows[MMGR_SIEVE_ROWS];
    const size_t nrows = cellul_pick_rows(&(CellulCtx){.other = needle, .nlen = nlen, .rows = rows, .ci = ci});

    const size_t take = (nlen > MMGR_SWAR_BYTES) ? MMGR_SWAR_BYTES : nlen;
    const mmgr_word nmask = MMGR_CALL(mask.bytes_below, ScrutMaskCfg, .bytes = take);
    const mmgr_word nraw = MMGR_CALL(word.load, ScrutWordCfg, .at = needle) & nmask;
    const mmgr_word nword =
        ci ? (MMGR_CALL(word.fold_lower, ScrutWordCfg, .word = nraw) & nmask) : nraw;

    const size_t starts = read_cap - nlen + 1u;

    size_t maxrow = rows[0];

    for (size_t r = 1; r < nrows; ++r)
    {
        if (rows[r] > maxrow)
        {
            maxrow = rows[r];
        }
    }

    const size_t tail =
        (nlen > take) ? (MMGR_CALL(word.count, ScrutWordCfg, .bytes = nlen - take) * MMGR_SWAR_BYTES) : 0u;
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
        const mmgr_word end =
            MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + at));
        mmgr_word m;

        m = MMGR_CALL(lane.eq, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + at + rows[0]),
                      .byte = (uint8_t)needle[rows[0]], .ci = ci);

        for (size_t r = 1; r < nrows; ++r)
        {
            m &= MMGR_CALL(lane.eq, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + at + rows[r]),
                           .byte = (uint8_t)needle[rows[r]], .ci = ci);
        }

        if (end != 0)
        {
            m &= MMGR_CALL(mask.before, ScrutMaskCfg, .mask = end);
        }

        while (m != 0)
        {
            const size_t k = at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
            const mmgr_word cw = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + k);

            const mmgr_word syn =
                ((!ci || (MMGR_CALL(lane.any_upper, ScrutLaneCfg, .word = cw) == 0))
                     ? (cw ^ nword)
                     : MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = cw, .val = nword, .ci = MMGR_TRUE)) &
                nmask;

            if (syn == 0)
            {
                if (take == nlen)
                {
                    return hay + k;
                }

                const CellulCtx v = {.src = hay + k + take, .other = needle + take, .cap = nlen - take};
                const size_t d = ci ? cellul_diff_ci(&v) : cellul_diff_cs(&v);

                if (d == (nlen - take))
                {
                    return hay + k;
                }
            }
            m = MMGR_CALL(mask.drop_first, ScrutMaskCfg, .mask = m);
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
            const uint8_t h = (uint8_t)hay[k + i];
            const CellulCtx b = {.ca = (uint8_t)needle[i], .cb = h, .end_wins = MMGR_FALSE};

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

    const size_t n = cellul_len(&(CellulCtx){.src = c->src, .cap = c->cap - 1u});

    MMGR_CALL(proxim.read, ProximusCfg, .dst = c->dst, .at = c->src, .size = n);
    c->dst[n] = '\0';
    return n;
}

MMGR_INLINE mmgr_bool cellul_rd_str(const CellulCtx *c)
{
    const uint8_t *const buf = (const uint8_t *)c->src;
    size_t at = c->at;
    if ((at > c->cap) || ((c->cap - at) < 4u))
    {
        return MMGR_FALSE;
    }

    const uint32_t n = (uint32_t)MMGR_CALL(magna_extremitas.rd, EndianCfg, .src = buf + at, .width = MMGR_ENDIAN_32);
    at += 4u;

    if (n > (c->cap - at))
    {
        return MMGR_FALSE;
    }
    *c->out = buf + at;
    *c->slen = n;
    return MMGR_TRUE;
}


MMGR_INLINE mmgr_iword cellul_to_long(const CellulCtx *c)
{
    const char *p = c->src;

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
    mmgr_word v = 0;
    while (cellul_is_digit(*p))
    {
        v = (mmgr_word)(v * 10u) + (mmgr_word)(*p++ - '0');
    }

    if (c->end != NULL)
    {
        *c->end = (p != ds) ? p : c->src;
    }
    if (neg)
    {
        return (mmgr_iword)(0u - v);
    }
    return (mmgr_iword)v;
}

MMGR_INLINE mmgr_word cellul_to_ulong(const CellulCtx *c)
{
    const char *p = c->src;

    while (cellul_is_ws(*p))
    {
        p++;
    }
    if (*p == '+')
    {
        p++;
    }

    const char *const ds = p;
    mmgr_word v = 0;
    while (cellul_is_digit(*p))
    {
        v = (mmgr_word)(v * 10u) + (mmgr_word)(*p++ - '0');
    }

    if (c->end != NULL)
    {
        *c->end = (p != ds) ? p : c->src;
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

    mmgr_iword ex = 0;
    while (cellul_is_digit(**c->cur))
    {
        if (ex < MMGR_MUTO_EXP_LIMIT)
        {
            ex = (mmgr_iword)((ex * 10) + (**c->cur - '0'));
        }
        (*c->cur)++;
    }
    *c->exp = eneg ? -ex : ex;
}

MMGR_INLINE double cellul_to_double(const CellulCtx *c)
{
    const char *p = c->src;

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
    mmgr_iword drop = 0;
    mmgr_iword over = 0;
    mmgr_iword lost = 0;

    while (cellul_is_digit(*p))
    {
        if (!MMGR_CALL(muto.take, TransformoCfg, .mant = &mant, .digit = *p))
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
            if (MMGR_CALL(muto.take, TransformoCfg, .mant = &mant, .digit = *p))
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

    mmgr_iword ex = 0;
    if (any && ((*p == 'e') || (*p == 'E')))
    {
        cellul_expo(&(CellulCtx){.cur = &p, .exp = &ex});
    }

    const double val =
        MMGR_CALL(muto.scale, TransformoCfg, .mant = &mant, .ex = (mmgr_iword)(ex + over - drop), .rest = lost, .neg = neg);

    if (c->end != NULL)
    {
        *c->end = any ? p : c->src;
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

    while ((off < c->mlen) && (c->mpint[off] == 0))
    {
        off++;
    }

    const uint32_t vlen = c->mlen - off;
    if (vlen > c->fieldlen)
    {
        return MMGR_FALSE;
    }
    MMGR_CALL(memor.set, MemoriaCfg, .dst = c->field, .val = (uint8_t)0, .bytes = c->fieldlen);
    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = c->field + (c->fieldlen - vlen), .src = c->mpint + off, .bytes = (size_t)vlen);
    return MMGR_TRUE;
}


CatenaFinitaCfg (mmgr_cellul_init)(const CatenaFinitaCfg *c)
{
    return *c;
}

size_t (mmgr_cellul_len)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_len, CellulCtx, .src = c->src + c->at, .cap = c->cap - c->at);
}

size_t (mmgr_cellul_diff)(const CatenaFinitaCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_diff_ci, CellulCtx, .src = c->src, .other = c->other, .cap = c->cap);
    }
    return MMGR_CALL(cellul_diff_cs, CellulCtx, .src = c->src, .other = c->other, .cap = c->cap);
}

mmgr_bool (mmgr_cellul_eq)(const CatenaFinitaCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_agree_ci, CellulCtx, .src = c->src, .other = c->other, .cap = c->cap, .end_wins = MMGR_FALSE);
    }
    return MMGR_CALL(cellul_agree_cs, CellulCtx, .src = c->src, .other = c->other, .cap = c->cap, .end_wins = MMGR_FALSE);
}

mmgr_bool (mmgr_cellul_starts)(const CatenaFinitaCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_agree_ci, CellulCtx, .src = c->other, .other = c->src, .cap = c->cap, .end_wins = MMGR_TRUE);
    }
    return MMGR_CALL(cellul_agree_cs, CellulCtx, .src = c->other, .other = c->src, .cap = c->cap, .end_wins = MMGR_TRUE);
}

const char *(mmgr_cellul_find)(const CatenaFinitaCfg *c)
{
    const CellulCtx x = {.src = c->src, .cap = c->cap, .other = c->other, .other_cap = c->other_cap};

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
    return MMGR_CALL(cellul_chr, CellulCtx, .src = c->src, .cap = c->cap, .byte = c->byte);
}

size_t (mmgr_cellul_copy)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_copy, CellulCtx, .dst = c->dst, .src = c->src, .cap = c->cap);
}

mmgr_bool (mmgr_cellul_ws)(const CatenaFinitaCfg *c)
{
    return cellul_is_ws(c->src[c->at]);
}

mmgr_bool (mmgr_cellul_digit)(const CatenaFinitaCfg *c)
{
    return cellul_is_digit(c->src[c->at]);
}

mmgr_bool (mmgr_cellul_rd_str)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_rd_str, CellulCtx, .src = c->src, .cap = c->cap, .at = c->at, .out = c->out,
                     .slen = c->slen);
}

mmgr_iword (mmgr_cellul_step_word)(const VerboProgrediorCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_step_word_ci, CellulCtx, .wa = c->wa, .wb = c->wb, .end_wins = c->end_wins);
    }
    return MMGR_CALL(cellul_step_word_cs, CellulCtx, .wa = c->wa, .wb = c->wb, .end_wins = c->end_wins);
}

mmgr_iword (mmgr_cellul_step_byte)(const VerboProgrediorCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_step_byte_ci, CellulCtx, .ca = c->ca, .cb = c->cb, .end_wins = c->end_wins);
    }
    return MMGR_CALL(cellul_step_byte_cs, CellulCtx, .ca = c->ca, .cb = c->cb, .end_wins = c->end_wins);
}

mmgr_iword (mmgr_cellul_to_long)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_long, CellulCtx, .src = c->src, .end = c->end);
}

mmgr_word (mmgr_cellul_to_ulong)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_ulong, CellulCtx, .src = c->src, .end = c->end);
}

double (mmgr_cellul_to_double)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_double, CellulCtx, .src = c->src, .end = c->end);
}

float (mmgr_cellul_to_float)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_float, CellulCtx, .src = c->src, .end = c->end);
}

mmgr_bool (mmgr_cellul_mpint_fixed)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_mpint_fixed, CellulCtx, .mpint = c->mpint, .mlen = c->mlen, .field = c->field,
                     .fieldlen = c->fieldlen);
}
