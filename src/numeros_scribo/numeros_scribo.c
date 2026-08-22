// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "numeros_scribo/numeros_scribo.h"
#include "cellularum_laboro/cellularum_laboro.h"

/**
 * @file numeros_scribo.c
 * @brief Render a field spec and a value list into text.
 *
 * Every entry below takes one parameter, a pointer to the NumerosCfg the caller built. A buffer, a
 * spec, a value list and the builder writing into it are one render, and NumerCtx is that.
 */

MMGR_OPTIMIZE_O2

/** @brief The render, in one place. */
typedef struct
{
    char *out;                /**< Destination. */
    size_t cap;               /**< Its size. */
    const mmgr_field *spec;   /**< The fields to lay down, or NULL for the spec free form. */
    const mmgr_fval *v;       /**< The values. */
    size_t nv;                /**< How many. */
    size_t k;                 /**< Which value is next. */
    mmgr_verba b;             /**< The builder writing into out. */
    const mmgr_fval *one;     /**< The value being rendered. */
    uint8_t width;            /**< The width it is rendered at. */
    const mmgr_field *cursor; /**< The field being rendered. */
} NumerCtx;

/**
 * @brief The value's string, or the empty one if it is NULL.
 * @param c The render.
 * @return Never NULL.
 */
MMGR_INLINE const char *numer_str(const NumerCtx *c)
{
    if (c->one->as.s == NULL)
    {
        return "";
    }
    return c->one->as.s;
}

/**
 * @brief Render the one value in @c one at width @c width.
 * @param c In/out. The render.
 * @return MMGR_FALSE if the kind is not one this renders.
 *
 * One body, so the spec driven entry and the spec free one cannot drift apart.
 */
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

/**
 * @brief Empty the destination and report nothing written.
 * @param c In/out. The render.
 * @return Zero, always.
 */
MMGR_INLINE size_t numer_abandon(NumerCtx *c)
{
    c->out[0] = '\0';
    return 0;
}

/**
 * @brief Close the builder, emptying the destination if nothing came out.
 * @param c In/out. The render.
 * @return Bytes written.
 */
MMGR_INLINE size_t numer_finish(NumerCtx *c)
{
    const size_t n = verba.finish(&c->b);

    if (n == 0)
    {
        c->out[0] = '\0';
    }
    return n;
}

/**
 * @brief Render the spec against the values.
 * @param c In/out. The render.
 * @return Bytes written, or zero if the two did not line up.
 */
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

/**
 * @brief Render the values with no spec, each at its own width.
 * @param c In/out. The render.
 * @return Bytes written, or zero if a kind was not one this renders.
 */
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

/**
 * @brief Where an append would start, or the capacity if there is no room.
 * @param c The render.
 * @return Offset of the terminator.
 */
MMGR_INLINE size_t numer_used(const NumerCtx *c)
{
    return mmgr_cellul_len(c->out, c->cap);
}

/* The namespace is a table of function pointers with the caller's config in their types, so these
   are what it points at. They build the context and hand it to the bodies above.

   Parenthesised because the header defines a macro of each name for the call site, and the
   identifier here would otherwise sit immediately before a parenthesis and be taken for an
   invocation of it.

   They are nameable rather than file local because a static const table in the header has to be
   able to point at them, and a static const table is what gcc devirtualizes. Through an extern one
   every call from another translation unit is a load of the table, a load of the entry, and an
   indirect call it cannot see through. */

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
