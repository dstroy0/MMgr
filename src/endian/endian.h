/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Endian reads and writes: the width enum, the arguments, and the two order tables.
 *
 * @note parva_extremitas is little endian, magna_extremitas big endian; both share the same calls.
 */
#ifndef MMGR_ENDIAN_H
#define MMGR_ENDIAN_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Width of one endian read or write, counted in bytes.
 *
 * @note The enumerators are the byte counts themselves, so the implementation switches on them directly.
 */
typedef enum MMGR_ENUM_PACKED
{
    MMGR_ENDIAN_16 = 2, /**< Two bytes. */
    MMGR_ENDIAN_32 = 4, /**< Four bytes. */
    MMGR_ENDIAN_64 = 8, /**< Eight bytes. */
} mmgr_endian_width;

/**
 * @brief Arguments for the endian calls; each reads only what it needs.
 *
 * @note wr reads dst, val and width; rd reads src and width; rev reads val and width.
 */
typedef struct
{
    uint8_t *const dst;            /**< Destination for wr [BORROWS]. */
    const uint8_t *const src;      /**< Source for rd [BORROWS]. */
    const uint64_t val;            /**< Value for wr, or the value rev reverses. */
    const mmgr_endian_width width; /**< Bytes the call moves. */
} EndianCfg;

/**
 * @brief Type of an endian dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the three members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note Two instances share this type, one per byte order.
 */
typedef struct
{
    size_t (*wr)(const EndianCfg *args);    /**< Writes val to dst and returns width. */
    uint64_t (*rd)(const EndianCfg *args);  /**< Reads width bytes from src. */
    uint64_t (*rev)(const EndianCfg *args); /**< Reverses val at width bytes. */
} EndianNs;
MMGR_NS_LAYOUT(EndianNs, wr, rd, rev);

/**
 * @brief Writes args->width bytes of args->val to args->dst without reversing them.
 *
 * @param[in,out] args Destination, value and width [BORROWS].
 * @return          args->width.
 * @warning args->dst must be writable for args->width bytes.
 */
size_t mmgr_wr_le(const EndianCfg *args);

/**
 * @brief Reads args->width bytes from args->src without reversing them.
 *
 * @param[in] args Source and width [BORROWS].
 * @return      The value read, in the low args->width bytes.
 * @warning args->src must be readable for args->width bytes.
 */
uint64_t mmgr_rd_le(const EndianCfg *args);

/**
 * @brief Reverses args->val, then writes args->width bytes of it to args->dst.
 *
 * @param[in,out] args Destination, value and width [BORROWS].
 * @return          args->width.
 * @warning args->dst must be writable for args->width bytes.
 */
size_t mmgr_wr_be(const EndianCfg *args);

/**
 * @brief Reads args->width bytes from args->src, then reverses them.
 *
 * @param[in] args Source and width [BORROWS].
 * @return      The reversed value, in the low args->width bytes.
 * @warning args->src must be readable for args->width bytes.
 */
uint64_t mmgr_rd_be(const EndianCfg *args);

/**
 * @brief Reverses the byte order of args->val at args->width bytes.
 *
 * @param[in] args Value and width [BORROWS].
 * @return      The reversed value, right-aligned into the low args->width bytes.
 * @note Touches no memory; both tables point rev at this one function.
 */
uint64_t mmgr_endian_rev(const EndianCfg *args);

/**
 * @brief Dispatch table instance named parva_extremitas, the little endian order.
 *
 * @note wr and rd move bytes as they lie; rev is the same function both tables use.
 */
MMGR_NS EndianNs parva_extremitas MMGR_UNUSED = {
    .wr = mmgr_wr_le,
    .rd = mmgr_rd_le,
    .rev = mmgr_endian_rev,
};

/**
 * @brief Dispatch table instance named magna_extremitas, the big endian order.
 *
 * @note wr reverses before writing and rd reverses after reading; rev is shared with parva_extremitas.
 */
MMGR_NS EndianNs magna_extremitas MMGR_UNUSED = {
    .wr = mmgr_wr_be,
    .rd = mmgr_rd_be,
    .rev = mmgr_endian_rev,
};

MMGR_FINIS_DECLS

#endif
