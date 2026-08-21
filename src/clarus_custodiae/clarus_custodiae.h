// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CLARUS_CUSTODIAE_H
#define MMGR_CLARUS_CUSTODIAE_H

#include "spatium/spatium.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file clarus_custodiae.h
 * @brief The plaintext guardian. Hands out tenants and takes them back.
 *
 * One tenant over a static buffer. Nothing is freed
 * individually and nothing is ever reallocated: take what you need, release back to a mark.
 *
 * The table is the whole surface. There are no free functions to call.
 */


/** @brief Opaque pool state. */
struct PlainInternal;

/** @brief Dispatch table, with state behind the entries. The layout assert pins where the
 *         run ends. */
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

    struct PlainInternal *internal;
} ClarusCustodiaeNs;
MMGR_NS_LAYOUT_OPEN(ClarusCustodiaeNs, internal, alloc, span, persist, reset, mark, release, used, high_water, capacity, owns);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
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
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS ClarusCustodiaeNs clarus MMGR_UNUSED = {
    .alloc = mmgr_clarus_capio,
    .span = mmgr_clarus_span,
    .persist = mmgr_clarus_persist_span,
    .reset = mmgr_clarus_reset,
    .mark = mmgr_clarus_mark,
    .release = mmgr_clarus_reddo,
    .used = mmgr_clarus_used,
    .high_water = mmgr_clarus_high_water,
    .capacity = mmgr_clarus_capacity,
    .owns = mmgr_clarus_owns,
    .internal = &mmgr_clarus_internal,
};

MMGR_FINIS_DECLS

#endif
