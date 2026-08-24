#include "numeros_scribo/numeros_scribo.h"
#include "cellularum_laboro/cellularum_laboro.h"

typedef struct
{
    char *out;
    size_t cap;
    size_t at;
    const mmgr_field *spec;
    const mmgr_fval *v;
    size_t nv;
    const mmgr_fval *one;
    uint8_t width;
} NumerCtx;

MMGR_INLINE const char *numer_str(const NumerCtx *c)
{
    if (c->one->as.s == NULL)
    {
        return "";
    }
    return c->one->as.s;
}

typedef enum MMGR_ENUM_PACKED
{
    NUMER_ARM_NONE = 0,
    NUMER_ARM_STR,
    NUMER_ARM_U32,
    NUMER_ARM_U64,
    NUMER_ARM_I64,
    NUMER_ARM_D,
    NUMER_ARM_CH,
} NumerArm;

typedef struct
{
    size_t (*fn)(const VerbaCfg *c);
    NumerArm arm;
    uint8_t base;
    uint8_t width;
} NumerKind;

static size_t numer_refuse(const VerbaCfg *c)
{
    return c->cap;
}

static const NumerKind s_kind[MMGR_FK_XML + 1u] = {
    [MMGR_FK_END] = {numer_refuse, NUMER_ARM_NONE, 0u, 0u},
    [MMGR_FK_LIT] = {numer_refuse, NUMER_ARM_NONE, 0u, 0u},
    [MMGR_FK_STR] = {mmgr_verba_put, NUMER_ARM_STR, 0u, 0u},
    [MMGR_FK_U32] = {mmgr_verba_u32, NUMER_ARM_U32, 10u, 1u},
    [MMGR_FK_U64] = {mmgr_verba_u64, NUMER_ARM_U64, 10u, 1u},
    [MMGR_FK_I64] = {mmgr_verba_i64, NUMER_ARM_I64, 10u, 1u},
    [MMGR_FK_DEC] = {mmgr_verba_u32w, NUMER_ARM_U32, 10u, 0u},
    [MMGR_FK_HEX] = {mmgr_verba_hex, NUMER_ARM_U64, 16u, 1u},
    [MMGR_FK_OCT] = {mmgr_verba_uint, NUMER_ARM_U64, 8u, 1u},
    [MMGR_FK_G] = {mmgr_verba_g, NUMER_ARM_D, 0u, 6u},
    [MMGR_FK_FIX] = {mmgr_verba_fixed, NUMER_ARM_D, 0u, 0u},
    [MMGR_FK_CH] = {mmgr_verba_ch, NUMER_ARM_CH, 0u, 0u},
    [MMGR_FK_JSON] = {mmgr_verba_json, NUMER_ARM_STR, 0u, 0u},
    [MMGR_FK_XML] = {mmgr_verba_xml, NUMER_ARM_STR, 0u, 0u},
};

MMGR_INLINE size_t numer_emit_one(const NumerCtx *c)
{
    MMGR_ASSERT(c->one->kind <= MMGR_FK_XML, "no such field kind");

    const NumerKind k = s_kind[c->one->kind];
    const uint8_t width = (c->width != 0u) ? c->width : k.width;
    const VerbaCfg cfg = {.p = c->out,
                          .cap = c->cap,
                          .at = c->at,
                          .s = (k.arm == NUMER_ARM_STR) ? numer_str(c) : NULL,
                          .c = (k.arm == NUMER_ARM_CH) ? c->one->as.c : 0,
                          .v = (k.arm == NUMER_ARM_U32) ? (uint64_t)c->one->as.u32 : c->one->as.u64,
                          .sv = c->one->as.i64,
                          .d = c->one->as.d,
                          .base = k.base,
                          .min = width,
                          .sig = width,
                          .decimals = width};

    return k.fn(&cfg);
}

MMGR_INLINE size_t numer_abandon(const NumerCtx *c)
{
    c->out[0] = '\0';
    return 0;
}

MMGR_INLINE size_t numer_finish(const NumerCtx *c)
{
    const size_t n = MMGR_CALL(verba.finish, VerbaCfg, .p = c->out, .cap = c->cap, .at = c->at);

    if (n == 0)
    {
        c->out[0] = '\0';
    }
    return n;
}

MMGR_INLINE size_t numer_build(const NumerCtx *c)
{
    size_t at = 0;
    size_t k = 0;

    if (c->cap == 0u)
    {
        return 0;
    }

    for (const mmgr_field *cursor = c->spec; cursor->kind != MMGR_FK_END; cursor++)
    {
        if (cursor->kind == MMGR_FK_LIT)
        {
            at = MMGR_CALL(verba.put_n, VerbaCfg, .p = c->out, .cap = c->cap, .at = at, .s = cursor->lit,
                           .sl = cursor->len);
            continue;
        }

        if ((k >= c->nv) || (c->v[k].kind != cursor->kind))
        {
            return numer_abandon(c);
        }

        at = MMGR_CALL(numer_emit_one, NumerCtx, .out = c->out, .cap = c->cap, .at = at, .one = &c->v[k],
                       .width = cursor->width);
        k++;
    }
    if (k != c->nv)
    {
        return numer_abandon(c);
    }
    return MMGR_CALL(numer_finish, NumerCtx, .out = c->out, .cap = c->cap, .at = at);
}

MMGR_INLINE size_t numer_emit(const NumerCtx *c)
{
    size_t at = 0;

    if (c->cap == 0u)
    {
        return 0;
    }

    for (size_t k = 0; k < c->nv; k++)
    {
        at = MMGR_CALL(numer_emit_one, NumerCtx, .out = c->out, .cap = c->cap, .at = at, .one = &c->v[k],
                       .width = c->v[k].width);
    }
    return MMGR_CALL(numer_finish, NumerCtx, .out = c->out, .cap = c->cap, .at = at);
}

MMGR_INLINE size_t numer_used(const NumerCtx *c)
{
    return MMGR_CALL(cellul.len, CatenaFinitaCfg, .s = c->out, .cap = c->cap);
}

size_t mmgr_numer_build(const NumerosCfg *c)
{
    return MMGR_CALL(numer_build, NumerCtx, .out = c->out, .cap = c->cap, .spec = c->spec, .v = c->v, .nv = c->nv);
}

size_t mmgr_numer_emit(const NumerosCfg *c)
{
    return MMGR_CALL(numer_emit, NumerCtx, .out = c->out, .cap = c->cap, .v = c->v, .nv = c->nv);
}

size_t mmgr_numer_append(const NumerosCfg *c)
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

    const size_t n = MMGR_CALL(numer.build, NumerosCfg, .out = c->out + used, .cap = c->cap - used, .spec = c->spec,
                               .v = c->v, .nv = c->nv);
    if (n == 0)
    {
        c->out[used] = '\0';
        return 0;
    }
    return used + n;
}

size_t mmgr_numer_emit_append(const NumerosCfg *c)
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

    const size_t n =
        MMGR_CALL(numer.emit, NumerosCfg, .out = c->out + used, .cap = c->cap - used, .v = c->v, .nv = c->nv);
    if (n == 0)
    {
        c->out[used] = '\0';
        return 0;
    }
    return used + n;
}
