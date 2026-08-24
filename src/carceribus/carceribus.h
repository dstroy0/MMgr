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
 * @brief One pool's state: its bytes and the two ends that grow toward each other.
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
    size_t hw;           /**< Running maximum of persist_end and of the bytes taken from the top. */
#endif
} CarcerCtx;

/**
 * @brief Arguments for every carcer call: the pool, a size or mark, and an address.
 *
 * @note Each call reads only what it needs.
 */
typedef struct
{
    CarcerCtx *const pool; /**< Pool to act on [BORROWS]. */
    const size_t size;     /**< Byte count for persist_capio, persist_reddo and interim_capio. */
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
    void (*interim_reddo)(const CarcerCfg *c);   /**< Sets the top from interim_mark. */
    void (*interim_reset)(const CarcerCfg *c);   /**< Assigns the top the pool's size. */
    mmgr_bool (*owns)(const CarcerCfg *c);       /**< Tests whether at lies in the pool's bytes. */
    size_t (*octas_praesto)(const CarcerCfg *c); /**< Returns interim_top minus persist_end. */
    size_t (*persist_used)(const CarcerCfg *c);  /**< Returns the pool's persist_end. */
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
 * @brief Moves persist_end back by c->size.
 *
 * @param[in,out] c Pool and the byte count to give back [BORROWS].
 * @warning c->size must not exceed the value mmgr_carcer_persist_used reports.
 */
void mmgr_carcer_persist_reddo(const CarcerCfg *c);

/**
 * @brief Lowers interim_top by c->size and returns base plus the new interim_top.
 *
 * @param[in,out] c Pool and the byte count wanted [BORROWS].
 * @return          Start of the lowered region [BORROWS].
 * @warning c->size must not exceed the value mmgr_carcer_octas_praesto reports.
 */
void *mmgr_carcer_interim_capio(const CarcerCfg *c);

/**
 * @brief Returns the pool's current interim_top.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      The value of interim_top.
 * @note Writes nothing.
 */
size_t mmgr_carcer_interim_mark(const CarcerCfg *c);

/**
 * @brief Assigns interim_top the value mmgr_carcer_interim_mark returns for this pool.
 *
 * @param[in,out] c Pool to act on [BORROWS].
 * @note c->size is not read.
 */
void mmgr_carcer_interim_reddo(const CarcerCfg *c);

/**
 * @brief Assigns interim_top the pool's size.
 *
 * @param[in,out] c Pool to act on [BORROWS].
 * @note persist_end is not written.
 */
void mmgr_carcer_interim_reset(const CarcerCfg *c);

/**
 * @brief Returns whether c->at lies in the pool's bytes.
 *
 * @param[in] c Pool and the address to test [BORROWS].
 * @return      MMGR_TRUE when c->at is at or after base and before base plus size.
 * @note Writes nothing.
 */
mmgr_bool mmgr_carcer_owns(const CarcerCfg *c);

/**
 * @brief Returns interim_top minus persist_end.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      The bytes between the two ends.
 * @note Writes nothing.
 */
size_t mmgr_carcer_octas_praesto(const CarcerCfg *c);

/**
 * @brief Returns the pool's persist_end.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      The value of persist_end.
 * @note Writes nothing.
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
