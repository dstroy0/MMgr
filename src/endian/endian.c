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
 * @brief Writes c->width bytes of c->val to c->dst in the target's own order.
 *
 * @param[in,out] c Destination, value and width [BORROWS].
 * @note Dispatches to proxim.put16, put32 or put64 on the width.
 * @warning Any width other than 2 or 4 takes the default branch and writes eight bytes.
 */
MMGR_INLINE void endian_put(const EndianCtx *c)
{
    switch (c->width)
    {
    case 2:
        MMGR_CALL(proxim.put16, ProximusCfg, .dst = c->dst, .val = c->val);
        break;
    case 4:
        MMGR_CALL(proxim.put32, ProximusCfg, .dst = c->dst, .val = c->val);
        break;
    default:
        MMGR_CALL(proxim.put64, ProximusCfg, .dst = c->dst, .val = c->val);
        break;
    }
}

/**
 * @brief Reads c->width bytes from c->src in the target's own order.
 *
 * @param[in] c Source and width [BORROWS].
 * @return      The value read, in the low bytes of the result.
 * @note Dispatches to proxim.load16, load32 or load64 on the width.
 * @warning Any width other than 2 or 4 takes the default branch and reads eight bytes.
 */
MMGR_INLINE uint64_t endian_get(const EndianCtx *c)
{
    switch (c->width)
    {
    case 2:
        return MMGR_CALL(proxim.load16, ProximusCfg, .at = c->src);
    case 4:
        return MMGR_CALL(proxim.load32, ProximusCfg, .at = c->src);
    default:
        return MMGR_CALL(proxim.load64, ProximusCfg, .at = c->src);
    }
}

/**
 * @brief Reverses the byte order of c->val and returns the low c->width bytes.
 *
 * @param[in] c Value and width [BORROWS].
 * @return      The reversed value, right-aligned into the low c->width bytes.
 * @note Swaps at eight, then sixteen, then thirty-two bits, so the whole 64-bit value is reversed first.
 * @note The final shift drops the 8 * (8 - width) bytes the reversal moved above the result.
 * @warning 8u - c->width is unsigned, so a c->width above 8 wraps into a very large shift count.
 */
MMGR_INLINE uint64_t endian_rev(const EndianCtx *c)
{
    uint64_t v = c->val;

    // Suffixed constants keep each mask at uint64_t, matching the value being swapped
    v = ((v & 0x00FF00FF00FF00FFull) << 8) | ((v >> 8) & 0x00FF00FF00FF00FFull);
    v = ((v & 0x0000FFFF0000FFFFull) << 16) | ((v >> 16) & 0x0000FFFF0000FFFFull);
    v = (v << 32) | (v >> 32);
    return v >> (8u * (8u - c->width));
}

/**
 * @brief Writes c->val to c->dst without reversing it.
 *
 * @param[in,out] c Destination, value and width [BORROWS].
 * @return          c->width.
 * @note Calls endian_put directly, where endian_wr_be reverses first.
 */
MMGR_INLINE size_t endian_wr_le(const EndianCtx *c)
{
    endian_put(c);
    return c->width;
}

/**
 * @brief Reverses c->val, then writes it to c->dst.
 *
 * @param[in,out] c Destination, value and width [BORROWS].
 * @return          c->width.
 * @note Builds a fresh EndianCtx holding the reversed value, leaving c untouched.
 */
MMGR_INLINE size_t endian_wr_be(const EndianCtx *c)
{
    MMGR_CALL(endian_put, EndianCtx, .dst = c->dst, .val = endian_rev(c), .width = c->width);
    return c->width;
}

/**
 * @brief Reads c->width bytes from c->src without reversing them.
 *
 * @param[in] c Source and width [BORROWS].
 * @return      The value read.
 * @note Calls endian_get directly, where endian_rd_be reverses the result.
 */
MMGR_INLINE uint64_t endian_rd_le(const EndianCtx *c)
{
    return endian_get(c);
}

/**
 * @brief Reads c->width bytes from c->src, then reverses them.
 *
 * @param[in] c Source and width [BORROWS].
 * @return      The reversed value, right-aligned into the low c->width bytes.
 * @note Feeds endian_get's result into endian_rev through a fresh EndianCtx.
 */
MMGR_INLINE uint64_t endian_rd_be(const EndianCtx *c)
{
    return MMGR_CALL(endian_rev, EndianCtx, .val = endian_get(c), .width = c->width);
}

/**
 * @brief Writes c->val to c->dst without reversing it.
 *
 * @note Documented at the declaration in endian.h.
 */
size_t mmgr_wr_le(const EndianCfg *c)
{
    // Explicit cast widens the packed enum to size_t, which the mmgr_endian_width member then narrows back
    return MMGR_CALL(endian_wr_le, EndianCtx, .dst = c->dst, .val = c->val, .width = (size_t)c->width);
}

/**
 * @brief Reads c->width bytes from c->src without reversing them.
 *
 * @note Documented at the declaration in endian.h.
 */
uint64_t mmgr_rd_le(const EndianCfg *c)
{
    // Explicit cast widens the packed enum to size_t, which the mmgr_endian_width member then narrows back
    return MMGR_CALL(endian_rd_le, EndianCtx, .src = c->src, .width = (size_t)c->width);
}

/**
 * @brief Reverses c->val, then writes it to c->dst.
 *
 * @note Documented at the declaration in endian.h.
 */
size_t mmgr_wr_be(const EndianCfg *c)
{
    // Explicit cast widens the packed enum to size_t, which the mmgr_endian_width member then narrows back
    return MMGR_CALL(endian_wr_be, EndianCtx, .dst = c->dst, .val = c->val, .width = (size_t)c->width);
}

/**
 * @brief Reads c->width bytes from c->src, then reverses them.
 *
 * @note Documented at the declaration in endian.h.
 */
uint64_t mmgr_rd_be(const EndianCfg *c)
{
    // Explicit cast widens the packed enum to size_t, which the mmgr_endian_width member then narrows back
    return MMGR_CALL(endian_rd_be, EndianCtx, .src = c->src, .width = (size_t)c->width);
}

/**
 * @brief Reverses the byte order of c->val at c->width bytes.
 *
 * @note Documented at the declaration in endian.h.
 */
uint64_t mmgr_endian_rev(const EndianCfg *c)
{
    // Explicit cast widens the packed enum to size_t, which the mmgr_endian_width member then narrows back
    return MMGR_CALL(endian_rev, EndianCtx, .val = c->val, .width = (size_t)c->width);
}
