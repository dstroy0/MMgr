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

/** @brief The pool. */
extern struct PlainInternal mmgr_clarus_internal;

/**
 * @brief Take @p n bytes from the tenant.
 * @param n Byte count.
 * @param align Alignment, a power of two.
 * @return The bytes, or NULL if the tenant is full.
 */
void *mmgr_clarus_capio(size_t n, size_t align);

/**
 * @brief Take @p n bytes as a writable span.
 * @param n Byte count.
 * @param align Alignment, a power of two.
 * @return The span. Empty with no storage if the tenant is full.
 */
mmgr_spat mmgr_clarus_span(size_t n, size_t align);

/**
 * @brief Take @p n bytes that a release will not reclaim.
 * @param n Byte count.
 * @return The span. Empty with no storage if the tenant is full.
 */
mmgr_spat mmgr_clarus_persist_span(size_t n);

/**
 * @brief Release everything the tenant holds.
 */
void mmgr_clarus_reset(void);

/**
 * @brief Current fill point, to release back to later.
 * @return The mark.
 */
size_t mmgr_clarus_mark(void);

/**
 * @brief Release everything taken since @p mark.
 * @param mark A mark from this tenant.
 *
 * Nothing is freed and nothing moves. The fill point moves back, so every pointer handed out after
 * @p mark is dead.
 */
void mmgr_clarus_reddo(size_t mark);

/**
 * @brief How much of the tenant is taken.
 * @return Byte count.
 */
size_t mmgr_clarus_used(void);

/**
 * @brief The most that has ever been taken.
 * @return Byte count. Never falls.
 */
size_t mmgr_clarus_high_water(void);

/**
 * @brief Size of one tenant.
 * @return Byte count.
 */
size_t mmgr_clarus_capacity(void);

/**
 * @brief Did this pool hand out @p p.
 * @param p Pointer.
 * @return MMGR_TRUE if it did.
 */
mmgr_bool mmgr_clarus_owns(const void *p);


/** @brief Module namespace. */
MMGR_NS ClarusCustodiaeNs clarus MMGR_UNUSED = {.alloc = mmgr_clarus_capio,
                                                .span = mmgr_clarus_span,
                                                .persist = mmgr_clarus_persist_span,
                                                .reset = mmgr_clarus_reset,
                                                .mark = mmgr_clarus_mark,
                                                .release = mmgr_clarus_reddo,
                                                .used = mmgr_clarus_used,
                                                .high_water = mmgr_clarus_high_water,
                                                .capacity = mmgr_clarus_capacity,
                                                .owns = mmgr_clarus_owns,
                                                .internal = &mmgr_clarus_internal};

MMGR_FINIS_DECLS

#endif
