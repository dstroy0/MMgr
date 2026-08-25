/**
 * @brief Bit writer packing least significant bits first into a caller-supplied buffer.
 */
#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"

/**
 * @brief Arguments to bitor_put: the writer, the value and the bit count.
 *
 * @note mmgr_bitor_put copies these three members out of a BitorumCfg.
 */
typedef struct
{
    mmgr_bitor *writer; /**< Writer to append to [BORROWS]. */
    uint64_t val;       /**< Bits to write, taken from the low end. */
    mmgr_word nbits;    /**< Number of bits of val to write. */
} BitorCtx;

/**
 * @brief Appends the low c->nbits bits of c->val to c->writer.
 *
 * @param[in,out] c Writer, value and bit count [BORROWS].
 * @note Writes whole bytes only; leftover bits stay in writer->residue.
 * @note Does nothing when writer->overflow is already set.
 * @note Sets writer->overflow and clears the residue when the bytes would pass writer->cap.
 * @warning c->nbits must not exceed 64.
 */
MMGR_INLINE void bitor_put(const BitorCtx *c)
{
    mmgr_bitor *const writer = c->writer;

    if (writer->overflow)
    {
        return;
    }

    MMGR_ASSERT(writer->cnt <= writer->cap, "the count of written bytes has passed the capacity");
    MMGR_ASSERT(writer->nbits < 8u, "a whole byte was left in the residue instead of being written");

    // A request of 64 bits or more takes the all-ones mask, avoiding a shift by the full width
    const uint64_t mask = (c->nbits >= 64u) ? ~(uint64_t)0 : ((UINT64_C(1) << c->nbits) - 1u);
    // Explicit cast narrows the combined residue and request bits, in whole bytes, to size_t
    const size_t whole = (size_t)((writer->nbits + c->nbits) / 8u);
    uint64_t work = c->val & mask;
    mmgr_word left = c->nbits;

    if (whole > (writer->cap - writer->cnt))
    {
        writer->overflow = MMGR_TRUE;
        writer->nbits = 0;
        writer->residue = 0;
        return;
    }

    for (size_t i = 0; i < whole; i++)
    {
        // take is 8 on every pass after the first, since the residue is cleared each time
        const mmgr_word take = 8u - writer->nbits;
        const uint8_t chunk = (uint8_t)(work & 0xFFu);

        // Explicit casts hold the byte assembly in uint8_t, discarding the int promotion from <<
        writer->out[writer->cnt + i] = (uint8_t)(writer->residue | (uint8_t)(chunk << writer->nbits));
        work >>= take;
        left -= take;
        writer->residue = 0;
        writer->nbits = 0;
    }

    if (left != 0u)
    {
        // Explicit casts narrow the leftover bits into the uint8_t residue; left plus writer->nbits is under 8
        const uint8_t tail = (uint8_t)(work & ((1u << left) - 1u));

        writer->residue = (uint8_t)(writer->residue | (uint8_t)(tail << writer->nbits));
        writer->nbits += left;
    }
    writer->cnt += whole;
}

/**
 * @brief Writes the partial byte c->writer still holds, padded with zeros above its bits.
 *
 * @param[in,out] c Writer to finish [BORROWS].
 * @note The residue already carries its bits in the low nbits positions with zeros above, so the
 *       padding is what is there rather than anything this has to add.
 * @note Does nothing when the residue is empty, which is what makes it safe to end every stream with
 *       whether or not the last put happened to land on a byte.
 */
MMGR_INLINE void bitor_align(const BitorCtx *c)
{
    mmgr_bitor *const writer = c->writer;

    if (writer->overflow || (writer->nbits == 0u))
    {
        return;
    }
    if (writer->cnt >= writer->cap)
    {
        writer->overflow = MMGR_TRUE;
        return;
    }
    writer->out[writer->cnt] = writer->residue;
    writer->cnt++;
    writer->residue = 0;
    writer->nbits = 0;
}

/**
 * @brief Fills an mmgr_bitor from c->out and c->cap, with the counters zeroed.
 *
 * @note Documented at the declaration in bitorum_introitus_exitus.h.
 */
mmgr_bitor mmgr_bitor_init(const BitorumCfg *c)
{
    MMGR_ASSERT(c->out != NULL, "a bit writer needs a buffer");
    MMGR_ASSERT(c->cap != 0, "a bit writer needs a capacity");

    mmgr_bitor writer;
    writer.out = c->out;
    writer.cap = c->cap;
    writer.cnt = 0;
    writer.residue = 0;
    writer.nbits = 0;
    writer.overflow = MMGR_FALSE;
    return writer;
}

/**
 * @brief Copies c->writer, c->val and c->nbits into a BitorCtx and calls bitor_put.
 *
 * @note c->out and c->cap are not read; they belong to mmgr_bitor_init.
 * @note Documented at the declaration in bitorum_introitus_exitus.h.
 */
void mmgr_bitor_put(const BitorumCfg *c)
{
    MMGR_CALL(bitor_put, BitorCtx, .writer = c->writer, .val = c->val, .nbits = c->nbits);
}

/**
 * @brief Copies c->writer into a BitorCtx and calls bitor_align.
 *
 * @note c->val and c->nbits are not read; a flush has nothing to be given.
 * @note Documented at the declaration in bitorum_introitus_exitus.h.
 */
void mmgr_bitor_align(const BitorumCfg *c)
{
    MMGR_CALL(bitor_align, BitorCtx, .writer = c->writer);
}
