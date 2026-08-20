// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/byteio/byteio.h"

void mmgr_byteio_put(mmgr_spat *w, uint8_t b)
{
    if (w->pos < w->cap)
    {
        w->buf[w->pos] = b;
    }
    else
    {
        w->overflow = MMGR_TRUE;
    }
    w->pos++;
}

void mmgr_byteio_put_be(mmgr_spat *w, uint64_t val, int32_t nbytes)
{
    for (int32_t s = (nbytes - 1) * 8; s >= 0; s -= 8)
    {
        mmgr_byteio_put(w, (uint8_t)(val >> s));
    }
}

void mmgr_byteio_bytes(mmgr_spat *w, const void *src, size_t n)
{
    if (w->buf != NULL && w->pos <= w->cap && w->cap - w->pos >= n)
    {
        memor.cpy(w->buf + w->pos, src, n);
    }
    else if (n > 0)
    {
        w->overflow = MMGR_TRUE;
    }
    w->pos += n;
}

mmgr_bool mmgr_byteio_take_be(mmgr_fspat *r, size_t nbytes, uint64_t *out)
{
    if (r->pos > r->len || r->len - r->pos < nbytes)
    {
        r->err = MMGR_TRUE;
        return MMGR_FALSE;
    }
    uint64_t v = 0;
    for (size_t i = 0; i < nbytes; i++)
    {
        v = (v << 8) | r->buf[r->pos + i];
    }
    *out = v;
    r->pos += nbytes;
    return MMGR_TRUE;
}

mmgr_bool mmgr_rd_u32(const uint8_t *p, size_t len, size_t *off, uint32_t *out)
{
    if (*off > len || len - *off < 4)
    {
        return MMGR_FALSE;
    }
    *out = mmgr_rd32be(p + *off);
    *off += 4;
    return MMGR_TRUE;
}

mmgr_bool mmgr_rd_str(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen)
{
    size_t start = *off;
    uint32_t n = 0;
    if (!mmgr_rd_u32(p, len, off, &n))
    {
        return MMGR_FALSE;
    }
    if (n > len - *off)
    {
        *off = start;
        return MMGR_FALSE;
    }
    *out = p + *off;
    *slen = n;
    *off += n;
    return MMGR_TRUE;
}

mmgr_bool mmgr_mpint_to_fixed(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen)
{
    uint32_t off = 0;
    while (off < mlen && m[off] == 0)
    {
        off++;
    }
    uint32_t vlen = mlen - off;
    if (vlen > outlen)
    {
        return MMGR_FALSE;
    }
    memor.set(out, 0, outlen);
    memor.cpy(out + (outlen - vlen), m + off, vlen);
    return MMGR_TRUE;
}
