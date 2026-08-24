/**
 * @brief Persistent tenancy that is released without being wiped.
 */
#include "custodia_soluta/custodia_soluta.h"

/**
 * @brief Arguments for the soluta backends.
 *
 * @note Mirrors SolutaCfg without its const qualifiers.
 */
typedef struct
{
    CarcerCtx *pool; /**< Pool the tenancy is taken from [BORROWS]. */
    void *at;        /**< Start of the tenancy [BORROWS]. */
    size_t bytes;    /**< Extent of the tenancy. */
} SolutaCtx;

/**
 * @brief Takes c->bytes from the persistent end of the pool.
 *
 * @param[in,out] c Pool and the extent wanted [BORROWS].
 * @return          Start of the tenancy [BORROWS].
 */
MMGR_INLINE void *soluta_init(SolutaCtx *c)
{
    return MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = c->pool, .size = c->bytes);
}

/**
 * @brief Gives c->bytes back to the persistent end of the pool.
 *
 * @param[in,out] c Pool and the extent to release [BORROWS].
 * @note Calls carcer.persist_reddo directly, where secura_reddo wipes first.
 */
MMGR_INLINE void soluta_reddo(SolutaCtx *c)
{
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = c->pool, .size = c->bytes);
}

/**
 * @brief Returns the bytes taken from the pool's persistent end.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      The pool's persist_used figure.
 */
MMGR_INLINE size_t soluta_used(SolutaCtx *c)
{
    return MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = c->pool);
}

/**
 * @brief Takes c->bytes from the pool's persistent end.
 *
 * @note Documented at the declaration in custodia_soluta.h.
 */
void *mmgr_soluta_init(const SolutaCfg *c)
{
    return MMGR_CALL(soluta_init, SolutaCtx, .pool = c->pool, .bytes = c->bytes);
}

/**
 * @brief Gives c->bytes back to the pool's persistent end.
 *
 * @note Documented at the declaration in custodia_soluta.h.
 */
void mmgr_soluta_reddo(const SolutaCfg *c)
{
    MMGR_CALL(soluta_reddo, SolutaCtx, .pool = c->pool, .bytes = c->bytes);
}

/**
 * @brief Returns the bytes taken from the pool's persistent end.
 *
 * @note Documented at the declaration in custodia_soluta.h.
 */
size_t mmgr_soluta_used(const SolutaCfg *c)
{
    return MMGR_CALL(soluta_used, SolutaCtx, .pool = c->pool);
}
