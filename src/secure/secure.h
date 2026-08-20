// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_SECURE_H
#define MMGR_SECURE_H

#include "mmgr/protomem/protomem.h"
#include "mmgr/span/span.h"

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#define MMGR_SEC_POOL_SLOTS (MMGR_GHOST_WORKER_SLOT + 1)

struct SecureInternal;

typedef struct
{
    void *(*alloc)(size_t n, size_t align);
    mmgr_span (*span)(size_t n, size_t align);
    mmgr_span (*persist_span)(size_t n);
    void (*reset)(void);
    size_t (*mark)(void);
    void (*release)(size_t mark);
    size_t (*used)(void);
    size_t (*high_water)(void);
    size_t (*capacity)(void);
    mmgr_bool (*owns)(const void *p);
    int (*slot_of)(const void *p);

    struct SecureInternal *internal;
} SecureNs;

extern struct SecureInternal mmgr_secure_state;

static inline void mmgr_secure_wipe(void *ptr, size_t len)
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

void *mmgr_secure_alloc(size_t n, size_t align);

mmgr_span mmgr_secure_span(size_t n, size_t align);

mmgr_span mmgr_secure_persist_span(size_t n);

size_t mmgr_secure_mark(void);

void mmgr_secure_release(size_t mark);

void mmgr_secure_reset(void);

size_t mmgr_secure_used(void);

size_t mmgr_secure_high_water(void);

size_t mmgr_secure_capacity(void);

mmgr_bool mmgr_secure_owns(const void *p);

int mmgr_secure_slot_of(const void *p);

static const SecureNs secure __attribute__((unused)) = {.alloc = mmgr_secure_alloc,
                                                        .span = mmgr_secure_span,
                                                        .persist_span = mmgr_secure_persist_span,
                                                        .reset = mmgr_secure_reset,
                                                        .mark = mmgr_secure_mark,
                                                        .release = mmgr_secure_release,
                                                        .used = mmgr_secure_used,
                                                        .high_water = mmgr_secure_high_water,
                                                        .capacity = mmgr_secure_capacity,
                                                        .owns = mmgr_secure_owns,
                                                        .slot_of = mmgr_secure_slot_of,
                                                        .internal = &mmgr_secure_state};

MMGR_END_DECLS

#endif
