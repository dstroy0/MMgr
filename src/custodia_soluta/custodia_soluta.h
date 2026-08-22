// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CUSTODIA_SOLUTA_H
#define MMGR_CUSTODIA_SOLUTA_H

#include "spatium/spatium.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file custodia_soluta.h
 * @brief The plaintext guardian. Hands out tenants and takes them back.
 *
 * One tenant over a static buffer. Nothing is freed
 * individually and nothing is ever reallocated: take what you need, release back to a mark.
 *
 * The table is the whole surface. There are no free functions to call.
 */


/** @brief Opaque pool state. */
struct SolutaInternal;

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

    struct SolutaInternal *internal;
} CustodiaSolutaNs;
MMGR_NS_LAYOUT_OPEN(CustodiaSolutaNs, internal, alloc, span, persist, reset, mark, release, used, high_water, capacity, owns);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
extern struct SolutaInternal mmgr_soluta_internal;
void *mmgr_soluta_capio(size_t n, size_t align);
mmgr_spat mmgr_soluta_span(size_t n, size_t align);
mmgr_spat mmgr_soluta_persist_span(size_t n);
void mmgr_soluta_reset(void);
size_t mmgr_soluta_mark(void);
void mmgr_soluta_reddo(size_t mark);
size_t mmgr_soluta_used(void);
size_t mmgr_soluta_high_water(void);
size_t mmgr_soluta_capacity(void);
mmgr_bool mmgr_soluta_owns(const void *p);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS CustodiaSolutaNs soluta MMGR_UNUSED = {
    .alloc = mmgr_soluta_capio,
    .span = mmgr_soluta_span,
    .persist = mmgr_soluta_persist_span,
    .reset = mmgr_soluta_reset,
    .mark = mmgr_soluta_mark,
    .release = mmgr_soluta_reddo,
    .used = mmgr_soluta_used,
    .high_water = mmgr_soluta_high_water,
    .capacity = mmgr_soluta_capacity,
    .owns = mmgr_soluta_owns,
    .internal = &mmgr_soluta_internal,
};

MMGR_FINIS_DECLS

#endif
