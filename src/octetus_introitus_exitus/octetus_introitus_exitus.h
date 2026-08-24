/**
 * @brief Fixed-width byte moves: the arguments, the two calls, and the byteio dispatch table.
 *
 * @note Both calls reverse the byte order from the target's own, which is big endian on a little endian target.
 */
#ifndef MMGR_OCTETUS_INTROITUS_EXITUS_H
#define MMGR_OCTETUS_INTROITUS_EXITUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Arguments for the two byteio calls.
 *
 * @note put reads at, val and bytes; take reads from, out and bytes.
 */
typedef struct
{
    uint8_t *const at;         /**< Destination for put [BORROWS]. */
    const uint8_t *const from; /**< Source for take [BORROWS]. */
    uint64_t *const out;       /**< Where take stores the value it read [BORROWS]. */
    const uint64_t val;        /**< Value put writes, taken from its low bytes. */
    const size_t bytes;        /**< Bytes the call moves, 1 through 8. */
} OctetusCfg;

/**
 * @brief Type of the byteio dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the two members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 */
typedef struct
{
    void (*put)(const OctetusCfg *c);  /**< Writes bytes of a value to memory. */
    void (*take)(const OctetusCfg *c); /**< Reads bytes of a value from memory. */
} OctetusIntroitusExitusNs;
MMGR_NS_LAYOUT(OctetusIntroitusExitusNs, put, take);

/**
 * @brief Places the low c->bytes bytes of c->val at c->at, in reversed byte order.
 *
 * @param[in] c Destination, value and count [BORROWS].
 * @note The 8 - c->bytes bytes after the value are written as zeros.
 * @warning Stores eight bytes, so c->at must be writable for eight and aligned for a uint64_t.
 * @warning c->bytes must be 1 through 8.
 */
void mmgr_octet_put(const OctetusCfg *c);

/**
 * @brief Reads c->bytes from c->from in reversed byte order and stores the value in *c->out.
 *
 * @param[in] c Source, destination for the value, and count [BORROWS].
 * @note The value lands in the low c->bytes of *c->out, with the bytes above it zero.
 * @warning Reads eight bytes, so c->from must be readable for eight and aligned for a uint64_t.
 * @warning c->bytes must be 1 through 8.
 */
void mmgr_octet_take(const OctetusCfg *c);

/**
 * @brief Dispatch table instance named byteio; each member calls the matching mmgr_octet_ function.
 */
MMGR_NS OctetusIntroitusExitusNs byteio MMGR_UNUSED = {
    .put = mmgr_octet_put,
    .take = mmgr_octet_take,
};

MMGR_FINIS_DECLS

#endif
