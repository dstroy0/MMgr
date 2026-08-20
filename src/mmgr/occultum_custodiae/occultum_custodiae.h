// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_OCCULTUM_CUSTODIAE_H
#define MMGR_OCCULTUM_CUSTODIAE_H

#include "mmgr/memoria_operor/memoria_operor.h"
#include "mmgr/spatium/spatium.h"

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#define MMGR_SEC_POOL_SLOTS (MMGR_GHOST_WORKER_SLOT + 1)

struct SecureInternal;

typedef struct
{
    void *(*alloc)(size_t n, size_t align);
    mmgr_spat (*span)(size_t n, size_t align);
    mmgr_spat (*persist_span)(size_t n);
    void (*reset)(void);
    size_t (*mark)(void);
    void (*release)(size_t mark);
    size_t (*used)(void);
    size_t (*high_water)(void);
    size_t (*capacity)(void);
    mmgr_bool (*owns)(const void *p);
    int (*slot_of)(const void *p);

    struct SecureInternal *internal;
} OccultumCustodiaeNs;

extern struct SecureInternal mmgr_occult_state;

static inline void mmgr_occult_wipe(void *ptr, size_t len)
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

void *mmgr_occult_capio(size_t n, size_t align);

mmgr_spat mmgr_occult_span(size_t n, size_t align);

mmgr_spat mmgr_occult_persist_span(size_t n);

size_t mmgr_occult_mark(void);

void mmgr_occult_reddo(size_t mark);

void mmgr_occult_reset(void);

size_t mmgr_occult_used(void);

size_t mmgr_occult_high_water(void);

size_t mmgr_occult_capacity(void);

mmgr_bool mmgr_occult_owns(const void *p);

int mmgr_occult_slot_of(const void *p);

static const OccultumCustodiaeNs occult __attribute__((unused)) = {.alloc = mmgr_occult_capio,
                                                                   .span = mmgr_occult_span,
                                                                   .persist_span = mmgr_occult_persist_span,
                                                                   .reset = mmgr_occult_reset,
                                                                   .mark = mmgr_occult_mark,
                                                                   .release = mmgr_occult_reddo,
                                                                   .used = mmgr_occult_used,
                                                                   .high_water = mmgr_occult_high_water,
                                                                   .capacity = mmgr_occult_capacity,
                                                                   .owns = mmgr_occult_owns,
                                                                   .slot_of = mmgr_occult_slot_of,
                                                                   .internal = &mmgr_occult_state};

MMGR_END_DECLS

#endif
