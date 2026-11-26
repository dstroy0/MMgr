/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file memoria_operor.h
 * @brief Byte-level memory work: its arguments, the calls, and the memor dispatch table.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 */
#ifndef MMGR_MEMORIA_OPEROR_H
#define MMGR_MEMORIA_OPEROR_H

/** @addtogroup mod_memor
 *  @{ */

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief Arguments for the memor calls.
 *
 * @note cpy and the two moves read dst, src and bytes. cmp reads src, other and bytes.
 * @note chr reads src, bytes and val. set reads dst, bytes and val.
 * @note sort reads dst, scratch, count, bytes and order. bytes is then the size of one element.
 */
typedef struct
{
    void *const dst;         /**< Destination for cpy, the moves and set, and the elements of sort [BORROWS]. */
    const void *const src;   /**< Source for cpy and the moves, region for cmp and chr [BORROWS]. */
    const void *const other; /**< Second region for cmp [BORROWS]. */
    const size_t bytes;      /**< Bytes the call moves, examines or writes, or the bytes of one element of sort. */
    const uint8_t val;       /**< Byte chr looks for, or the byte set writes. */
    void *const scratch;     /**< Room for every element of sort, which its merges write [BORROWS]. */
    const size_t count;      /**< Elements sort orders. */
    int (*const order)(const void *left, const void *right); /**< 0 or less where left comes at or before right. */
} MemoriaCfg;

/**
 * @brief Type of the memor dispatch table.
 *
 * @note EMBED_TABLE_LAYOUT asserts the seven members sit at consecutive EMBED_FUNCTION_POINTER_BYTES
 *       offsets, with nothing else.
 * @note cpy, move_down and move_up all copy, and differ in the overlap each one allows.
 */
typedef struct
{
    void (*cpy)(const MemoriaCfg *args);        /**< Copies upward, for regions that do not overlap. */
    void (*move_down)(const MemoriaCfg *args);  /**< Copies upward, for a destination below the source. */
    void (*move_up)(const MemoriaCfg *args);    /**< Copies downward, for a destination above the source. */
    embed_iword (*cmp)(const MemoriaCfg *args); /**< Orders two regions by their first difference. */
    const void *(*chr)(const MemoriaCfg *args); /**< Finds the first byte equal to val. */
    void (*set)(const MemoriaCfg *args);        /**< Fills a region with val. */
    embed_bool (*sort)(const MemoriaCfg *args); /**< Puts elements in the order of a compare. */
} MemoriaOperorNs;
EMBED_TABLE_LAYOUT(MemoriaOperorNs, cpy, move_down, move_up, cmp, chr, set, sort);

/**
 * @brief Copies args->bytes from args->src to args->dst, on an upward walk.
 *
 * @param[in] args Destination, source and count [BORROWS].
 * @note Moves whole words first, then the rest of the bytes one at a time.
 * @warning The backend's argument type qualifies both pointers restrict, and so the regions must not overlap.
 * @warning args->dst must be writable and args->src readable for args->bytes.
 */
EMBED_HOST_DEVICE void mmgr_memor_cpy(const MemoriaCfg *args);

/**
 * @brief Copies args->bytes from args->src to args->dst, on a downward walk from the far end.
 *
 * @param[in] args Destination, source and count [BORROWS].
 * @note Works back from the end. An args->dst above args->src is safe even when the regions overlap.
 * @warning args->dst must be writable and args->src readable for args->bytes.
 */
EMBED_HOST_DEVICE void mmgr_memor_move_up(const MemoriaCfg *args);

/**
 * @brief Compares args->bytes of args->src against args->other.
 *
 * @param[in] args The two regions and the count [BORROWS].
 * @return         The difference of the first unequal byte pair, or 0 when every byte matches.
 * @note The sign follows the bytes that differ, and so the result orders the two regions.
 * @warning Both regions must be readable for args->bytes rounded up to a whole word. A count that
 *          does not fill the last one still reads a whole word at a time.
 */
EMBED_HOST_DEVICE embed_iword mmgr_memor_cmp(const MemoriaCfg *args);

/**
 * @brief Finds the first byte in args->src equal to args->val, within args->bytes.
 *
 * @param[in] args Region, count and the byte sought [BORROWS].
 * @return         Address of the match, or NULL when the byte does not occur [BORROWS].
 * @note A terminator is not special. The search covers all args->bytes.
 * @warning args->src must be readable for args->bytes rounded up to a whole word. A count that
 *          does not fill the last one still reads a whole word at a time.
 */
EMBED_HOST_DEVICE const void *mmgr_memor_chr(const MemoriaCfg *args);

/**
 * @brief Writes args->val into args->bytes of args->dst.
 *
 * @param[in] args Destination, count and the byte to write [BORROWS].
 * @note Stores whole words built from args->val, then finishes byte by byte.
 * @warning args->dst must be writable for args->bytes.
 */
EMBED_HOST_DEVICE void mmgr_memor_set(const MemoriaCfg *args);

/**
 * @brief Sorts args->count elements of args->bytes bytes each at args->dst in the order of args->order.
 *
 * @param[in] args The elements, the room, the count, the size of one element and the order [BORROWS].
 * @return         EMBED_TRUE where the elements are in order. EMBED_FALSE where args->bytes is 0 or a pointer is NULL,
 *                 which leaves the elements as they were. Fewer than two elements are in order already.
 * @note args->order answers 0 or less where its left element comes at or before its right one, as the compare of
 *       qsort answers. Two elements that order as equal keep the order that they had.
 * @note A bottom-up merge sort: each pass merges runs of twice the width of the pass before, from args->dst into
 *       args->scratch and back. Every element moves log2 of args->count times, and MMgr takes no memory of its own.
 * @note theory/linea/Linea/Sort.lean proves that the sort keeps the elements and leaves them in order, where the
 *       order is transitive and total.
 * @warning args->dst and args->scratch must each hold args->count times args->bytes bytes, and must not overlap.
 */
EMBED_HOST_DEVICE embed_bool mmgr_memor_sort(const MemoriaCfg *args);

/**
 * @brief Dispatch table instance named memor.
 *
 * @note cpy and move_down both point at mmgr_memor_cpy. Every other member has its own function.
 * @note There is no mmgr_memor_move_down. A destination below the source is what the upward walk
 *       already handles. So the table names that walk twice and carries no second copy of it.
 */
EMBED_TABLE_STORAGE MemoriaOperorNs memor EMBED_UNUSED = {
    .cpy = mmgr_memor_cpy,
    .move_down = mmgr_memor_cpy,
    .move_up = mmgr_memor_move_up,
    .cmp = mmgr_memor_cmp,
    .chr = mmgr_memor_chr,
    .set = mmgr_memor_set,
    .sort = mmgr_memor_sort,
};

EMBED_END_DECLS

/** @} */

#endif
