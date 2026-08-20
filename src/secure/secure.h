// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef PROTOCORE_SECURE_H
#define PROTOCORE_SECURE_H

#include "mmgr/protomem/protomem.h"
#include "mmgr/span/span.h"

#include "protocore_config.h"

PROTOCORE_BEGIN_DECLS

#define PROTOCORE_SEC_POOL_SLOTS (PROTOCORE_GHOST_WORKER_SLOT + 1)

struct SecureInternal;

typedef struct
{
    void *(*alloc)(size_t n, size_t align);
    protocore_span (*span)(size_t n, size_t align);
    protocore_span (*persist_span)(size_t n);
    void (*reset)(void);
    size_t (*mark)(void);
    void (*release)(size_t mark);
    size_t (*used)(void);
    size_t (*high_water)(void);
    size_t (*capacity)(void);
    proto_bool (*owns)(const void *p);
    int (*slot_of)(const void *p);

    struct SecureInternal *internal;
} SecureNs;

extern struct SecureInternal protocore_secure_state;

static inline void protocore_secure_wipe(void *ptr, size_t len)
{

    volatile uint8_t *b = (volatile uint8_t *)ptr;
    while (len != 0 && (((uintptr_t)b & (sizeof(uintptr_t) - 1)) != 0))
    {
        *b++ = 0;
        len--;
    }
    volatile uintptr_t *w = (volatile uintptr_t *)b;
    while (len >= sizeof(uintptr_t))
    {
        *w++ = 0;
        len -= sizeof(uintptr_t);
    }
    b = (volatile uint8_t *)w;
    while (len != 0)
    {
        *b++ = 0;
        len--;
    }
}

void *protocore_secure_alloc(size_t n, size_t align);

protocore_span protocore_secure_span(size_t n, size_t align);

protocore_span protocore_secure_persist_span(size_t n);

size_t protocore_secure_mark(void);

void protocore_secure_release(size_t mark);

void protocore_secure_reset(void);

size_t protocore_secure_used(void);

size_t protocore_secure_high_water(void);

size_t protocore_secure_capacity(void);

proto_bool protocore_secure_owns(const void *p);

int protocore_secure_slot_of(const void *p);

static const SecureNs secure __attribute__((unused)) = {.alloc = protocore_secure_alloc,
                                                        .span = protocore_secure_span,
                                                        .persist_span = protocore_secure_persist_span,
                                                        .reset = protocore_secure_reset,
                                                        .mark = protocore_secure_mark,
                                                        .release = protocore_secure_release,
                                                        .used = protocore_secure_used,
                                                        .high_water = protocore_secure_high_water,
                                                        .capacity = protocore_secure_capacity,
                                                        .owns = protocore_secure_owns,
                                                        .slot_of = protocore_secure_slot_of,
                                                        .internal = &protocore_secure_state};

PROTOCORE_END_DECLS

#endif
