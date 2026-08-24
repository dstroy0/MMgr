/**
 * @brief Double-ended pool: its state, its arguments, and the carcer dispatch table.
 */
#ifndef MMGR_CARCERIBUS_H
#define MMGR_CARCERIBUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Address and extent of a whole region.
 *
 * @note Set by the mmgr_carcer_init macro to the region's bytes array and its size.
 */
typedef struct
{
    uint8_t *const at; /**< First byte of the region [BORROWS]. */
    const size_t size; /**< Bytes in the region. */
} CarcerInit;

/**
 * @brief One pool's state: its bytes and the two ends that grow towards each other.
 *
 * @note Set up by the mmgr_carcer_init macro, which starts interim_top at the pool size.
 */
typedef struct
{
    uint8_t *const base; /**< First byte of the pool [BORROWS]. */
    const size_t size;   /**< Bytes in the pool. */
    size_t persist_end;  /**< Offset just past the persistent bytes, counting up from base. */
    size_t interim_top;  /**< Offset of the lowest interim byte, counting down from size. */
#if MMGR_ENABLE_HW_MEM_CAPACITY_CB
    size_t hw;           /**< Highest total occupancy seen, both ends together. */
#endif
} CarcerCtx;

/**
 * @brief Arguments for every carcer call: the pool, a size or mark, and an address.
 *
 * @note Each call reads only what it needs; size is a byte count except in interim_reddo.
 */
typedef struct
{
    CarcerCtx *const pool; /**< Pool to act on [BORROWS]. */
    const size_t size;     /**< Byte count, or the restore mark for interim_reddo. */
    const void *const at;  /**< Address tested by owns [BORROWS]. */
} CarcerCfg;

/**
 * @brief Type of the carcer dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the nine members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 */
typedef struct
{
    void *(*persist_capio)(const CarcerCfg *c);  /**< Takes size bytes from the bottom. */
    void (*persist_reddo)(const CarcerCfg *c);   /**< Gives size bytes back to the bottom. */
    void *(*interim_capio)(const CarcerCfg *c);  /**< Takes size bytes from the top. */
    size_t (*interim_mark)(const CarcerCfg *c);  /**< Reads the current top. */
    void (*interim_reddo)(const CarcerCfg *c);   /**< Restores the top to a mark. */
    void (*interim_reset)(const CarcerCfg *c);   /**< Releases every interim byte. */
    mmgr_bool (*owns)(const CarcerCfg *c);       /**< Tests whether at is inside the pool. */
    size_t (*octas_praesto)(const CarcerCfg *c); /**< Bytes free between the two ends. */
    size_t (*persist_used)(const CarcerCfg *c);  /**< Bytes taken from the bottom. */
} CarceribusNs;
MMGR_NS_LAYOUT(CarceribusNs, persist_capio, persist_reddo, interim_capio, interim_mark, interim_reddo, interim_reset,
               owns, octas_praesto, persist_used);

/**
 * @brief Takes c->size bytes from the bottom of the pool.
 *
 * @param[in,out] c Pool and byte count [BORROWS].
 * @return          Start of the taken bytes [BORROWS].
 * @note Advances persist_end by c->size.
 * @warning c->size must not exceed the bytes reported by octas_praesto.
 */
void *mmgr_carcer_persist_capio(const CarcerCfg *c);

/**
 * @brief Gives c->size bytes back to the bottom of the pool.
 *
 * @param[in,out] c Pool and byte count [BORROWS].
 * @note Moves persist_end back by c->size.
 * @warning c->size must not exceed the bytes reported by persist_used.
 */
void mmgr_carcer_persist_reddo(const CarcerCfg *c);

/**
 * @brief Takes c->size bytes from the top of the pool.
 *
 * @param[in,out] c Pool and byte count [BORROWS].
 * @return          Start of the taken bytes [BORROWS].
 * @note Lowers interim_top by c->size.
 * @warning c->size must not exceed the bytes reported by octas_praesto.
 */
void *mmgr_carcer_interim_capio(const CarcerCfg *c);

/**
 * @brief Reads the pool's current interim_top.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      A mark to pass back as c->size in interim_reddo.
 * @note Reads only; the pool is unchanged.
 */
size_t mmgr_carcer_interim_mark(const CarcerCfg *c);

/**
 * @brief Restores interim_top to a mark taken by interim_mark.
 *
 * @param[in,out] c Pool, with the mark in c->size [BORROWS].
 * @note Releases every interim byte taken since the mark.
 * @warning c->size must be a mark from this pool, not a byte count.
 */
void mmgr_carcer_interim_reddo(const CarcerCfg *c);

/**
 * @brief Sets interim_top back to the pool size.
 *
 * @param[in,out] c Pool to reset [BORROWS].
 * @note Releases every interim byte; persist_end is untouched.
 */
void mmgr_carcer_interim_reset(const CarcerCfg *c);

/**
 * @brief Returns whether c->at lies within the pool's bytes.
 *
 * @param[in] c Pool and the address to test [BORROWS].
 * @return      MMGR_TRUE when c->at is at or after base and before base plus size.
 * @note Reads only; the pool is unchanged.
 */
mmgr_bool mmgr_carcer_owns(const CarcerCfg *c);

/**
 * @brief Returns the bytes still free between the two ends.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      interim_top minus persist_end.
 * @note Reads only; the pool is unchanged.
 */
size_t mmgr_carcer_octas_praesto(const CarcerCfg *c);

/**
 * @brief Returns the bytes taken from the bottom of the pool.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      persist_end, which is also the offset of the next persistent byte.
 * @note Reads only; the pool is unchanged.
 */
size_t mmgr_carcer_persist_used(const CarcerCfg *c);

/**
 * @brief Dispatch table instance named carcer; each member calls the matching mmgr_carcer_ function.
 */
MMGR_NS CarceribusNs carcer MMGR_UNUSED = {
    .persist_capio = mmgr_carcer_persist_capio,
    .persist_reddo = mmgr_carcer_persist_reddo,
    .interim_capio = mmgr_carcer_interim_capio,
    .interim_mark = mmgr_carcer_interim_mark,
    .interim_reddo = mmgr_carcer_interim_reddo,
    .interim_reset = mmgr_carcer_interim_reset,
    .owns = mmgr_carcer_owns,
    .octas_praesto = mmgr_carcer_octas_praesto,
    .persist_used = mmgr_carcer_persist_used,
};

MMGR_FINIS_DECLS

#endif
