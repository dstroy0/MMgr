/**
 * @brief Writes and reads one to eight bytes, with the byte order reversed from the target's own.
 *
 * @note On a little endian target that puts the most significant byte first; endian_rev reverses either way.
 * @note Both calls move eight bytes at c->at or c->from whatever c->bytes says, and both need that address aligned.
 */
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

#include "endian/endian.h"
#include "proximus_operor/proximus_operor.h"

/**
 * @brief Arguments for the byte write.
 *
 * @note Mirrors the OctetusCfg members put reads, without their const qualifiers.
 * @warning bytes must be 1 through 8; 8u - bytes is unsigned, and a shift count of 64 is undefined.
 */
typedef struct
{
    uint8_t *at;  /**< Destination [BORROWS]. */
    uint64_t val; /**< Value whose low bytes are written. */
    size_t bytes; /**< Bytes of val to place, 1 through 8. */
} OctetPutCtx;

/**
 * @brief Arguments for the byte read.
 *
 * @note Mirrors the OctetusCfg members take reads, without their top-level const qualifiers.
 */
typedef struct
{
    const uint8_t *from; /**< Source [BORROWS]. */
    uint64_t *out;       /**< Where the value read is stored [BORROWS]. */
    size_t bytes;        /**< Bytes to take from from, 1 through 8. */
} OctetTakeCtx;

/**
 * @brief Places the low c->bytes bytes of c->val at c->at, in reversed byte order.
 *
 * @param[in] c Destination, value and count [BORROWS].
 * @note Shifts the wanted bytes to the top of a 64-bit word, reverses all eight, then stores all eight.
 * @note The 8 - c->bytes bytes past the value come out zero, since the shift brought zeros in below it.
 * @warning proxim.al_put64 stores eight bytes, so c->at must be writable for eight and aligned for a uint64_t.
 */
MMGR_INLINE void octet_put(const OctetPutCtx *c)
{
    const uint64_t v = c->val << (8u * (8u - c->bytes));

    MMGR_CALL(proxim.al_put64, ProximusCfg, .dst = c->at,
              .val = MMGR_CALL(magna_extremitas.rev, EndianCfg, .val = v, .width = MMGR_ENDIAN_64));
}

/**
 * @brief Reads c->bytes from c->from in reversed byte order and stores the value in *c->out.
 *
 * @param[in] c Source, destination for the value, and count [BORROWS].
 * @note Loads eight bytes, reverses all eight, then shifts the 8 - c->bytes past the value out of the result.
 * @warning proxim.al_load64 reads eight bytes, so c->from must be readable for eight and aligned for a uint64_t.
 * @warning The bytes past c->bytes take part in the load, though the shift drops them from the result.
 */
MMGR_INLINE void octet_take(const OctetTakeCtx *c)
{
    const uint64_t v = MMGR_CALL(proxim.al_load64, ProximusCfg, .at = c->from);

    // Explicit cast narrows the count to the packed mmgr_endian_width EndianCfg::width is declared with,
    // which admits any count, not only the 2, 4 and 8 the enum names
    *c->out = MMGR_CALL(magna_extremitas.rev, EndianCfg, .val = v, .width = (mmgr_endian_width)c->bytes);
}

/**
 * @brief Places the low c->bytes bytes of c->val at c->at, in reversed byte order.
 *
 * @note Documented at the declaration in octetus_introitus_exitus.h.
 */
void mmgr_octet_put(const OctetusCfg *c)
{
    MMGR_CALL(octet_put, OctetPutCtx, .at = c->at, .val = c->val, .bytes = c->bytes);
}

/**
 * @brief Reads c->bytes from c->from in reversed byte order and stores the value in *c->out.
 *
 * @note Documented at the declaration in octetus_introitus_exitus.h.
 */
void mmgr_octet_take(const OctetusCfg *c)
{
    MMGR_CALL(octet_take, OctetTakeCtx, .from = c->from, .out = c->out, .bytes = c->bytes);
}
