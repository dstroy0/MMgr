/**
 * @brief Byte-level copy, move, compare, search and fill.
 *
 * @note cpy, move_up and set move whole words then odd bytes; cmp and chr mask the tail lanes instead.
 */
#include "memoria_operor/memoria_operor.h"

#include "proximus_operor/proximus_operor.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @brief Arguments for the forward copy.
 *
 * @warning Both pointers are restrict qualified, so the two regions must not overlap.
 */
typedef struct
{
    uint8_t *restrict dst;       /**< Destination [BORROWS]. */
    const uint8_t *restrict src; /**< Source [BORROWS]. */
    size_t bytes;                /**< Bytes to copy. */
} MemorCpyCtx;

/**
 * @brief Arguments for the backward move.
 *
 * @note Neither pointer is restrict qualified, unlike MemorCpyCtx.
 */
typedef struct
{
    uint8_t *dst;       /**< Destination [BORROWS]. */
    const uint8_t *src; /**< Source [BORROWS]. */
    size_t bytes;       /**< Bytes to move. */
} MemorMoveCtx;

/**
 * @brief Arguments for the compare and the byte search.
 *
 * @note cmp reads src, other and bytes; chr reads src, bytes and val.
 */
typedef struct
{
    const uint8_t *src;   /**< First region, and the one chr searches [BORROWS]. */
    const uint8_t *other; /**< Second region for cmp [BORROWS]. */
    size_t bytes;         /**< Bytes to examine. */
    uint8_t val;          /**< Byte chr looks for. */
} MemorScanCtx;

/**
 * @brief Arguments for the fill.
 */
typedef struct
{
    uint8_t *dst; /**< Destination [BORROWS]. */
    size_t bytes; /**< Bytes to write. */
    uint8_t val;  /**< Byte to write into each of them. */
} MemorSetCtx;

/**
 * @brief Copies c->bytes from c->src to c->dst, walking upward.
 *
 * @param[in,out] c Destination, source and count [BORROWS].
 * @note Moves whole words first, then the remaining bytes one at a time.
 * @note Advances c->dst and c->src as it goes, so both point past the copy when it returns.
 * @warning The regions must not overlap; MemorCpyCtx declares both pointers restrict.
 */
MMGR_INLINE void memor_cpy(MemorCpyCtx *c)
{
    // Explicit cast holds the remainder mask at size_t, matching the byte count it is applied to
    size_t t = c->bytes & (size_t)(MMGR_RAW_WORD - 1u);
    size_t w = c->bytes - t;

    if (w != 0u)
    {
        do
        {
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = c->dst,
                      .val = MMGR_CALL(proxim.al_load, ProximusCfg, .at = c->src));
            c->dst += MMGR_RAW_WORD;
            c->src += MMGR_RAW_WORD;
            w -= MMGR_RAW_WORD;
        } while (w);
    }
    if (t != 0u)
    {
        do
        {
            *c->dst++ = *c->src++;
        } while (--t);
    }
}

/**
 * @brief Copies c->bytes from c->src to c->dst, walking downward from the far end.
 *
 * @param[in,out] c Destination, source and count [BORROWS].
 * @note Starts at the end of both regions and works back toward the start.
 * @note Takes the odd bytes first, then whole words, which is the reverse of memor_cpy's order.
 * @note Advances both pointers to the end, then walks them back, so each ends where it began.
 */
MMGR_INLINE void memor_move_up(MemorMoveCtx *c)
{
    // Explicit cast holds the remainder mask at size_t, matching the byte count it is applied to
    size_t t = c->bytes & (size_t)(MMGR_RAW_WORD - 1u);
    size_t w = c->bytes - t;

    c->dst += c->bytes;
    c->src += c->bytes;

    if (t != 0u)
    {
        do
        {
            *--c->dst = *--c->src;
        } while (--t);
    }
    if (w != 0u)
    {
        do
        {
            c->dst -= MMGR_RAW_WORD;
            c->src -= MMGR_RAW_WORD;
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = c->dst,
                      .val = MMGR_CALL(proxim.al_load, ProximusCfg, .at = c->src));
            w -= MMGR_RAW_WORD;
        } while (w);
    }
}

/**
 * @brief Compares c->bytes of c->src against c->other.
 *
 * @param[in] c The two regions and the count [BORROWS].
 * @return      The difference of the first unequal byte pair, or 0 when every byte matches.
 * @note Scans a word at a time; mask.lanes_below keeps lanes past the count out of the result.
 * @note The sign follows the differing bytes, so the result orders the two regions.
 */
MMGR_INLINE mmgr_iword memor_cmp(MemorScanCtx *c)
{
    for (size_t at = 0; at < c->bytes; at += MMGR_SWAR_BYTES)
    {
        const mmgr_word d = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at) ^
                                  MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at);
        // Explicit cast holds the differing-lane mask at mmgr_word width, bounded to the bytes still in range
        const mmgr_word m =
            (mmgr_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = d)) &
                              MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = c->bytes - at));
        if (m != 0)
        {
            const size_t k = at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);

            // Explicit casts widen both bytes to mmgr_iword so the difference keeps its sign
            return (mmgr_iword)c->src[k] - (mmgr_iword)c->other[k];
        }
    }
    return 0;
}

