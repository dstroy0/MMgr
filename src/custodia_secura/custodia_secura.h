// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CUSTODIA_SECURA_H
#define MMGR_CUSTODIA_SECURA_H

#include "memoria_operor/memoria_operor.h"
#include "spatium/spatium.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file occultum_custodiae.h
 * @brief The secure guardian. Same shape as custodia_soluta, but released bytes are wiped.
 *
 * The table is the whole surface. There are no free functions to call.
 */


/** @brief Opaque pool state. */
struct SecuraInternal;

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

    struct SecuraInternal *internal;
} CustodiaSecuraNs;
MMGR_NS_LAYOUT_OPEN(CustodiaSecuraNs, internal, alloc, span, persist_span, reset, mark, release, used, high_water,
                    capacity, owns);

/**
 * @brief Zero @p len bytes so the compiler cannot remove the writes.
 * @param ptr Region.
 * @param len Byte count.
 *
 * volatile is the whole point. A plain loop over memory about to be released is dead code and the
 * optimizer is entitled to delete it, which is how a secret survives its own erasure. Bytes to the
 * first word boundary, then words, then the tail.
 */
static inline void mmgr_secura_wipe(void *ptr, size_t len)
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

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
extern struct SecuraInternal mmgr_secura_state;
void *mmgr_secura_capio(size_t n, size_t align);
mmgr_spat mmgr_secura_span(size_t n, size_t align);
mmgr_spat mmgr_secura_persist_span(size_t n);
size_t mmgr_secura_mark(void);
void mmgr_secura_reddo(size_t mark);
void mmgr_secura_reset(void);
size_t mmgr_secura_used(void);
size_t mmgr_secura_high_water(void);
size_t mmgr_secura_capacity(void);
mmgr_bool mmgr_secura_owns(const void *p);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS CustodiaSecuraNs secura MMGR_UNUSED = {
    .alloc = mmgr_secura_capio,
    .span = mmgr_secura_span,
    .persist_span = mmgr_secura_persist_span,
    .reset = mmgr_secura_reset,
    .mark = mmgr_secura_mark,
    .release = mmgr_secura_reddo,
    .used = mmgr_secura_used,
    .high_water = mmgr_secura_high_water,
    .capacity = mmgr_secura_capacity,
    .owns = mmgr_secura_owns,
    .internal = &mmgr_secura_state,
};

MMGR_FINIS_DECLS

#endif
