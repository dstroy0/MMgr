// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_MEMORIA_OPEROR_H
#define MMGR_MEMORIA_OPEROR_H

#include "proximus_operor/proximus_operor.h"

#include "config/mmgr_config.h"

MMGR_BEGIN_DECLS

/**
 * @file memoria_operor.h
 * @brief Bulk memory work, a word at a time.
 *
 * Every entry takes an explicit length. Nothing here scans for a terminator.
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

/**
 * @brief Copy @p n bytes. Regions must not overlap.
 * @param dst Destination.
 * @param src Source.
 * @param n Byte count.
 */
void mmgr_memor_cpy(void *dst, const void *src, size_t n);

/**
 * @brief Copy @p n bytes. Regions may overlap.
 * @param dst Destination.
 * @param src Source.
 * @param n Byte count.
 */
void mmgr_memor_move(void *dst, const void *src, size_t n);

/**
 * @brief Compare @p n bytes.
 * @param a First region.
 * @param b Second region.
 * @param n Byte count.
 * @return Difference of the first bytes that differ, or 0.
 */
int mmgr_memor_cmp(const void *a, const void *b, size_t n);

/**
 * @brief Find @p c in the first @p n bytes.
 * @param p Region.
 * @param n Byte count.
 * @param c Byte to find.
 * @return Pointer to it, or NULL.
 */
const void *mmgr_memor_chr(const void *p, size_t n, uint8_t c);

/**
 * @brief Fill @p n bytes with @p v.
 * @param dst Destination.
 * @param v Byte to write.
 * @param n Byte count.
 */
void mmgr_memor_set(void *dst, unsigned char v, size_t n);

/**
 * @brief Zero @p n bytes.
 * @param dst Destination.
 * @param n Byte count.
 */
void mmgr_memor_zero(void *dst, size_t n);

#if defined(MMGR_ORACLE_LIBC) && MMGR_ORACLE_LIBC
#include "config/mmgr_oracle_libc.h"

/**
 * @brief Module namespace, pointed at libc.
 *
 * Test only. Every entry here has a direct libc equivalent, so the whole suite runs against libc
 * unchanged and any disagreement is a finding. See mmgr_oracle_libc.h.
 */
MMGR_NS MemoriaOperorNs memor MMGR_UNUSED = {
    .cpy = mmgr_oracle_cpy,
    .move = mmgr_oracle_move,
    .cmp = memcmp,
    .chr = mmgr_oracle_chr,
    .set = mmgr_oracle_set,
    .zero = mmgr_oracle_zero,
};
#else
/** @brief Module namespace. const is what lets the compiler devirtualize a call through it. */
MMGR_NS MemoriaOperorNs memor MMGR_UNUSED = {
    .cpy = mmgr_memor_cpy,
    .move = mmgr_memor_move,
    .cmp = mmgr_memor_cmp,
    .chr = mmgr_memor_chr,
    .set = mmgr_memor_set,
    .zero = mmgr_memor_zero,
};
#endif

MMGR_END_DECLS

#endif
