#include "endian/endian.h"
#include "proximus_operor/proximus_operor.h"

typedef struct
{
    uint8_t *w;
    const uint8_t *r;
    uint64_t v;
    size_t n;
} EndianCtx;

MMGR_INLINE void endian_put(const EndianCtx *c)
{
    switch (c->n)
    {
    case 2:
        MMGR_CALL(proxim.put16, ProximusCfg, .dst = c->w, .val = c->v);
        break;
    case 4:
        MMGR_CALL(proxim.put32, ProximusCfg, .dst = c->w, .val = c->v);
        break;
    default:
        MMGR_CALL(proxim.put64, ProximusCfg, .dst = c->w, .val = c->v);
        break;
    }
}

MMGR_INLINE uint64_t endian_get(const EndianCtx *c)
{
    switch (c->n)
    {
    case 2:
        return MMGR_CALL(proxim.load16, ProximusCfg, .at = c->r);
    case 4:
        return MMGR_CALL(proxim.load32, ProximusCfg, .at = c->r);
    default:
        return MMGR_CALL(proxim.load64, ProximusCfg, .at = c->r);
    }
}

MMGR_INLINE uint64_t endian_rev(const EndianCtx *c)
{
    uint64_t v = c->v;

    v = ((v & 0x00FF00FF00FF00FFull) << 8) | ((v >> 8) & 0x00FF00FF00FF00FFull);
    v = ((v & 0x0000FFFF0000FFFFull) << 16) | ((v >> 16) & 0x0000FFFF0000FFFFull);
    v = (v << 32) | (v >> 32);
    return v >> (8u * (8u - c->n));
}

MMGR_INLINE size_t endian_wr_le(const EndianCtx *c)
{
    endian_put(c);
    return c->n;
}

MMGR_INLINE size_t endian_wr_be(const EndianCtx *c)
{
    MMGR_CALL(endian_put, EndianCtx, .w = c->w, .v = endian_rev(c), .n = c->n);
    return c->n;
}

MMGR_INLINE uint64_t endian_rd_le(const EndianCtx *c)
{
    return endian_get(c);
}

MMGR_INLINE uint64_t endian_rd_be(const EndianCtx *c)
{
    return MMGR_CALL(endian_rev, EndianCtx, .v = endian_get(c), .n = c->n);
}

size_t mmgr_wr_le(const EndianCfg *c)
{
    return MMGR_CALL(endian_wr_le, EndianCtx, .w = c->w, .v = c->v, .n = (size_t)c->n);
}

uint64_t mmgr_rd_le(const EndianCfg *c)
{
    return MMGR_CALL(endian_rd_le, EndianCtx, .r = c->r, .n = (size_t)c->n);
}

size_t mmgr_wr_be(const EndianCfg *c)
{
    return MMGR_CALL(endian_wr_be, EndianCtx, .w = c->w, .v = c->v, .n = (size_t)c->n);
}

uint64_t mmgr_rd_be(const EndianCfg *c)
{
    return MMGR_CALL(endian_rd_be, EndianCtx, .r = c->r, .n = (size_t)c->n);
}

uint64_t mmgr_endian_rev(const EndianCfg *c)
{
    return MMGR_CALL(endian_rev, EndianCtx, .v = c->v, .n = (size_t)c->n);
}
