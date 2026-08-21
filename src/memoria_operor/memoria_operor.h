// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_MEMORIA_OPEROR_H
#define MMGR_MEMORIA_OPEROR_H

#include "proximus_operor/proximus_operor.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file memoria_operor.h
 * @brief Bulk memory work, a word at a time.
 *
 * Every entry takes an explicit length. Nothing here scans for a terminator.
 *
 * The table is the whole surface. There are no free functions to call.
 */

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    void (*cpy)(void *dst, const void *src, size_t n);
    void (*move)(void *dst, const void *src, size_t n);
    int (*cmp)(const void *a, const void *b, size_t n);
    const void *(*chr)(const void *p, size_t n, uint8_t c);
    void (*set)(void *dst, unsigned char v, size_t n);
    void (*zero)(void *dst, size_t n);
} MemoriaOperorNs;
MMGR_NS_LAYOUT(MemoriaOperorNs, cpy, move, cmp, chr, set, zero);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
void mmgr_memor_cpy(void *dst, const void *src, size_t n);
void mmgr_memor_move(void *dst, const void *src, size_t n);
int mmgr_memor_cmp(const void *a, const void *b, size_t n);
const void *mmgr_memor_chr(const void *p, size_t n, uint8_t c);
void mmgr_memor_set(void *dst, unsigned char v, size_t n);
void mmgr_memor_zero(void *dst, size_t n);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS MemoriaOperorNs memor MMGR_UNUSED = {
    .cpy = mmgr_memor_cpy,
    .move = mmgr_memor_move,
    .cmp = mmgr_memor_cmp,
    .chr = mmgr_memor_chr,
    .set = mmgr_memor_set,
    .zero = mmgr_memor_zero,
};

MMGR_FINIS_DECLS

#endif
