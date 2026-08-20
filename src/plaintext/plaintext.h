// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_PLAINTEXT_H
#define MMGR_PLAINTEXT_H

#include "mmgr/span/span.h"

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#define MMGR_REG_POOL_SLOTS (MMGR_GHOST_WORKER_SLOT + 1)

struct PlainInternal;

typedef struct
{
    void *(*alloc)(size_t n, size_t align);
    mmgr_span (*span)(size_t n, size_t align);
    mmgr_span (*persist)(size_t n);
    void (*reset)(void);
    size_t (*mark)(void);
    void (*release)(size_t mark);
    size_t (*used)(void);
    size_t (*high_water)(void);
    size_t (*capacity)(void);
    mmgr_bool (*owns)(const void *p);
    int (*slot_of)(const void *p);

    struct PlainInternal *internal;
} PlainNs;

extern struct PlainInternal mmgr_plaintext_internal;

void *mmgr_plaintext_alloc(size_t n, size_t align);

mmgr_span mmgr_plaintext_span(size_t n, size_t align);

mmgr_span mmgr_plaintext_persist_span(size_t n);

void mmgr_plaintext_reset(void);

size_t mmgr_plaintext_mark(void);

void mmgr_plaintext_release(size_t mark);

size_t mmgr_plaintext_used(void);

size_t mmgr_plaintext_high_water(void);

size_t mmgr_plaintext_capacity(void);

mmgr_bool mmgr_plaintext_owns(const void *p);

int mmgr_plaintext_slot_of(const void *p);

static const PlainNs plain __attribute__((unused)) = {.alloc = mmgr_plaintext_alloc,
                                                      .span = mmgr_plaintext_span,
                                                      .persist = mmgr_plaintext_persist_span,
                                                      .reset = mmgr_plaintext_reset,
                                                      .mark = mmgr_plaintext_mark,
                                                      .release = mmgr_plaintext_release,
                                                      .used = mmgr_plaintext_used,
                                                      .high_water = mmgr_plaintext_high_water,
                                                      .capacity = mmgr_plaintext_capacity,
                                                      .owns = mmgr_plaintext_owns,
                                                      .slot_of = mmgr_plaintext_slot_of,
                                                      .internal = &mmgr_plaintext_internal};

MMGR_END_DECLS

#endif
