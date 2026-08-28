/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Little and big endian reads and writes at two, four or eight bytes.
 */
#include "endian/endian.h"
#include "proximus_operor/proximus_operor.h"

/**
 * @brief Arguments for the endian backends.
 *
 * @note Mirrors EndianCfg without its const qualifiers.
 */
typedef struct
{
    uint8_t *dst;            /**< Destination for the write calls [BORROWS]. */
    const uint8_t *src;      /**< Source for the read calls [BORROWS]. */
    uint64_t val;            /**< Value to write, or the value to reverse. */
    mmgr_endian_width width; /**< Bytes the call moves: 2, 4 or 8. */
} EndianCtx;

/**
 * @brief Writes args->width bytes of args->val to args->dst in the target's own order.
 *
 * @param[in,out] args Destination, value and width [BORROWS].
 * @note Dispatches to proxim.put16, put32 or put64 on the width.
 * @warning Any width other than 2 or 4 takes the default branch and writes eight bytes.
 */
MMGR_INLINE void endian_put(const EndianCtx *args)
{
    switch (args->width)
    {
    case 2:
        MMGR_CALL(proxim.put16, ProximusCfg, .dst = args->dst, .val = args->val);
        break;
    case 4:
        MMGR_CALL(proxim.put32, ProximusCfg, .dst = args->dst, .val = args->val);
        break;
    default:
        MMGR_CALL(proxim.put64, ProximusCfg, .dst = args->dst, .val = args->val);
        break;
    }
}

/**
 * @brief Reads args->width bytes from args->src in the target's own order.
 *
 * @param[in] args Source and width [BORROWS].
 * @return      The value read, in the low bytes of the result.
 * @note Dispatches to proxim.load16, load32 or load64 on the width.
 * @warning Any width other than 2 or 4 takes the default branch and reads eight bytes.
 */
MMGR_INLINE uint64_t endian_get(const EndianCtx *args)
{
    switch (args->width)
    {
    case 2:
        return MMGR_CALL(proxim.load16, ProximusCfg, .at = args->src);
    case 4:
        return MMGR_CALL(proxim.load32, ProximusCfg, .at = args->src);
    default:
        return MMGR_CALL(proxim.load64, ProximusCfg, .at = args->src);
    }
}

/**
 * @brief Reverses the byte order of args->val and returns the low args->width bytes.
 *
 * @param[in] args Value and width [BORROWS].
 * @return      The reversed value, right-aligned into the low args->width bytes.
 * @note Swaps at eight, then sixteen, then thirty-two bits, so the whole 64-bit value is reversed first.
 * @note The final shift drops the 8 * (8 - width) bytes the reversal moved above the result.
 * @warning 8u - args->width is unsigned, so an args->width above 8 wraps into a very large shift count.
 */
MMGR_INLINE uint64_t endian_rev(const EndianCtx *args)
{
    uint64_t v = args->val;

    // Suffixed constants keep each mask at uint64_t, matching the value being swapped
    v = ((v & 0x00FF00FF00FF00FFull) << 8) | ((v >> 8) & 0x00FF00FF00FF00FFull);
    v = ((v & 0x0000FFFF0000FFFFull) << 16) | ((v >> 16) & 0x0000FFFF0000FFFFull);
    v = (v << 32) | (v >> 32);
    return v >> (8u * (8u - args->width));
}

/**
 * @brief Writes args->val to args->dst without reversing it.
 *
 * @param[in,out] args Destination, value and width [BORROWS].
 * @return          args->width.
 * @note Calls endian_put directly, where endian_wr_be reverses first.
 */
MMGR_INLINE size_t endian_wr_le(const EndianCtx *args)
{
    endian_put(args);
    return args->width;
}

/**
 * @brief Reverses args->val, then writes it to args->dst.
 *
 * @param[in,out] args Destination, value and width [BORROWS].
 * @return          args->width.
 * @note Builds a fresh EndianCtx holding the reversed value, leaving c untouched.
 */
MMGR_INLINE size_t endian_wr_be(const EndianCtx *args)
{
    MMGR_CALL(endian_put, EndianCtx, .dst = args->dst, .val = endian_rev(args), .width = args->width);
    return args->width;
}

/**
 * @brief Reads args->width bytes from args->src without reversing them.
 *
 * @param[in] args Source and width [BORROWS].
 * @return      The value read.
 * @note Calls endian_get directly, where endian_rd_be reverses the result.
 */
MMGR_INLINE uint64_t endian_rd_le(const EndianCtx *args)
{
    return endian_get(args);
}

/**
 * @brief Reads args->width bytes from args->src, then reverses them.
 *
 * @param[in] args Source and width [BORROWS].
 * @return      The reversed value, right-aligned into the low args->width bytes.
 * @note Feeds endian_get's result into endian_rev through a fresh EndianCtx.
 */
MMGR_INLINE uint64_t endian_rd_be(const EndianCtx *args)
{
    return MMGR_CALL(endian_rev, EndianCtx, .val = endian_get(args), .width = args->width);
}

/**
 * @brief Binds the four order entries to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_ and endian_ prefixes, which the two share.
 */
#define ENDIAN_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_, endian_, EndianCtx, EndianCfg, ret, name, __VA_ARGS__)

/**
 * @brief Binds the reversal entry, which carries the longer public prefix.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_endian_ and endian_ prefixes.
 * @note A second macro because this entry is named mmgr_endian_rev while the four above are named
 *       mmgr_wr_le and its kin. GENERIC_ENTRY pastes one prefix onto one name, so only the pair differs.
 */
#define ENDIAN_REV_ENTRY(ret, name, ...)                                                                               \
    GENERIC_ENTRY(mmgr_endian_, endian_, EndianCtx, EndianCfg, ret, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in endian.h.
 * @note args->width is forwarded as it stands. EndianCfg and EndianCtx both declare it mmgr_endian_width,
 *       so there is no conversion to make.
 */
ENDIAN_ENTRY(size_t, wr_le, .dst = args->dst, .val = args->val, .width = args->width)
ENDIAN_ENTRY(uint64_t, rd_le, .src = args->src, .width = args->width)
ENDIAN_ENTRY(size_t, wr_be, .dst = args->dst, .val = args->val, .width = args->width)
ENDIAN_ENTRY(uint64_t, rd_be, .src = args->src, .width = args->width)
ENDIAN_REV_ENTRY(uint64_t, rev, .val = args->val, .width = args->width)
