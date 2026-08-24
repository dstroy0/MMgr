#include "endian/endian.h"
#include "proximus_operor/proximus_operor.h"

typedef struct
{
    uint8_t *dst;
    const uint8_t *src;
    uint64_t val;
    mmgr_endian_width width;
} EndianCtx;

MMGR_INLINE void endian_put(const EndianCtx *c)
{
    switch (c->width)
    {
    case 2:
        MMGR_CALL(proxim.put16, ProximusCfg, .dst = c->dst, .val = c->val);
        break;
    case 4:
        MMGR_CALL(proxim.put32, ProximusCfg, .dst = c->dst, .val = c->val);
        break;
    default:
        MMGR_CALL(proxim.put64, ProximusCfg, .dst = c->dst, .val = c->val);
        break;
    }
}

MMGR_INLINE uint64_t endian_get(const EndianCtx *c)
{
    switch (c->width)
    {
    case 2:
        return MMGR_CALL(proxim.load16, ProximusCfg, .at = c->src);
    case 4:
        return MMGR_CALL(proxim.load32, ProximusCfg, .at = c->src);
    default:
        return MMGR_CALL(proxim.load64, ProximusCfg, .at = c->src);
    }
}

MMGR_INLINE uint64_t endian_rev(const EndianCtx *c)
{
    uint64_t v = c->val;

    v = ((v & 0x00FF00FF00FF00FFull) << 8) | ((v >> 8) & 0x00FF00FF00FF00FFull);
    v = ((v & 0x0000FFFF0000FFFFull) << 16) | ((v >> 16) & 0x0000FFFF0000FFFFull);
    v = (v << 32) | (v >> 32);
    return v >> (8u * (8u - c->width));
}

MMGR_INLINE size_t endian_wr_le(const EndianCtx *c)
{
    endian_put(c);
    return c->width;
}

MMGR_INLINE size_t endian_wr_be(const EndianCtx *c)
{
    MMGR_CALL(endian_put, EndianCtx, .dst = c->dst, .val = endian_rev(c), .width = c->width);
    return c->width;
}

MMGR_INLINE uint64_t endian_rd_le(const EndianCtx *c)
{
    return endian_get(c);
}

MMGR_INLINE uint64_t endian_rd_be(const EndianCtx *c)
{
    return MMGR_CALL(endian_rev, EndianCtx, .val = endian_get(c), .width = c->width);
}

size_t mmgr_wr_le(const EndianCfg *c)
{
    return MMGR_CALL(endian_wr_le, EndianCtx, .dst = c->dst, .val = c->val, .width = (size_t)c->width);
}

uint64_t mmgr_rd_le(const EndianCfg *c)
{
    return MMGR_CALL(endian_rd_le, EndianCtx, .src = c->src, .width = (size_t)c->width);
}

size_t mmgr_wr_be(const EndianCfg *c)
{
    return MMGR_CALL(endian_wr_be, EndianCtx, .dst = c->dst, .val = c->val, .width = (size_t)c->width);
}

uint64_t mmgr_rd_be(const EndianCfg *c)
{
    return MMGR_CALL(endian_rd_be, EndianCtx, .src = c->src, .width = (size_t)c->width);
}

uint64_t mmgr_endian_rev(const EndianCfg *c)
{
    return MMGR_CALL(endian_rev, EndianCtx, .val = c->val, .width = (size_t)c->width);
}
