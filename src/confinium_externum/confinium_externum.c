/**
 * @brief Chooses between internal and external memory, and flips a two-buffer index.
 *
 * @warning The whole file is compiled only when MMGR_ENABLE_EXTRAM is set.
 */
#include "confinium_externum/confinium_externum.h"

#if MMGR_ENABLE_EXTRAM

/**
 * @brief Arguments the placement decision reads.
 *
 * @note Mirrors ExternumCfg without its const qualifiers.
 */
typedef struct
{
    size_t size;              /**< Bytes the caller wants to place. */
    mmgr_bool dma_required;   /**< The bytes must be reachable by DMA. */
    size_t free_dram;         /**< Bytes still free in internal memory. */
    size_t free_psram;        /**< Bytes still free in external memory. */
    size_t psram_threshold;   /**< Size at or above which external memory is tried first. */
    size_t dram_reserve;      /**< Internal bytes that must remain free after the placement. */
} ExterCtx;

/**
 * @brief Reports whether the request fits internal memory and still leaves the reserve.
 *
 * @param[in] c Request size and the internal memory figures [BORROWS].
 * @return      MMGR_TRUE when the bytes fit and dram_reserve would still be free afterwards.
 * @note Tests the size first, so the subtraction that follows cannot wrap.
 */
MMGR_INLINE mmgr_bool exter_dram_fits(const ExterCtx *c)
{
    return (c->size <= c->free_dram) && ((c->free_dram - c->size) >= c->dram_reserve);
}

/**
 * @brief Reports whether the request fits external memory.
 *
 * @param[in] c Request size and the external memory figure [BORROWS].
 * @return      MMGR_TRUE when the bytes fit.
 * @note A single comparison, where exter_dram_fits also checks dram_reserve.
 */
MMGR_INLINE mmgr_bool exter_psram_fits(const ExterCtx *c)
{
    return c->size <= c->free_psram;
}

/**
 * @brief Decides where a request should be placed.
 *
 * @param[in] c Request size, the DMA requirement and both memory figures [BORROWS].
 * @return      PLACE_DRAM, PLACE_PSRAM, or PLACE_FAIL when neither will take it.
 * @note A size of 0 is refused outright.
 * @note A DMA request only ever goes to internal memory, and fails rather than falling back.
 * @note At or above psram_threshold external memory is tried first, below it internal is.
 */
MMGR_INLINE mmgr_place exter_place(const ExterCtx *c)
{
    if (c->size == 0)
    {
        return PLACE_FAIL;
    }

    const mmgr_bool d_fits = exter_dram_fits(c);
    const mmgr_bool p_fits = exter_psram_fits(c);

    if (c->dma_required)
    {
        return d_fits ? PLACE_DRAM : PLACE_FAIL;
    }

    if (c->size >= c->psram_threshold)
    {
        if (p_fits)
        {
            return PLACE_PSRAM;
        }
        if (d_fits)
        {
            return PLACE_DRAM;
        }
        return PLACE_FAIL;
    }

    if (d_fits)
    {
        return PLACE_DRAM;
    }
    if (p_fits)
    {
        return PLACE_PSRAM;
    }
    return PLACE_FAIL;
}

/**
 * @brief Points the pair at buffer 0.
 *
 * @param[out] pp Pair to reset [BORROWS].
 */
MMGR_INLINE void exter_pingpong_init(PingPong *const pp)
{
    pp->fill_idx = 0;
}

/**
 * @brief Returns the index of the buffer currently being filled.
 *
 * @param[in] pp Pair to read [BORROWS].
 * @return       0 or 1.
 */
MMGR_INLINE uint8_t exter_pingpong_fill(PingPong *const pp)
{
    return pp->fill_idx;
}

/**
 * @brief Returns the index of the buffer currently being drained.
 *
 * @param[in] pp Pair to read [BORROWS].
 * @return       The other index, 0 or 1.
 */
MMGR_INLINE uint8_t exter_pingpong_drain(PingPong *const pp)
{
    // Explicit cast keeps the result in uint8_t after the exclusive or promotes to int
    return (uint8_t)(pp->fill_idx ^ 1u);
}

/**
 * @brief Swaps the two roles and returns the new fill index.
 *
 * @param[in,out] pp Pair to flip [BORROWS].
 * @return           The index now being filled, 0 or 1.
 */
MMGR_INLINE uint8_t exter_pingpong_swap(PingPong *const pp)
{
    pp->fill_idx ^= 1u;
    return pp->fill_idx;
}

/**
 * @brief Copies c into an ExterCtx and returns exter_place's decision.
 *
 * @note Documented at the declaration in confinium_externum.h.
 */
mmgr_place mmgr_exter_place(const ExternumCfg *c)
{
    return MMGR_CALL(exter_place, ExterCtx, .size = c->size, .dma_required = c->dma_required, .free_dram = c->free_dram,
                     .free_psram = c->free_psram, .psram_threshold = c->psram_threshold,
                     .dram_reserve = c->dram_reserve);
}

/**
 * @brief Points the pair at buffer 0.
 *
 * @note Documented at the declaration in confinium_externum.h.
 */
void mmgr_pingpong_init(PingPong *const pp)
{
    exter_pingpong_init(pp);
}

/**
 * @brief Returns the index being filled.
 *
 * @note Documented at the declaration in confinium_externum.h.
 */
uint8_t mmgr_pingpong_fill_index(PingPong *const pp)
{
    return exter_pingpong_fill(pp);
}

/**
 * @brief Returns the index being drained.
 *
 * @note Documented at the declaration in confinium_externum.h.
 */
uint8_t mmgr_pingpong_drain_index(PingPong *const pp)
{
    return exter_pingpong_drain(pp);
}

/**
 * @brief Swaps the two roles and returns the new fill index.
 *
 * @note Documented at the declaration in confinium_externum.h.
 */
uint8_t mmgr_pingpong_swap(PingPong *const pp)
{
    return exter_pingpong_swap(pp);
}

#endif
