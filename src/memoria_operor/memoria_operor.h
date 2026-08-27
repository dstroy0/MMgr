/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
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
    void (*cpy)(const MemoriaCfg *c);        /**< Copies upward; the regions must not overlap. */
    void (*move_down)(const MemoriaCfg *c);  /**< Copies upward, for a destination below the source. */
    void (*move_up)(const MemoriaCfg *c);    /**< Copies downward, for a destination above the source. */
    mmgr_iword (*cmp)(const MemoriaCfg *c);  /**< Orders two regions by their first difference. */
    const void *(*chr)(const MemoriaCfg *c); /**< Finds the first byte equal to val. */
    void (*set)(const MemoriaCfg *c);        /**< Fills a region with val. */
} MemoriaOperorNs;
MMGR_NS_LAYOUT(MemoriaOperorNs, cpy, move_down, move_up, cmp, chr, set);

/**
 * @brief Copies c->bytes from c->src to c->dst, walking upward.
 *
 * @param[in] c Destination, source and count [BORROWS].
 * @note Moves whole words first, then the remaining bytes one at a time.
 * @warning The backend's argument type qualifies both pointers restrict, so the regions must not overlap.
 */
void mmgr_memor_cpy(const MemoriaCfg *c);

/**
 * @brief Copies c->bytes from c->src to c->dst, walking downward from the far end.
 *
 * @param[in] c Destination, source and count [BORROWS].
 * @note Works back from the end, so a c->dst above c->src is safe even when the regions overlap.
 */
void mmgr_memor_move_up(const MemoriaCfg *c);

/**
 * @brief Compares c->bytes of c->src against c->other.
 *
 * @param[in] c The two regions and the count [BORROWS].
 * @return      The difference of the first unequal byte pair, or 0 when every byte matches.
 * @note The sign follows the differing bytes, so the result orders the two regions.
 * @warning Both regions must be readable for c->bytes.
 */
mmgr_iword mmgr_memor_cmp(const MemoriaCfg *c);

/**
 * @brief Finds the first byte in c->src equal to c->val, within c->bytes.
 *
 * @param[in] c Region, count and the byte sought [BORROWS].
 * @return      Address of the match, or NULL when the byte does not occur [BORROWS].
 * @note A terminator is not special; all c->bytes are searched.
 * @warning c->src must be readable for c->bytes.
 */
const void *mmgr_memor_chr(const MemoriaCfg *c);

/**
 * @brief Writes c->val into c->bytes of c->dst.
 *
 * @param[in] c Destination, count and the byte to write [BORROWS].
 * @note Stores whole words built from c->val, then finishes byte by byte.
 * @warning c->dst must be writable for c->bytes.
 */
void mmgr_memor_set(const MemoriaCfg *c);

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
