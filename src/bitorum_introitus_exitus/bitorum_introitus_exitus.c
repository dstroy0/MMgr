/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Bit writer packing least significant bits first into a caller-supplied buffer.
 *
 * @note Two entries do the work and they are not interchangeable. bitor_put writes whole bytes and
 *       nothing else, so bits that do not fill one stay in the residue; bitor_align is what puts that
 *       last partial byte out. A stream whose length is not a multiple of eight and that never calls
 *       align ends one byte short, with no flag raised and nothing to notice at the call.
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
    uint8_t *out;       /**< Buffer bitor_init builds a writer over [BORROWS]. */
    size_t cap;         /**< Bytes available in out. */
    uint64_t val;       /**< Bits to write, taken from the low end. */
    mmgr_word nbits;    /**< Number of bits of val to write. */
} BitorCtx;

/**
 * @brief Appends the low args->nbits bits of args->val to args->writer.
 *
 * @param[in,out] args Writer, value and bit count [BORROWS].
 * @note Writes whole bytes only; leftover bits stay in writer->residue.
 * @note Does nothing when writer->overflow is already set.
 * @note Sets writer->overflow and clears the residue when the bytes would pass writer->cap.
 * @warning args->nbits must not exceed 64.
 */
MMGR_INLINE void bitor_put(const BitorCtx *args)
{
    mmgr_bitor *const writer = args->writer;

    if (writer->overflow)
    {
        return;
    }

    MMGR_ASSERT(writer->cnt <= writer->cap, "the count of written bytes has passed the capacity");
    MMGR_ASSERT(writer->nbits < 8u, "a whole byte was left in the residue instead of being written");
    MMGR_ASSERT(args->nbits <= 64u, "a put of more bits than a uint64_t holds");

    // A request of exactly 64 takes the all-ones mask, since a shift by the full width is undefined
    const uint64_t mask = (args->nbits >= 64u) ? ~(uint64_t)0 : ((UINT64_C(1) << args->nbits) - 1u);
    // Explicit cast narrows the combined residue and request bits, in whole bytes, to size_t
    const size_t whole = (size_t)((writer->nbits + args->nbits) / 8u);
    uint64_t work = args->val & mask;
    mmgr_word left = args->nbits;

    if (whole > (writer->cap - writer->cnt))
    {
        writer->overflow = MMGR_TRUE;
        writer->nbits = 0;
        writer->residue = 0;
        return;
    }

    uint8_t *const to = writer->out + writer->cnt;

    if (whole != 0u)
    {
        const mmgr_word take = 8u - writer->nbits;
        const uint8_t chunk = (uint8_t)(work & 0xFFu);

        // Explicit casts hold each step at uint8_t; the shift and the or promote away from it
        to[0] = (uint8_t)(writer->residue | (uint8_t)(chunk << writer->nbits));
        work >>= take;
        left -= take;
        writer->residue = 0;
        writer->nbits = 0;

        // The first byte is the only one the residue reaches: it is cleared just above and nothing
        // refills it before the loop ends, so every byte after it is a mask and a store with no
        // merge and no room to work out. Written as one loop it recomputed both every pass, which
        // measured 336 cycles against 121 for sixty four bits on an ESP32-S3.
        for (size_t i = 1u; i < whole; i++)
        {
            to[i] = (uint8_t)(work & 0xFFu);
            work >>= 8u;
            left -= 8u;
        }
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
 * @brief Writes the partial byte args->writer still holds, padded with zeros above its bits.
 *
 * @param[in,out] args Writer to finish [BORROWS].
 * @note The residue holds its bits in the low nbits positions with zeros above, so no padding is added.
 * @note Does nothing when the residue is empty, so it is safe to call at the end of any stream.
 */
MMGR_INLINE void bitor_align(const BitorCtx *args)
{
    mmgr_bitor *const writer = args->writer;

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
 * @brief Fills an mmgr_bitor from args->out and args->cap, with the counters zeroed.
 *
 * @param[in] args Buffer out and its extent cap [BORROWS].
 * @return      A writer with no bytes written and no residue.
 * @note The returned writer keeps args->out, which must outlive it [BORROWS].
 * @warning args->out must not be null and args->cap must not be zero.
 */
MMGR_INLINE mmgr_bitor bitor_init(const BitorCtx *args)
{
    MMGR_ASSERT(args->out != NULL, "a bit writer needs a buffer");
    MMGR_ASSERT(args->cap != 0, "a bit writer needs a capacity");

    mmgr_bitor writer;
    writer.out = args->out;
    writer.cap = args->cap;
    writer.cnt = 0;
    writer.residue = 0;
    writer.nbits = 0;
    writer.overflow = MMGR_FALSE;
    return writer;
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_bitor_ and bitor_ prefixes, which the two share.
 */
#define BITOR_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_bitor_, bitor_, BitorCtx, BitorumCfg, ret, name, __VA_ARGS__)

/**
 * @brief Binds the same four to GENERIC_ENTRY_V, for an entry that returns nothing.
 *
 * @param[in] name Name after the mmgr_bitor_ and bitor_ prefixes, which the two share.
 */
#define BITOR_ENTRY_V(name, ...) GENERIC_ENTRY_V(mmgr_bitor_, bitor_, BitorCtx, BitorumCfg, name, __VA_ARGS__)

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in bitorum_introitus_exitus.h.
 * @note The fields each line forwards are the ones that entry reads; MMGR_CALL zeroes the rest.
 */
BITOR_ENTRY(mmgr_bitor, init, .out = args->out, .cap = args->cap)
BITOR_ENTRY_V(put, .writer = args->writer, .val = args->val, .nbits = args->nbits)
BITOR_ENTRY_V(align, .writer = args->writer)
