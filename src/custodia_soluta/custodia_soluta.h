/**
 * @brief Persistent tenancy released without wiping: its arguments and the soluta table.
 *
 * @note The counterpart to custodia_secura, which zeroes the bytes on release.
 */
#ifndef MMGR_CUSTODIA_SOLUTA_H
#define MMGR_CUSTODIA_SOLUTA_H

#include "carceribus/carceribus.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Arguments for the soluta calls; each reads only what it needs.
 *
 * @note init and release read pool and bytes; used reads pool.
 * @note The member list matches SecuraCfg.
 */
typedef struct
{
    CarcerCtx *const pool; /**< Pool the tenancy is taken from [BORROWS]. */
    void *const at;        /**< Start of the tenancy [BORROWS]. */
    const size_t bytes;    /**< Extent of the tenancy. */
} SolutaCfg;

/**
 * @brief Type of the soluta dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the three members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note The release member calls mmgr_soluta_reddo; the two names differ.
 */
typedef struct
{
    void *(*init)(const SolutaCfg *c);   /**< Takes bytes from the persistent end. */
    void (*release)(const SolutaCfg *c); /**< Gives the bytes back, unwiped. */
    size_t (*used)(const SolutaCfg *c);  /**< Bytes taken from the persistent end. */
} CustodiaSolutaNs;
MMGR_NS_LAYOUT(CustodiaSolutaNs, init, release, used);

/**
 * @brief Takes c->bytes from the pool's persistent end.
 *
 * @param[in,out] c Pool and the extent wanted [BORROWS].
 * @return          Start of the tenancy [BORROWS].
 * @warning c->bytes must not exceed the value mmgr_carcer_octas_praesto reports for the pool.
 */
void *mmgr_soluta_init(const SolutaCfg *c);

/**
 * @brief Gives c->bytes back to the pool's persistent end.
 *
 * @param[in,out] c Pool and the extent to release [BORROWS].
 * @note Releases without wiping; mmgr_secura_reddo wipes first.
 * @warning carcer.persist_reddo only subtracts from persist_end, so releases unwind in reverse order.
 */
void mmgr_soluta_reddo(const SolutaCfg *c);

/**
 * @brief Returns the bytes currently taken from the pool's persistent end.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      Bytes taken, across every tenancy in that pool.
 * @note Writes nothing.
 */
size_t mmgr_soluta_used(const SolutaCfg *c);

/**
 * @brief Dispatch table instance named soluta; release calls mmgr_soluta_reddo, the rest match by name.
 */
MMGR_NS CustodiaSolutaNs soluta MMGR_UNUSED = {
    .init = mmgr_soluta_init,
    .release = mmgr_soluta_reddo,
    .used = mmgr_soluta_used,
};

MMGR_FINIS_DECLS

#endif
