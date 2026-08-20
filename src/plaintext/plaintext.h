// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_PLAINTEXT_H
#define PROTOCORE_PLAINTEXT_H

#include "mmgr/span/span.h"

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

#define PROTOCORE_REG_POOL_SLOTS (PROTOCORE_GHOST_WORKER_SLOT + 1)

struct PlainInternal;

typedef struct
{
    void *(*alloc)(size_t n, size_t align);
    protocore_span (*span)(size_t n, size_t align);
    protocore_span (*persist)(size_t n);
    void (*reset)(void);
    size_t (*mark)(void);
    void (*release)(size_t mark);
    size_t (*used)(void);
    size_t (*high_water)(void);
    size_t (*capacity)(void);
    proto_bool (*owns)(const void *p);
    int (*slot_of)(const void *p);

    struct PlainInternal *internal;
} PlainNs;

extern struct PlainInternal protocore_plaintext_internal;

void *protocore_plaintext_alloc(size_t n, size_t align);

protocore_span protocore_plaintext_span(size_t n, size_t align);

protocore_span protocore_plaintext_persist_span(size_t n);

void protocore_plaintext_reset(void);

size_t protocore_plaintext_mark(void);

void protocore_plaintext_release(size_t mark);

size_t protocore_plaintext_used(void);

size_t protocore_plaintext_high_water(void);

size_t protocore_plaintext_capacity(void);

proto_bool protocore_plaintext_owns(const void *p);

int protocore_plaintext_slot_of(const void *p);

static const PlainNs plain __attribute__((unused)) = {.alloc = protocore_plaintext_alloc,
                                                      .span = protocore_plaintext_span,
                                                      .persist = protocore_plaintext_persist_span,
                                                      .reset = protocore_plaintext_reset,
                                                      .mark = protocore_plaintext_mark,
                                                      .release = protocore_plaintext_release,
                                                      .used = protocore_plaintext_used,
                                                      .high_water = protocore_plaintext_high_water,
                                                      .capacity = protocore_plaintext_capacity,
                                                      .owns = protocore_plaintext_owns,
                                                      .slot_of = protocore_plaintext_slot_of,
                                                      .internal = &protocore_plaintext_internal};

PROTOCORE_END_DECLS

#endif
