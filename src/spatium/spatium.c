// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "spatium/spatium.h"

mmgr_spat mmgr_spat_from(uint8_t *p, size_t cap)
{
    mmgr_spat s;
    s.buf = (p != NULL && cap != 0) ? p : NULL;
    s.cap = (s.buf != NULL) ? cap : 0;
    s.pos = 0;
    s.overflow = MMGR_FALSE;
    return s;
}

mmgr_fspat mmgr_fspat_from(const uint8_t *p, size_t len)
{
    mmgr_fspat s;
    s.buf = (p != NULL && len != 0) ? p : NULL;
    s.len = (s.buf != NULL) ? len : 0;
    s.pos = 0;
    s.err = MMGR_FALSE;
    return s;
}

mmgr_bool mmgr_spat_ok(mmgr_spat s)
{
    return s.buf != NULL && !s.overflow;
}

mmgr_bool mmgr_spat_has_storage(mmgr_spat s)
{
    return s.buf != NULL;
}

mmgr_bool mmgr_fspat_ok(mmgr_fspat s)
{
    return s.buf != NULL && !s.err;
}

size_t mmgr_spat_len(mmgr_spat s)
{
    return s.pos;
}

size_t mmgr_spat_room(mmgr_spat s)
{
    return (s.pos < s.cap) ? (s.cap - s.pos) : 0;
}

void mmgr_spat_reset(mmgr_spat *s)
{
    s->pos = 0;
    s->overflow = MMGR_FALSE;
}

mmgr_spat mmgr_spat_after(mmgr_spat s, size_t off)
{
    if (!mmgr_spat_has_storage(s) || off >= s.cap)
    {
        return mmgr_spat_from(NULL, 0);
    }
    return mmgr_spat_from(s.buf + off, s.cap - off);
}

mmgr_spat mmgr_spat_first(mmgr_spat s, size_t n)
{
    if (!mmgr_spat_has_storage(s))
    {
        return mmgr_spat_from(NULL, 0);
    }
    return mmgr_spat_from(s.buf, (n < s.cap) ? n : s.cap);
}

mmgr_fspat mmgr_spat_produced(mmgr_spat s)
{
    if (!mmgr_spat_ok(s))
    {
        return mmgr_fspat_from(NULL, 0);
    }
    return mmgr_fspat_from(s.buf, s.pos);
}

mmgr_fspat mmgr_spat_read(mmgr_spat s, size_t len)
{
    if (!mmgr_spat_has_storage(s))
    {
        return mmgr_fspat_from(NULL, 0);
    }
    return mmgr_fspat_from(s.buf, (len < s.cap) ? len : s.cap);
}
