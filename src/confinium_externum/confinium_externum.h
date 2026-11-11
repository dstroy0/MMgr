/**
 * @brief Placement between internal and external memory, and a two-buffer index.
 *
 * @warning Everything below exists only when MMGR_ENABLE_EXTRAM holds.
 */
#ifndef MMGR_CONFINIUM_EXTERNUM_H
#define MMGR_CONFINIUM_EXTERNUM_H

#include "config/mmgr_config.h"

#if MMGR_ENABLE_EXTRAM

MMGR_INCIPE_DECLS

/**
 * @brief Where a request should go.
 *
 * @note Packed to one byte; mmgr_types.h asserts that the pack reaches the compiler.
 */
typedef enum MMGR_ENUM_PACKED
{
    PLACE_DRAM = 0,  /**< Internal memory. */
    PLACE_PSRAM = 1, /**< External memory. */
    PLACE_FAIL = 2   /**< Neither will take it. */
} mmgr_place;

/**
 * @brief A pair of buffers, one under fill while the other is under drain.
 *
 * @note fill_idx is the only member; the buffers it indexes live elsewhere.
 */
typedef struct
{
    uint8_t fill_idx; /**< Index of the buffer under fill, 0 or 1. */
} PingPong;

/**
 * @brief Arguments for every exter call; each reads only what it needs.
 *
 * @note The caller supplies free_dram and free_psram and they stand as given.
 * @note place reads the six figures; the pingpong entries read pp alone.
 */
typedef struct
{
    const size_t size;            /**< Bytes to place. */
    const mmgr_bool dma_required; /**< The bytes must be reachable by DMA. */
    const size_t free_dram;       /**< Bytes still free in internal memory. */
    const size_t free_psram;      /**< Bytes still free in external memory. */
    const size_t psram_threshold; /**< Size at or above which external memory comes first. */
    const size_t dram_reserve;    /**< Internal bytes that must remain free afterwards. */
    PingPong *const pp;           /**< Pair the pingpong entries act on [BORROWS]. */
} ExternumCfg;

/**
 * @brief Type of the exter dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the five members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note Every entry takes the same argument pack, as in carceribus and infinitas.
 */
typedef struct
{
    mmgr_place (*place)(const ExternumCfg *c);       /**< Decides where a request goes. */
    void (*pingpong_init)(const ExternumCfg *c);     /**< Points the pair at buffer 0. */
    uint8_t (*pingpong_fill)(const ExternumCfg *c);  /**< Index under fill. */
    uint8_t (*pingpong_drain)(const ExternumCfg *c); /**< Index under drain. */
    uint8_t (*pingpong_swap)(const ExternumCfg *c);  /**< Swaps the two roles. */
} ConfiniumExternumNs;
MMGR_NS_LAYOUT(ConfiniumExternumNs, place, pingpong_init, pingpong_fill, pingpong_drain, pingpong_swap);

/**
 * @brief Decides whether a request belongs in internal or external memory.
 *
 * @param[in] c Request size, the DMA requirement and both memory figures [BORROWS].
 * @return      PLACE_DRAM, PLACE_PSRAM, or PLACE_FAIL when neither will take it.
 * @note A size of 0 gives PLACE_FAIL.
 * @note A DMA request only ever gives PLACE_DRAM or PLACE_FAIL, never external memory.
 * @note At or above psram_threshold external memory comes first, below it internal does.
 * @warning Internal placement must also leave dram_reserve free; the external test is a size comparison alone.
 */
mmgr_place mmgr_exter_place(const ExternumCfg *c);

/**
 * @brief Points the pair at buffer 0.
 *
 * @param[in,out] c Pair to reset, as c->pp [BORROWS].
 * @warning c->pp must not be null.
 */
void mmgr_pingpong_init(const ExternumCfg *c);

/**
 * @brief Returns the index of the buffer under fill.
 *
 * @param[in] c Pair to read, as c->pp [BORROWS].
 * @return      0 or 1.
 * @note Does not modify c->pp.
 * @warning c->pp must not be null.
 */
uint8_t mmgr_pingpong_fill_index(const ExternumCfg *c);

/**
 * @brief Returns the index of the buffer under drain.
 *
 * @param[in] c Pair to read, as c->pp [BORROWS].
 * @return      The other index, 0 or 1.
 * @note Does not modify c->pp.
 * @warning c->pp must not be null.
 */
uint8_t mmgr_pingpong_drain_index(const ExternumCfg *c);

/**
 * @brief Swaps which buffer takes the fill and which the drain.
 *
 * @param[in,out] c Pair to flip, as c->pp [BORROWS].
 * @return          The index now under fill, 0 or 1.
 * @warning c->pp must not be null.
 */
uint8_t mmgr_pingpong_swap(const ExternumCfg *c);

/**
 * @brief Dispatch table instance named exter.
 *
 * @note pingpong_fill calls mmgr_pingpong_fill_index and pingpong_drain calls mmgr_pingpong_drain_index.
 */
MMGR_NS ConfiniumExternumNs exter MMGR_UNUSED = {
    .place = mmgr_exter_place,
    .pingpong_init = mmgr_pingpong_init,
    .pingpong_fill = mmgr_pingpong_fill_index,
    .pingpong_drain = mmgr_pingpong_drain_index,
    .pingpong_swap = mmgr_pingpong_swap,
};

MMGR_FINIS_DECLS

#endif

#endif
