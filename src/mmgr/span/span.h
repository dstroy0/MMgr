// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_SPAN_H
#define MMGR_SPAN_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

typedef struct
{
    uint8_t *buf;
    size_t cap;
    size_t pos;
    mmgr_bool overflow;
} mmgr_span;

typedef struct
{
    const uint8_t *buf;
    size_t len;
    size_t pos;
    mmgr_bool err;
} mmgr_cspan;

typedef struct
{
    mmgr_span (*from)(uint8_t *p, size_t cap);
    mmgr_bool (*ok)(mmgr_span s);
    mmgr_bool (*has_storage)(mmgr_span s);
    size_t (*len)(mmgr_span s);
    size_t (*room)(mmgr_span s);
    void (*reset)(mmgr_span *s);
    mmgr_span (*after)(mmgr_span s, size_t off);
    mmgr_span (*first)(mmgr_span s, size_t n);
    mmgr_cspan (*produced)(mmgr_span s);
    mmgr_cspan (*read)(mmgr_span s, size_t len);
    mmgr_cspan (*cfrom)(const uint8_t *p, size_t len);
    mmgr_bool (*cok)(mmgr_cspan s);
} SpanNs;

mmgr_span mmgr_span_from(uint8_t *p, size_t cap);
mmgr_bool mmgr_span_ok(mmgr_span s);
mmgr_bool mmgr_span_has_storage(mmgr_span s);
size_t mmgr_span_len(mmgr_span s);
size_t mmgr_span_room(mmgr_span s);
void mmgr_span_reset(mmgr_span *s);
mmgr_span mmgr_span_after(mmgr_span s, size_t off);
mmgr_span mmgr_span_first(mmgr_span s, size_t n);
mmgr_cspan mmgr_span_produced(mmgr_span s);
mmgr_cspan mmgr_span_read(mmgr_span s, size_t len);
mmgr_cspan mmgr_cspan_from(const uint8_t *p, size_t len);
mmgr_bool mmgr_cspan_ok(mmgr_cspan s);

static const SpanNs span __attribute__((unused)) = {.from = mmgr_span_from,
                                                    .ok = mmgr_span_ok,
                                                    .has_storage = mmgr_span_has_storage,
                                                    .len = mmgr_span_len,
                                                    .room = mmgr_span_room,
                                                    .reset = mmgr_span_reset,
                                                    .after = mmgr_span_after,
                                                    .first = mmgr_span_first,
                                                    .produced = mmgr_span_produced,
                                                    .read = mmgr_span_read,
                                                    .cfrom = mmgr_cspan_from,
                                                    .cok = mmgr_cspan_ok};

MMGR_END_DECLS

#endif
