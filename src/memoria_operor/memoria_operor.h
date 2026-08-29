/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file memoria_operor.h
 * @brief Byte-level memory work: its arguments, the calls, and the memor dispatch table.
 */
#ifndef MMGR_MEMORIA_OPEROR_H
#define MMGR_MEMORIA_OPEROR_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Arguments for the memor calls.
 *
 * @note cpy and the two moves read dst, src and bytes; cmp reads src, other and bytes.
 * @note chr reads src, bytes and val; set reads dst, bytes and val.
 */
typedef struct
{
    void *const dst;         /**< Destination for cpy, the moves and set [BORROWS]. */
    const void *const src;   /**< Source for cpy and the moves, region for cmp and chr [BORROWS]. */
    const void *const other; /**< Second region for cmp [BORROWS]. */
    const size_t bytes;      /**< Bytes the call moves, examines or writes. */
    const uint8_t val;       /**< Byte chr looks for, or the byte set writes. */
} MemoriaCfg;

/**
 * @brief Type of the memor dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the six members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note cpy, move_down and move_up all copy; they differ in the overlap each one allows.
 */
typedef struct
{
    void (*cpy)(const MemoriaCfg *args);        /**< Copies upward; the regions must not overlap. */
    void (*move_down)(const MemoriaCfg *args);  /**< Copies upward, for a destination below the source. */
    void (*move_up)(const MemoriaCfg *args);    /**< Copies downward, for a destination above the source. */
    mmgr_iword (*cmp)(const MemoriaCfg *args);  /**< Orders two regions by their first difference. */
    const void *(*chr)(const MemoriaCfg *args); /**< Finds the first byte equal to val. */
    void (*set)(const MemoriaCfg *args);        /**< Fills a region with val. */
} MemoriaOperorNs;
MMGR_NS_LAYOUT(MemoriaOperorNs, cpy, move_down, move_up, cmp, chr, set);

/**
 * @brief Copies args->bytes from args->src to args->dst, walking upward.
 *
 * @param[in] args Destination, source and count [BORROWS].
 * @note Moves whole words first, then the remaining bytes one at a time.
 * @warning The backend's argument type qualifies both pointers restrict, so the regions must not overlap.
 * @warning args->dst must be writable and args->src readable for args->bytes.
 */
void mmgr_memor_cpy(const MemoriaCfg *args);

/**
 * @brief Copies args->bytes from args->src to args->dst, walking downward from the far end.
 *
 * @param[in] args Destination, source and count [BORROWS].
 * @note Works back from the end, so an args->dst above args->src is safe even when the regions overlap.
 * @warning args->dst must be writable and args->src readable for args->bytes.
 */
void mmgr_memor_move_up(const MemoriaCfg *args);

/**
 * @brief Compares args->bytes of args->src against args->other.
 *
 * @param[in] args The two regions and the count [BORROWS].
 * @return      The difference of the first unequal byte pair, or 0 when every byte matches.
 * @note The sign follows the differing bytes, so the result orders the two regions.
 * @warning Both regions must be readable for args->bytes rounded up to a whole word, since a count that
 *          does not fill the last one is still read a whole word at a time.
 */
mmgr_iword mmgr_memor_cmp(const MemoriaCfg *args);

/**
 * @brief Finds the first byte in args->src equal to args->val, within args->bytes.
 *
 * @param[in] args Region, count and the byte sought [BORROWS].
 * @return      Address of the match, or NULL when the byte does not occur [BORROWS].
 * @note A terminator is not special; all args->bytes are searched.
 * @warning args->src must be readable for args->bytes rounded up to a whole word, since a count that
 *          does not fill the last one is still read a whole word at a time.
 */
const void *mmgr_memor_chr(const MemoriaCfg *args);

/**
 * @brief Writes args->val into args->bytes of args->dst.
 *
 * @param[in] args Destination, count and the byte to write [BORROWS].
 * @note Stores whole words built from args->val, then finishes byte by byte.
 * @warning args->dst must be writable for args->bytes.
 */
void mmgr_memor_set(const MemoriaCfg *args);

/**
 * @brief Dispatch table instance named memor.
 *
 * @note cpy and move_down both point at mmgr_memor_cpy; every other member has its own function.
 */
MMGR_NS MemoriaOperorNs memor MMGR_UNUSED = {
    .cpy = mmgr_memor_cpy,
    .move_down = mmgr_memor_cpy,
    .move_up = mmgr_memor_move_up,
    .cmp = mmgr_memor_cmp,
    .chr = mmgr_memor_chr,
    .set = mmgr_memor_set,
};

MMGR_FINIS_DECLS

#endif
