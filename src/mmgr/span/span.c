// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/span/span.h"

mmgr_span mmgr_span_from(uint8_t *p, size_t cap)
{
    mmgr_span s;
    s.buf = (p != NULL && cap != 0) ? p : NULL;
    s.cap = (s.buf != NULL) ? cap : 0;
    s.pos = 0;
    s.overflow = MMGR_FALSE;
    return s;
}

mmgr_cspan mmgr_cspan_from(const uint8_t *p, size_t len)
{
    mmgr_cspan s;
    s.buf = (p != NULL && len != 0) ? p : NULL;
    s.len = (s.buf != NULL) ? len : 0;
    s.pos = 0;
    s.err = MMGR_FALSE;
    return s;
}

mmgr_bool mmgr_span_ok(mmgr_span s)
{
    return s.buf != NULL && !s.overflow;
}

mmgr_bool mmgr_span_has_storage(mmgr_span s)
{
    return s.buf != NULL;
}

mmgr_bool mmgr_cspan_ok(mmgr_cspan s)
{
    return s.buf != NULL && !s.err;
}

size_t mmgr_span_len(mmgr_span s)
{
    return s.pos;
}

size_t mmgr_span_room(mmgr_span s)
{
    return (s.pos < s.cap) ? (s.cap - s.pos) : 0;
}

void mmgr_span_reset(mmgr_span *s)
{
    s->pos = 0;
    s->overflow = MMGR_FALSE;
}

mmgr_span mmgr_span_after(mmgr_span s, size_t off)
{
    if (!mmgr_span_has_storage(s) || off >= s.cap)
    {
        return mmgr_span_from(NULL, 0);
    }
    return mmgr_span_from(s.buf + off, s.cap - off);
}

mmgr_span mmgr_span_first(mmgr_span s, size_t n)
{
    if (!mmgr_span_has_storage(s))
    {
        return mmgr_span_from(NULL, 0);
    }
    return mmgr_span_from(s.buf, (n < s.cap) ? n : s.cap);
}

mmgr_cspan mmgr_span_produced(mmgr_span s)
{
    if (!mmgr_span_ok(s))
    {
        return mmgr_cspan_from(NULL, 0);
    }
    return mmgr_cspan_from(s.buf, s.pos);
}

mmgr_cspan mmgr_span_read(mmgr_span s, size_t len)
{
    if (!mmgr_span_has_storage(s))
    {
        return mmgr_cspan_from(NULL, 0);
    }
    return mmgr_cspan_from(s.buf, (len < s.cap) ? len : s.cap);
}
