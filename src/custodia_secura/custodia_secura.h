/**
 * @brief Persistent tenancy that is zeroed before release: its arguments and the secura table.
 */
#ifndef MMGR_CUSTODIA_SECURA_H
#define MMGR_CUSTODIA_SECURA_H

#include "carceribus/carceribus.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Arguments for the secura calls; each reads only what it needs.
 *
 * @note init reads pool and bytes; used reads pool; release reads all three; wipe reads at and bytes.
 */
typedef struct
{
    CarcerCtx *const pool; /**< Pool the tenancy is taken from [BORROWS]. */
    void *const at;        /**< Start of the tenancy to wipe [BORROWS]. */
    const size_t bytes;    /**< Extent of the tenancy. */
} SecuraCfg;

/**
 * @brief Type of the secura dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the four members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note The release member calls mmgr_secura_reddo; the two names differ.
 */
typedef struct
{
    void *(*init)(const SecuraCfg *c);    /**< Takes bytes from the persistent end. */
    void (*release)(const SecuraCfg *c);  /**< Wipes, then gives the bytes back. */
    size_t (*used)(const SecuraCfg *c);   /**< Bytes taken from the persistent end. */
    void (*wipe)(const SecuraCfg *c);     /**< Wipes without releasing. */
} CustodiaSecuraNs;
MMGR_NS_LAYOUT(CustodiaSecuraNs, init, release, used, wipe);

/**
 * @brief Takes c->bytes from the pool's persistent end.
 *
 * @param[in,out] c Pool and the extent wanted [BORROWS].
 * @return          Start of the tenancy [BORROWS].
 * @warning c->bytes must not exceed the value mmgr_carcer_octas_praesto reports for the pool.
 */
void *mmgr_secura_init(const SecuraCfg *c);

/**
 * @brief Zeroes the tenancy at c->at, then gives its bytes back to the pool.
 *
 * @param[in,out] c Pool, the tenancy address and its extent [BORROWS].
 * @note The wipe happens before the pool release.
 * @warning carcer.persist_reddo only subtracts from persist_end, so releases unwind in reverse order.
 */
void mmgr_secura_reddo(const SecuraCfg *c);

/**
 * @brief Returns the bytes currently taken from the pool's persistent end.
 *
 * @param[in] c Pool to read [BORROWS].
 * @return      Bytes taken, across every tenancy in that pool.
 */
size_t mmgr_secura_used(const SecuraCfg *c);

/**
 * @brief Writes zeros over c->bytes at c->at, leaving the tenancy in place.
 *
 * @param[in,out] c The address and extent to clear [BORROWS].
 * @warning c->at must be aligned for uintptr_t.
 * @warning Clears whole uintptr_t words only; c->bytes is expected to be a whole number of them.
 */
void mmgr_secura_wipe(const SecuraCfg *c);

/**
 * @brief Dispatch table instance named secura; release calls mmgr_secura_reddo, the rest match by name.
 */
MMGR_NS CustodiaSecuraNs secura MMGR_UNUSED = {
    .init = mmgr_secura_init,
    .release = mmgr_secura_reddo,
    .used = mmgr_secura_used,
    .wipe = mmgr_secura_wipe,
};

MMGR_FINIS_DECLS

#endif
