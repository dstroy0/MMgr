/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file memoria_linea.h
 * @brief The calls of memor with plain arguments.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note The calls are a copy, a move, a fill, a compare, a find and a sort.
 * @note Every entry takes its arguments by value, and therefore a C++ translation unit calls them as they are. Each
 *       entry fills the members of MemoriaCfg that its call reads. It leaves every other member zero, as a compound
 *       literal that names those members does. This header includes mmgr.h alone.
 * @note theory/linea proves each entry against the contract of its call. No call reads a member outside its own
 *       set, and the move is a copy at every overlap. The compare answers 0 exactly where every byte agrees. The find
 *       answers the first address that holds the byte.
 */
#ifndef MMGR_MEMORIA_LINEA_H
#define MMGR_MEMORIA_LINEA_H

/** @addtogroup mod_memor
 *  @{ */

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief Copies bytes from src to dst, on an upward walk.
 *
 * @param[out] dst   Destination to write [BORROWS].
 * @param[in]  src   Bytes to read [BORROWS].
 * @param[in]  bytes Bytes to copy.
 * @note memor.cpy with dst, src and bytes.
 * @warning The two regions must not overlap, unless dst lies at or below src. mmgr_bytes_move takes any overlap.
 * @warning dst must be writable and src readable for bytes.
 */
void mmgr_bytes_copy(void *dst, const void *src, size_t bytes);

/**
 * @brief Copies bytes from src to dst at any overlap.
 *
 * @param[out] dst   Destination to write [BORROWS].
 * @param[in]  src   Bytes to read [BORROWS].
 * @param[in]  bytes Bytes to copy.
 * @note memor.move_down where dst lies at or below src, and memor.move_up where it lies above. The read of each
 *       shared byte then comes before the store to it.
 * @warning dst must be writable and src readable for bytes.
 */
void mmgr_bytes_move(void *dst, const void *src, size_t bytes);

/**
 * @brief Writes value into each of bytes bytes at dst.
 *
 * @param[out] dst   Destination to write [BORROWS].
 * @param[in]  value The byte to write.
 * @param[in]  bytes Bytes to write.
 * @note memor.set with dst, bytes and val.
 * @warning dst must be writable for bytes.
 */
void mmgr_bytes_set(void *dst, uint8_t value, size_t bytes);

/**
 * @brief Orders two regions by their first difference.
 *
 * @param[in] left  First region [BORROWS].
 * @param[in] right Second region [BORROWS].
 * @param[in] bytes Bytes to compare.
 * @return          The difference of the first unequal byte pair, or 0 where every byte agrees.
 * @note memor.cmp with src, other and bytes.
 * @warning Both regions must be readable for bytes rounded up to a whole word, as mmgr_memor_cmp reads them.
 */
embed_iword mmgr_bytes_compare(const void *left, const void *right, size_t bytes);

/**
 * @brief Finds the first byte equal to value in a region.
 *
 * @param[in] region The region [BORROWS].
 * @param[in] value  The byte sought.
 * @param[in] bytes  Bytes of the region.
 * @return           The address of the first match, or NULL where no byte of the region holds value [BORROWS].
 * @note memor.chr with src, bytes and val. A terminator is not special.
 * @warning region must be readable for bytes rounded up to a whole word, as mmgr_memor_chr reads it.
 */
const void *mmgr_bytes_find(const void *region, uint8_t value, size_t bytes);

/**
 * @brief Sorts the elements of a region in the order of a compare, as qsort sorts them, through a room of the same
 *        size.
 *
 * @param[in,out] base    The elements [BORROWS].
 * @param[in]     count   Elements of the region.
 * @param[in]     size    Bytes of one element.
 * @param[in]     order   The compare: 0 or less where its left element comes at or before its right one.
 * @param[out]    scratch Room for count times size bytes, which the sort writes and leaves as it likes [BORROWS].
 * @return                EMBED_TRUE where the elements are in order. EMBED_FALSE where size is 0 or a pointer is
 *                        NULL, which leaves the elements as they were.
 * @note memor.sort with dst, count, bytes, order and scratch. Two elements that order as equal keep their order.
 * @warning base and scratch must each hold count times size bytes, and must not overlap.
 */
embed_bool mmgr_bytes_sort(void *base, size_t count, size_t size, int (*order)(const void *left, const void *right),
                           void *scratch);

EMBED_END_DECLS

/** @} */

#endif
