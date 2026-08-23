#include "numeros_scribo/numeros_scribo.h"
#include "cellularum_laboro/cellularum_laboro.h"


MMGR_OPTIMIZE_O2

typedef struct
{
    char *out;                    size_t cap;                   const mmgr_field *spec;       const mmgr_fval *v;           size_t nv;                    size_t k;                     mmgr_verba b;                 const mmgr_fval *one;         uint8_t width;                const mmgr_field *cursor; } NumerCtx;

MMGR_INLINE const char *numer_str(const NumerCtx *c)
{
    if (c->one->as.s == NULL)
    {
        return "";
    }
    return c->one->as.s;
}

MMGR_INLINE mmgr_bool numer_emit_one(NumerCtx *c)
{
    const mmgr_fval *a = c->one;

    switch (a->kind)
    {
    case MMGR_FK_STR:
        verba.put(&c->b, numer_str(c));
        return MMGR_TRUE;
    case MMGR_FK_U32:
        verba.u32(&c->b, a->as.u32);
        return MMGR_TRUE;
    case MMGR_FK_U64:
        verba.u64(&c->b, a->as.u64);
        return MMGR_TRUE;
    case MMGR_FK_I64:
        verba.i64(&c->b, a->as.i64);
        return MMGR_TRUE;
    case MMGR_FK_DEC:
        verba.u32w(&c->b, a->as.u32, c->width);
        return MMGR_TRUE;
    case MMGR_FK_HEX:
        verba.hex(&c->b, a->as.u64, c->width ? c->width : 1u);
        return MMGR_TRUE;
    case MMGR_FK_OCT:
        verba.uint(&c->b, a->as.u64, 8, c->width ? c->width : 1u);
        return MMGR_TRUE;
    case MMGR_FK_G:
        verba.g(&c->b, a->as.d, c->width ? c->width : 6u);
        return MMGR_TRUE;
    case MMGR_FK_FIX:
        verba.fixed(&c->b, a->as.d, c->width);
        return MMGR_TRUE;
    case MMGR_FK_CH:
        verba.ch(&c->b, a->as.c);
        return MMGR_TRUE;
    case MMGR_FK_JSON:
        verba.json(&c->b, numer_str(c));
        return MMGR_TRUE;
    case MMGR_FK_XML:
        verba.xml(&c->b, numer_str(c));
        return MMGR_TRUE;
    default:
        return MMGR_FALSE;
    }
}

MMGR_INLINE size_t numer_abandon(NumerCtx *c)
{
    c->out[0] = '\0';
    return 0;
}

MMGR_INLINE size_t numer_finish(NumerCtx *c)
{
    const size_t n = verba.finish(&c->b);

    if (n == 0)
    {
        c->out[0] = '\0';
    }
    return n;
}

MMGR_INLINE size_t numer_build(NumerCtx *c)
{
    if (c->cap == 0u)
    {
        return 0;
    }

    c->b.p = c->out;
    c->b.cap = c->cap;
    c->b.len = 0;
    c->b.ok = MMGR_TRUE;

    for (c->cursor = c->spec; c->cursor->kind != MMGR_FK_END; c->cursor++)
    {
        if (c->cursor->kind == MMGR_FK_LIT)
        {
            verba.put_n(&c->b, c->cursor->lit, c->cursor->len);
            continue;
        }

        if ((c->k >= c->nv) || (c->v[c->k].kind != c->cursor->kind))
        {
            return numer_abandon(c);
        }
        c->one = &c->v[c->k];
        c->width = c->cursor->width;
        c->k++;

        if (!numer_emit_one(c))
        {
            return numer_abandon(c);
        }
    }
    if (c->k != c->nv)
    {
        return numer_abandon(c);
    }
    return numer_finish(c);
}

MMGR_INLINE size_t numer_emit(NumerCtx *c)
{
    if (c->cap == 0u)
    {
        return 0;
    }

    c->b.p = c->out;
    c->b.cap = c->cap;
    c->b.len = 0;
    c->b.ok = MMGR_TRUE;

    for (c->k = 0; c->k < c->nv; c->k++)
    {
        c->one = &c->v[c->k];
        c->width = c->v[c->k].width;
        if (!numer_emit_one(c))
        {
            return numer_abandon(c);
        }
    }
    return numer_finish(c);
}

MMGR_INLINE size_t numer_used(const NumerCtx *c)
{
    return mmgr_cellul_len(c->out, c->cap);
}


size_t (mmgr_numer_build)(const NumerosCfg *c)
{
    return MMGR_CALL(numer_build, NumerCtx, .out = c->out, .cap = c->cap, .spec = c->spec, .v = c->v, .nv = c->nv);
}

size_t (mmgr_numer_emit)(const NumerosCfg *c)
{
    return MMGR_CALL(numer_emit, NumerCtx, .out = c->out, .cap = c->cap, .v = c->v, .nv = c->nv);
}

size_t (mmgr_numer_append)(const NumerosCfg *c)
{
    if (c->cap == 0u)
    {
        return 0;
    }

    const size_t used = MMGR_CALL(numer_used, NumerCtx, .out = c->out, .cap = c->cap);
    if (used >= c->cap)
    {
        return 0;
    }

    const size_t n = mmgr_numer_build(c->out + used, c->cap - used, c->spec, c->v, c->nv);
    if (n == 0)
    {
        c->out[used] = '\0';
        return 0;
    }
    return used + n;
}

size_t (mmgr_numer_emit_append)(const NumerosCfg *c)
{
    if (c->cap == 0u)
    {
        return 0;
    }

    const size_t used = MMGR_CALL(numer_used, NumerCtx, .out = c->out, .cap = c->cap);
    if (used >= c->cap)
    {
        return 0;
    }

    const size_t n = mmgr_numer_emit(c->out + used, c->cap - used, c->v, c->nv);
    if (n == 0)
    {
        c->out[used] = '\0';
        return 0;
    }
    return used + n;
}
