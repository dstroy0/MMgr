// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CLARUS_CUSTODIAE_H
#define MMGR_CLARUS_CUSTODIAE_H

#include "mmgr/spatium/spatium.h"

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

#define MMGR_REG_POOL_SLOTS (MMGR_GHOST_WORKER_SLOT + 1)

struct PlainInternal;

typedef struct
{
    void *(*alloc)(size_t n, size_t align);
    mmgr_spat (*span)(size_t n, size_t align);
    mmgr_spat (*persist)(size_t n);
    void (*reset)(void);
    size_t (*mark)(void);
    void (*release)(size_t mark);
    size_t (*used)(void);
    size_t (*high_water)(void);
    size_t (*capacity)(void);
    mmgr_bool (*owns)(const void *p);
    int (*slot_of)(const void *p);

    struct PlainInternal *internal;
} ClarusCustodiaeNs;

extern struct PlainInternal mmgr_clarus_internal;

void *mmgr_clarus_capio(size_t n, size_t align);

mmgr_spat mmgr_clarus_span(size_t n, size_t align);

mmgr_spat mmgr_clarus_persist_span(size_t n);

void mmgr_clarus_reset(void);

size_t mmgr_clarus_mark(void);

void mmgr_clarus_reddo(size_t mark);

size_t mmgr_clarus_used(void);

size_t mmgr_clarus_high_water(void);

size_t mmgr_clarus_capacity(void);

mmgr_bool mmgr_clarus_owns(const void *p);

int mmgr_clarus_slot_of(const void *p);

static const ClarusCustodiaeNs clarus __attribute__((unused)) = {.alloc = mmgr_clarus_capio,
                                                                 .span = mmgr_clarus_span,
                                                                 .persist = mmgr_clarus_persist_span,
                                                                 .reset = mmgr_clarus_reset,
                                                                 .mark = mmgr_clarus_mark,
                                                                 .release = mmgr_clarus_reddo,
                                                                 .used = mmgr_clarus_used,
                                                                 .high_water = mmgr_clarus_high_water,
                                                                 .capacity = mmgr_clarus_capacity,
                                                                 .owns = mmgr_clarus_owns,
                                                                 .slot_of = mmgr_clarus_slot_of,
                                                                 .internal = &mmgr_clarus_internal};

MMGR_END_DECLS

#endif
