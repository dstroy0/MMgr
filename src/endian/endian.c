#include "endian/endian.h"
#include "proximus_operor/proximus_operor.h"


typedef struct
{
    uint8_t *w;           const uint8_t *r;     uint64_t v;           size_t n;         } EndianCtx;

MMGR_INLINE void endian_put(uint8_t *w, uint64_t v, size_t n)
{
    switch (n)
    {
    case 2:
        proxim.put_u16(w, (uint16_t)v);
        break;
    case 4:
        proxim.put_u32(w, (uint32_t)v);
        break;
    default:
        proxim.put_u64(w, v);
        break;
    }
}

MMGR_INLINE uint64_t endian_rev(uint64_t v, size_t n)
{
    v = ((v & 0x00FF00FF00FF00FFull) << 8) | ((v >> 8) & 0x00FF00FF00FF00FFull);
    v = ((v & 0x0000FFFF0000FFFFull) << 16) | ((v >> 16) & 0x0000FFFF0000FFFFull);
    v = (v << 32) | (v >> 32);
    return v >> (8u * (8u - n));
}

MMGR_INLINE size_t endian_wr_le(EndianCtx *c)
{
    endian_put(c->w, c->v, c->n);
    return c->n;
}

MMGR_INLINE size_t endian_wr_be(EndianCtx *c)
{
    endian_put(c->w, endian_rev(c->v, c->n), c->n);
    return c->n;
}

MMGR_INLINE uint64_t endian_rd_le(EndianCtx *c)
{
    return proxim.load(c->r, c->n);
}

MMGR_INLINE uint64_t endian_rd_be(EndianCtx *c)
{
    return endian_rev(proxim.load(c->r, c->n), c->n);
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
