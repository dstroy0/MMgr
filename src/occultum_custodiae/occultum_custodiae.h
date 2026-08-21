// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_OCCULTUM_CUSTODIAE_H
#define MMGR_OCCULTUM_CUSTODIAE_H

#include "memoria_operor/memoria_operor.h"
#include "spatium/spatium.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file occultum_custodiae.h
 * @brief The secure guardian. Same shape as clarus, but released bytes are wiped.
 */


/** @brief Opaque pool state. */
struct SecureInternal;

/** @brief Dispatch table, with state behind the entries. The layout assert pins where the
 *         run ends. */
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

    struct SecureInternal *internal;
} OccultumCustodiaeNs;
MMGR_NS_LAYOUT_OPEN(OccultumCustodiaeNs, internal, alloc, span, persist_span, reset, mark, release, used, high_water,
                    capacity, owns);

/** @brief The pool. */
extern struct SecureInternal mmgr_occult_state;

/**
 * @brief Zero @p len bytes so the compiler cannot remove the writes.
 * @param ptr Region.
 * @param len Byte count.
 *
 * volatile is the whole point. A plain loop over memory about to be released is dead code and the
 * optimizer is entitled to delete it, which is how a secret survives its own erasure. Bytes to the
 * first word boundary, then words, then the tail.
 */
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

/**
 * @brief Take @p n bytes from the tenant.
 * @param n Byte count.
 * @param align Alignment, a power of two.
 * @return The bytes, or NULL if the tenant is full.
 */
void *mmgr_occult_capio(size_t n, size_t align);

/**
 * @brief Take @p n bytes as a writable span.
 * @param n Byte count.
 * @param align Alignment, a power of two.
 * @return The span. Empty with no storage if the tenant is full.
 */
mmgr_spat mmgr_occult_span(size_t n, size_t align);

/**
 * @brief Take @p n bytes that a release will not reclaim.
 * @param n Byte count.
 * @return The span. Empty with no storage if the tenant is full.
 */
mmgr_spat mmgr_occult_persist_span(size_t n);

/**
 * @brief Current fill point, to release back to later.
 * @return The mark.
 */
size_t mmgr_occult_mark(void);

/**
 * @brief Release everything taken since @p mark.
 * @param mark A mark from this tenant.
 *
 * Nothing is freed and nothing moves. The fill point moves back, so every pointer handed out after
 * @p mark is dead.
 */
void mmgr_occult_reddo(size_t mark);

/**
 * @brief Release everything the tenant holds.
 */
void mmgr_occult_reset(void);

/**
 * @brief How much of the tenant is taken.
 * @return Byte count.
 */
size_t mmgr_occult_used(void);

/**
 * @brief The most that has ever been taken.
 * @return Byte count. Never falls.
 */
size_t mmgr_occult_high_water(void);

/**
 * @brief Size of one tenant.
 * @return Byte count.
 */
size_t mmgr_occult_capacity(void);

/**
 * @brief Did this pool hand out @p p.
 * @param p Pointer.
 * @return MMGR_TRUE if it did.
 */
mmgr_bool mmgr_occult_owns(const void *p);


/** @brief Module namespace. */
MMGR_NS OccultumCustodiaeNs occult MMGR_UNUSED = {.alloc = mmgr_occult_capio,
                                                  .span = mmgr_occult_span,
                                                  .persist_span = mmgr_occult_persist_span,
                                                  .reset = mmgr_occult_reset,
                                                  .mark = mmgr_occult_mark,
                                                  .release = mmgr_occult_reddo,
                                                  .used = mmgr_occult_used,
                                                  .high_water = mmgr_occult_high_water,
                                                  .capacity = mmgr_occult_capacity,
                                                  .owns = mmgr_occult_owns,
                                                  .internal = &mmgr_occult_state};

MMGR_FINIS_DECLS

#endif
