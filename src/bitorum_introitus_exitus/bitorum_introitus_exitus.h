/**
 * @brief Bit writer state, its arguments, and the bitio dispatch table.
 */
#ifndef MMGR_BITORUM_INTROITUS_EXITUS_H
#define MMGR_BITORUM_INTROITUS_EXITUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Bit writer state: the buffer, how much is written, and the partial byte.
 *
 * @note Built by mmgr_bitor_init and advanced by mmgr_bitor_put.
 */
typedef struct
{
    uint8_t *out;       /**< Destination buffer [BORROWS]. */
    size_t cap;         /**< Bytes available in out. */
    size_t cnt;         /**< Whole bytes written so far. */
    uint8_t residue;    /**< Bits not yet written, in the low nbits positions. */
    mmgr_word nbits;    /**< Bits held in residue, always under 8. */
    mmgr_bool overflow; /**< Set once a request would pass cap; blocks later writes. */
} mmgr_bitor;

/**
 * @brief Arguments for mmgr_bitor_init and mmgr_bitor_put; each reads only what it needs.
 *
 * @note mmgr_bitor_init reads out and cap; mmgr_bitor_put reads writer, val and nbits.
 */
typedef struct
{
    mmgr_bitor *const writer; /**< Writer for mmgr_bitor_put [BORROWS]. */
    uint8_t *const out;       /**< Buffer for mmgr_bitor_init [BORROWS]. */
    const size_t cap;         /**< Bytes available in out. */
    const uint64_t val;       /**< Bits for mmgr_bitor_put, taken from the low end. */
    const mmgr_word nbits;    /**< Number of bits of val to write, at most 64. */
} BitorumCfg;

/**
 * @brief Type of the bitio dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts init is at offset 0 and put at MMGR_FP_SIZE, with nothing else present.
 */
typedef struct
{
    mmgr_bitor (*init)(const BitorumCfg *c); /**< Set to mmgr_bitor_init. */
    void (*put)(const BitorumCfg *c);        /**< Set to mmgr_bitor_put. */
} BitorumIntroitusExitusNs;
MMGR_NS_LAYOUT(BitorumIntroitusExitusNs, init, put);

/**
 * @brief Builds a bit writer over c->out with capacity c->cap.
 *
 * @param[in] c Buffer and capacity [BORROWS].
 * @return      A writer with no bytes written and no residue.
 * @note The returned writer keeps c->out, which must outlive it [BORROWS].
 * @warning c->out must not be null and c->cap must not be zero.
 */
mmgr_bitor mmgr_bitor_init(const BitorumCfg *c);

/**
 * @brief Appends the low c->nbits bits of c->val to c->writer.
 *
 * @param[in,out] c Writer, value and bit count [BORROWS].
 * @note Writes whole bytes only; leftover bits stay in the writer's residue.
 * @note Does nothing when the writer's overflow is already set.
 * @note Sets the writer's overflow and clears its residue when the bytes would pass its cap.
 * @warning c->nbits must not exceed 64.
 */
void mmgr_bitor_put(const BitorumCfg *c);

/**
 * @brief Dispatch table instance named bitio; init calls mmgr_bitor_init, put calls mmgr_bitor_put.
 */
MMGR_NS BitorumIntroitusExitusNs bitio MMGR_UNUSED = {
    .init = mmgr_bitor_init,
    .put = mmgr_bitor_put,
};

MMGR_FINIS_DECLS

#endif
