// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_SPAN_H
#define PROTOCORE_SPAN_H

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

typedef struct
{
    uint8_t *buf;
    size_t cap;
    size_t pos;
    proto_bool overflow;
} protocore_span;

typedef struct
{
    const uint8_t *buf;
    size_t len;
    size_t pos;
    proto_bool err;
} protocore_cspan;

typedef struct
{
    protocore_span (*from)(uint8_t *p, size_t cap);
    proto_bool (*ok)(protocore_span s);
    proto_bool (*has_storage)(protocore_span s);
    size_t (*len)(protocore_span s);
    size_t (*room)(protocore_span s);
    void (*reset)(protocore_span *s);
    protocore_span (*after)(protocore_span s, size_t off);
    protocore_span (*first)(protocore_span s, size_t n);
    protocore_cspan (*produced)(protocore_span s);
    protocore_cspan (*read)(protocore_span s, size_t len);
    protocore_cspan (*cfrom)(const uint8_t *p, size_t len);
    proto_bool (*cok)(protocore_cspan s);
} SpanNs;

protocore_span protocore_span_from(uint8_t *p, size_t cap);
proto_bool protocore_span_ok(protocore_span s);
proto_bool protocore_span_has_storage(protocore_span s);
size_t protocore_span_len(protocore_span s);
size_t protocore_span_room(protocore_span s);
void protocore_span_reset(protocore_span *s);
protocore_span protocore_span_after(protocore_span s, size_t off);
protocore_span protocore_span_first(protocore_span s, size_t n);
protocore_cspan protocore_span_produced(protocore_span s);
protocore_cspan protocore_span_read(protocore_span s, size_t len);
protocore_cspan protocore_cspan_from(const uint8_t *p, size_t len);
proto_bool protocore_cspan_ok(protocore_cspan s);

static const SpanNs span __attribute__((unused)) = {.from = protocore_span_from,
                                                    .ok = protocore_span_ok,
                                                    .has_storage = protocore_span_has_storage,
                                                    .len = protocore_span_len,
                                                    .room = protocore_span_room,
                                                    .reset = protocore_span_reset,
                                                    .after = protocore_span_after,
                                                    .first = protocore_span_first,
                                                    .produced = protocore_span_produced,
                                                    .read = protocore_span_read,
                                                    .cfrom = protocore_cspan_from,
                                                    .cok = protocore_cspan_ok};

PROTOCORE_END_DECLS

#endif
