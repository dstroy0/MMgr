/**
 * @brief Persistent tenancy that is zeroed before it is given back.
 */
#include "custodia_secura/custodia_secura.h"

/**
 * @brief Arguments for the secura backends.
 *
 * @note Mirrors SecuraCfg without its const qualifiers.
 */
typedef struct
{
    CarcerCtx *pool; /**< Pool the tenancy is taken from [BORROWS]. */
    void *at;        /**< Bytes to wipe [BORROWS]. */
    size_t bytes;    /**< Extent of the tenancy. */
} SecuraCtx;

/**
 * @brief Writes zeros over c->bytes at c->at.
 *
 * @param[in,out] c Address and extent to clear [BORROWS].
 * @note Clears sixteen words a pass, then the remaining word count bit by bit.
 * @warning c->at is read as uintptr_t, so it must carry that alignment.
 * @warning Clears whole words only; c->bytes is expected to be a whole number of them.
 */
MMGR_INLINE void secura_wipe(SecuraCtx *c)
{
    // Explicit cast reads the tenancy as words so a pass clears a register at a time
    uintptr_t *w = (uintptr_t *)c->at;
    size_t words = c->bytes / sizeof(uintptr_t);
    const uintptr_t z = 0u;

    while (words >= 16u)
    {
        w[0] = z;
        w[1] = z;
        w[2] = z;
        w[3] = z;
        w[4] = z;
        w[5] = z;
        w[6] = z;
        w[7] = z;
        w[8] = z;
        w[9] = z;
        w[10] = z;
        w[11] = z;
        w[12] = z;
        w[13] = z;
        w[14] = z;
        w[15] = z;
        w += 16;
        words -= 16u;
    }
    if (words & 8u)
    {
        w[0] = z;
        w[1] = z;
        w[2] = z;
        w[3] = z;
        w[4] = z;
        w[5] = z;
        w[6] = z;
        w[7] = z;
        w += 8;
    }
    if (words & 4u)
    {
        w[0] = z;
        w[1] = z;
        w[2] = z;
        w[3] = z;
        w += 4;
    }
    if (words & 2u)
    {
        w[0] = z;
        w[1] = z;
        w += 2;
    }
    if (words & 1u)
    {
        w[0] = z;
    }
}

/**
 * @brief Takes c->bytes from the persistent end of the pool.
 *
 * @param[in,out] c Pool and the extent wanted [BORROWS].
 * @return          Start of the tenancy [BORROWS].
 */
MMGR_INLINE void *secura_init(SecuraCtx *c)
{
    return MMGR_CALL(carcer.persist_capio, CarcerCfg, .pool = c->pool, .size = c->bytes);
}

/**
 * @brief Zeroes the tenancy, then gives its bytes back to the pool.
 *
 * @param[in,out] c Pool, the address to wipe and the extent [BORROWS].
 * @note Calls secura_wipe before carcer.persist_reddo.
 */
MMGR_INLINE void secura_reddo(SecuraCtx *c)
{
    secura_wipe(c);
    MMGR_CALL(carcer.persist_reddo, CarcerCfg, .pool = c->pool, .size = c->bytes);
}

/**
 * @brief Returns the bytes taken from the pool's persistent end.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      The pool's persist_used figure.
 */
MMGR_INLINE size_t secura_used(SecuraCtx *c)
{
    return MMGR_CALL(carcer.persist_used, CarcerCfg, .pool = c->pool);
}

/**
 * @brief Takes c->bytes from the pool's persistent end.
 *
 * @note Documented at the declaration in custodia_secura.h.
 */
void *mmgr_secura_init(const SecuraCfg *c)
{
    return MMGR_CALL(secura_init, SecuraCtx, .pool = c->pool, .bytes = c->bytes);
}

/**
 * @brief Zeroes the tenancy and gives its bytes back to the pool.
 *
 * @note Documented at the declaration in custodia_secura.h.
 */
void mmgr_secura_reddo(const SecuraCfg *c)
{
    MMGR_CALL(secura_reddo, SecuraCtx, .pool = c->pool, .at = c->at, .bytes = c->bytes);
}

/**
 * @brief Returns the bytes taken from the pool's persistent end.
 *
 * @note Documented at the declaration in custodia_secura.h.
 */
size_t mmgr_secura_used(const SecuraCfg *c)
{
    return MMGR_CALL(secura_used, SecuraCtx, .pool = c->pool);
}

/**
 * @brief Zeroes the tenancy without releasing it.
 *
 * @note secura_wipe reads at and bytes only.
 * @note Documented at the declaration in custodia_secura.h.
 */
void mmgr_secura_wipe(const SecuraCfg *c)
{
    MMGR_CALL(secura_wipe, SecuraCtx, .pool = c->pool, .at = c->at, .bytes = c->bytes);
}