/**
 * @brief Finds the first byte in c->src equal to c->val, within c->bytes.
 *
 * @param[in] c Region, count and the byte sought [BORROWS].
 * @return      Address of the match, or NULL when the byte does not occur [BORROWS].
 * @note Scans a word at a time; mask.lanes_below keeps lanes past the count out of the result.
 * @note A terminator is not special here; all c->bytes are searched.
 */
MMGR_INLINE const void *memor_chr(MemorScanCtx *c)
{
    for (size_t at = 0; at < c->bytes; at += MMGR_SWAR_BYTES)
    {
        const mmgr_word w = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at);
        // Explicit cast holds the match mask at mmgr_word width, bounded to the bytes still in range
        const mmgr_word m =
            (mmgr_word)(MMGR_CALL(lane.eq, ScrutLaneCfg, .word = w, .byte = c->val, .ci = MMGR_FALSE) &
                              MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = c->bytes - at));
        if (m != 0)
        {
            return c->src + at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return NULL;
}

/**
 * @brief Writes c->val into c->bytes of c->dst.
 *
 * @param[in,out] c Destination, count and the byte to write [BORROWS].
 * @note Builds a word with c->val in every lane, stores whole words, then finishes byte by byte.
 * @note Advances c->dst as it goes, so it points past the fill when it returns.
 */
MMGR_INLINE void memor_set(MemorSetCtx *c)
{
    // Explicit casts broadcast the byte into every lane: MMGR_SWAR_ONES has a 1 in each lane's low bit
    const mmgr_migro_word fill = (mmgr_migro_word)(MMGR_SWAR_ONES * (mmgr_migro_word)c->val);

    // Explicit cast holds the remainder mask at size_t, matching the byte count it is applied to
    size_t t = c->bytes & (size_t)(MMGR_RAW_WORD - 1u);
    size_t w = c->bytes - t;

    if (w != 0u)
    {
        do
        {
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = c->dst, .val = fill);
            c->dst += MMGR_RAW_WORD;
            w -= MMGR_RAW_WORD;
        } while (w);
    }
    if (t != 0u)
    {
        do
        {
            *c->dst++ = c->val;
        } while (--t);
    }
}

/**
 * @brief Copies c->bytes from c->src to c->dst, walking upward.
 *
 * @note Documented at the declaration in memoria_operor.h.
 */
void mmgr_memor_cpy(const MemoriaCfg *c)
{
    // Explicit casts convert the void pointers to the uint8_t pointers MemorCpyCtx declares
    MMGR_CALL(memor_cpy, MemorCpyCtx, .dst = (uint8_t *)c->dst, .src = (const uint8_t *)c->src, .bytes = c->bytes);
}

/**
 * @brief Copies c->bytes from c->src to c->dst, walking downward from the far end.
 *
 * @note Documented at the declaration in memoria_operor.h.
 */
void mmgr_memor_move_up(const MemoriaCfg *c)
{
    // Explicit casts convert the void pointers to the uint8_t pointers MemorMoveCtx declares
    MMGR_CALL(memor_move_up, MemorMoveCtx, .dst = (uint8_t *)c->dst, .src = (const uint8_t *)c->src, .bytes = c->bytes);
}

/**
 * @brief Compares c->bytes of c->src against c->other.
 *
 * @note Documented at the declaration in memoria_operor.h.
 */
mmgr_iword mmgr_memor_cmp(const MemoriaCfg *c)
{
    // Explicit casts convert the void pointers to the const uint8_t pointers MemorScanCtx declares
    return MMGR_CALL(memor_cmp, MemorScanCtx, .src = (const uint8_t *)c->src, .other = (const uint8_t *)c->other,
                     .bytes = c->bytes);
}

/**
 * @brief Finds the first byte in c->src equal to c->val, within c->bytes.
 *
 * @note Documented at the declaration in memoria_operor.h.
 */
const void *mmgr_memor_chr(const MemoriaCfg *c)
{
    // Explicit cast converts the void pointer to the const uint8_t pointer MemorScanCtx declares
    return MMGR_CALL(memor_chr, MemorScanCtx, .src = (const uint8_t *)c->src, .bytes = c->bytes, .val = c->val);
}

/**
 * @brief Writes c->val into c->bytes of c->dst.
 *
 * @note Documented at the declaration in memoria_operor.h.
 */
void mmgr_memor_set(const MemoriaCfg *c)
{
    // Explicit cast converts the void pointer to the uint8_t pointer MemorSetCtx declares
    MMGR_CALL(memor_set, MemorSetCtx, .dst = (uint8_t *)c->dst, .bytes = c->bytes, .val = c->val);
}
