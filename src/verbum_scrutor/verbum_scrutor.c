/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief SWAR tests over the bytes of one mmgr_word, treating each byte as a lane.
 *
 * @note A lane mask carries one set bit per matching lane, in that lane's high bit, and zero elsewhere.
 * @note scrut_spread widens such a mask to full 0xFF lanes when whole bytes are wanted instead.
 * @note The lane index calls count set high bits, so they report positions rather than masks.
 */
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @brief Arguments for the lane backends.
 *
 * @note Mirrors ScrutLaneCfg without its const qualifiers.
 * @note The comparison calls read word and byte; the index calls read mask alone.
 */
typedef struct
{
    mmgr_word word; /**< The eight or four bytes under test, one per lane. */
    mmgr_word val;  /**< Whole word scrut_xor compares against, already broadcast. */
    mmgr_word mask; /**< Lane mask the three index calls count. */
    uint8_t byte;   /**< Byte broadcast into every lane before the comparison. */
    uint8_t fam;    /**< Bits scrut_fam_eq keeps before comparing. */
    mmgr_bool ci;   /**< Non-zero to ignore case on alphabetic lanes. */
} ScrutLaneCtx;

/**
 * @brief Arguments for the mask backends.
 *
 * @note Mirrors ScrutMaskCfg without its const qualifiers.
 * @note spread, drop_lo, drop_hi and lanes_before read mask; bytes_below and lanes_below read bytes.
 * @note tail_mask is the only backend that reads wi.
 */
typedef struct
{
    mmgr_word mask; /**< Lane mask to reshape. */
    size_t bytes;   /**< Byte count the mask is built from, or the run length wanted. */
    size_t wi;      /**< Index of the word already reached, in whole words. */
} ScrutMaskCtx;

/**
 * @brief Arguments for the word backends.
 *
 * @note Mirrors ScrutWordCfg without its const qualifiers.
 * @note The two loads read at, fold_lower reads word, and words reads bytes.
 */
typedef struct
{
    mmgr_word word;  /**< Word to fold. */
    const void *at;  /**< Address to load a word from [BORROWS]. */
    size_t bytes;    /**< Byte count to convert into a word count. */
} ScrutWordCtx;

/**
 * @brief Returns the lanes that sit below the lowest set lane of c->mask.
 *
 * @param[in] c The lane mask to examine [BORROWS].
 * @return      A lane mask holding those lanes and nothing else.
 * @note Subtracting one sets every bit below the lowest set one, and ~c->mask drops that lowest one again.
 * @note An empty c->mask gives every lane, which is what makes scrut_lane_lo report MMGR_SWAR_BYTES.
 */
MMGR_INLINE mmgr_word scrut_below_lo(const ScrutMaskCtx *c)
{
    return (c->mask - 1u) & ~c->mask & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief Carries every set bit of c->mask down through all the lanes below it.
 *
 * @param[in] c The lane mask to smear [BORROWS].
 * @return      A mask set from the highest set lane down to lane zero.
 * @note Doubles the shift each pass, so it covers the whole word in three passes at 64 bits.
 * @note Smears downward only, so the highest set lane is what the result reaches up to.
 */
MMGR_INLINE mmgr_word scrut_smear(const ScrutMaskCtx *c)
{
    mmgr_word m = c->mask;

    for (uint32_t k = 8u; k < MMGR_SWAR_LANE_BITS; k <<= 1)
    {
        m |= (m >> k);
    }
    return m;
}

/**
 * @brief Marks the lanes of c->word that are at or above c->byte.
 *
 * @param[in] c The word and the byte to compare against [BORROWS].
 * @return      A lane mask holding those lanes.
 * @note Setting each lane's high bit before the subtraction gives it a borrow to spend, so lanes stay separate.
 * @note A lane at or above c->byte keeps that high bit, and one below it borrows the bit away.
 * @note Compares as unsigned bytes, so 0x80 and above are the largest values.
 */
MMGR_INLINE mmgr_word scrut_ge(const ScrutLaneCtx *c)
{
    return ((c->word | MMGR_VERBUM_SCRUTOR_HIGH) - MMGR_SWAR_ONES * c->byte) & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief Marks the lanes of c->word that are at or below c->byte.
 *
 * @param[in] c The word and the byte to compare against [BORROWS].
 * @return      A lane mask holding those lanes.
 * @note Subtracts the word from the broadcast byte, which is scrut_ge with the two operands swapped.
 * @note Compares as unsigned bytes, so 0x80 and above are the largest values.
 */
MMGR_INLINE mmgr_word scrut_le(const ScrutLaneCtx *c)
{
    return ((MMGR_SWAR_ONES * c->byte | MMGR_VERBUM_SCRUTOR_HIGH) - c->word) & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief Subtracts c->byte from every lane of c->word and keeps the low seven bits of each result.
 *
 * @param[in] c The word and the byte to subtract [BORROWS].
 * @return      The seven low bits of each lane's difference, with every lane's high bit clear.
 * @note Runs the same subtraction as scrut_ge but keeps MMGR_SWAR_LOW7, so this yields values rather than a mask.
 * @note A lane below c->byte wraps, so its seven bits are the difference taken modulo 128.
 */
MMGR_INLINE mmgr_word scrut_sub7(const ScrutLaneCtx *c)
{
    return ((c->word | MMGR_VERBUM_SCRUTOR_HIGH) - MMGR_SWAR_ONES * c->byte) & MMGR_SWAR_LOW7;
}

/**
 * @brief Marks the lanes of c->word that hold zero.
 *
 * @param[in] c The word to test [BORROWS].
 * @return      A lane mask holding the zero lanes.
 * @note Adding MMGR_SWAR_LOW7 carries into a lane's high bit unless its low seven bits are all zero.
 * @note Or-ing c->word back in then covers the case where only the high bit was set.
 * @note scrut_eq and scrut_fam_eq both reach this after an exclusive or, which turns equality into a zero test.
 */
MMGR_INLINE mmgr_word scrut_has_zero(const ScrutLaneCtx *c)
{
    return ~(((c->word & MMGR_SWAR_LOW7) + MMGR_SWAR_LOW7) | c->word) & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief Marks the lanes of c->word holding an ASCII letter, of either case.
 *
 * @param[in] c The word to test [BORROWS].
 * @return      A lane mask holding the letter lanes.
 * @note Setting bit five in every lane folds the two cases together, so one range test covers both.
 * @note The final and with the complement keeps only lanes whose high bit is clear, so a byte at 0x80 or
 *       above cannot pass by folding into the letter range.
 * @note scrut_xor and scrut_fold_lower both shift this result down two places to reach the case bit.
 */
MMGR_INLINE mmgr_word scrut_alpha(const ScrutLaneCtx *c)
{
    const mmgr_word lo = c->word | (MMGR_SWAR_ONES * 0x20u);

    return MMGR_CALL(scrut_ge, ScrutLaneCtx, .word = lo, .byte = 'a') &
           MMGR_CALL(scrut_le, ScrutLaneCtx, .word = lo, .byte = 'z') & ~lo;
}

/**
 * @brief Returns the lane by lane difference of c->word and c->val, optionally ignoring case.
 *
 * @param[in] c The two words and the case flag [BORROWS].
 * @return      A word whose lanes are zero exactly where the two agreed.
 * @note With c->ci clear this is a plain exclusive or, and every bit of every lane counts.
 * @note With c->ci set, scrut_alpha shifted down two gives the case bit of each letter lane, and clearing
 *       it in the difference makes the two cases of a letter compare equal.
 * @note Only letter lanes are folded, so a difference in bit five of a digit or a symbol still counts.
 */
MMGR_INLINE mmgr_word scrut_xor(const ScrutLaneCtx *c)
{
    const mmgr_word x = c->word ^ c->val;

    if (!c->ci)
    {
        return x;
    }
    return x & ~(MMGR_CALL(scrut_alpha, ScrutLaneCtx, .word = c->word) >> 2);
}

/**
 * @brief Marks the lanes of c->word that equal c->byte.
 *
 * @param[in] c The word, the byte to find, and the case flag [BORROWS].
 * @return      A lane mask holding the matching lanes.
 * @note Broadcasts c->byte into every lane, takes the difference through scrut_xor, then tests for zero.
 * @note c->ci is passed on to scrut_xor, so a letter matches either case when it is set.
 */
MMGR_INLINE mmgr_word scrut_eq(const ScrutLaneCtx *c)
{
    const mmgr_word broadcast = MMGR_SWAR_ONES * c->byte;
    const mmgr_word x = MMGR_CALL(scrut_xor, ScrutLaneCtx, .word = c->word, .val = broadcast, .ci = c->ci);

    return MMGR_CALL(scrut_has_zero, ScrutLaneCtx, .word = x);
}

/**
 * @brief Marks the lanes of c->word that match c->byte once both are reduced to the c->fam bits.
 *
 * @param[in] c The word, the byte to match, and the bits that count [BORROWS].
 * @return      A lane mask holding the matching lanes.
 * @note c->byte is itself reduced by c->fam first, so bits outside the family take no part on either side.
 * @note A c->fam of 0xFF makes this a plain equality test, and a c->fam of 0 marks every lane.
 */
MMGR_INLINE mmgr_word scrut_fam_eq(const ScrutLaneCtx *c)
{
    const mmgr_word bits = MMGR_SWAR_ONES * c->fam;
    const mmgr_word want = MMGR_SWAR_ONES * (c->byte & c->fam);

    return MMGR_CALL(scrut_has_zero, ScrutLaneCtx, .word = (c->word & bits) ^ want);
}

/**
 * @brief Marks the lanes of c->word whose MMGR_FAM_CS bits equal MMGR_FAM_CI.
 *
 * @param[in] c The word to test [BORROWS].
 * @return      A lane mask holding those lanes.
 * @warning That covers the whole 0x40 to 0x5F block, so the at sign and the six symbols among the capitals
 *          are marked alongside A through Z.
 * @note Use scrut_alpha when only letters should count.
 */
MMGR_INLINE mmgr_word scrut_any_upper(const ScrutLaneCtx *c)
{
    return MMGR_CALL(scrut_fam_eq, ScrutLaneCtx, .word = c->word, .fam = MMGR_FAM_CS, .byte = MMGR_FAM_CI);
}

/**
 * @brief Marks the lanes of c->word whose top four bits are 0x30.
 *
 * @param[in] c The word to test [BORROWS].
 * @return      A lane mask holding those lanes.
 * @warning That covers the whole 0x30 to 0x3F block, so the seven symbols after the nine are marked as well.
 * @note A caller that needs only 0 through 9 can and this with scrut_le at the character nine.
 */
MMGR_INLINE mmgr_word scrut_any_digit(const ScrutLaneCtx *c)
{
    return MMGR_CALL(scrut_fam_eq, ScrutLaneCtx, .word = c->word, .fam = 0xF0u, .byte = 0x30u);
}

/**
 * @brief Counts the set lanes of c->mask.
 *
 * @param[in] c The lane mask to count [BORROWS].
 * @return      How many lanes are set, 0 through MMGR_SWAR_BYTES.
 * @note Shifting down seven puts each lane's bit at its own low position, and multiplying by MMGR_SWAR_ONES
 *       sums every one of them into the top lane, which the final shift then reads out.
 * @note Reads c->mask alone, so the word and byte members take no part.
 */
MMGR_INLINE size_t scrut_lane_count(const ScrutLaneCtx *c)
{

    return (mmgr_word)((c->mask >> 7) * MMGR_SWAR_ONES) >> (MMGR_SWAR_LANE_BITS - 8u);
}

/**
 * @brief Returns the index of the lowest set lane of c->mask.
 *
 * @param[in] c The lane mask to examine [BORROWS].
 * @return      The index, or MMGR_SWAR_BYTES when no lane is set.
 * @note Counts the lanes below the lowest set one, which is that lane's index.
 * @note An empty mask needs no guard here, since scrut_below_lo then reports every lane.
 * @note The lane table binds this to first on a little endian target and to last on a big endian one.
 */
MMGR_INLINE size_t scrut_lane_lo(const ScrutLaneCtx *c)
{
    return MMGR_CALL(scrut_lane_count, ScrutLaneCtx, .mask = MMGR_CALL(scrut_below_lo, ScrutMaskCtx, .mask = c->mask));
}

/**
 * @brief Returns the index of the highest set lane of c->mask.
 *
 * @param[in] c The lane mask to examine [BORROWS].
 * @return      The index, or MMGR_SWAR_BYTES when no lane is set.
 * @note Smearing down from the highest set lane and counting gives that lane's index once one is taken off.
 * @note The empty mask is caught first, since the smear of an empty mask would count zero and then wrap.
 * @note The lane table binds this to last on a little endian target and to first on a big endian one.
 */
MMGR_INLINE size_t scrut_lane_hi(const ScrutLaneCtx *c)
{
    if (c->mask == 0u)
    {
        return MMGR_SWAR_BYTES;
    }
    return MMGR_CALL(scrut_lane_count, ScrutLaneCtx, .mask = MMGR_CALL(scrut_smear, ScrutMaskCtx, .mask = c->mask)) - 1u;
}

/**
 * @brief Widens every set lane of c->mask from its high bit to a full byte of ones.
 *
 * @param[in] c The lane mask to widen [BORROWS].
 * @return      A word holding 0xFF in each set lane and 0x00 in the rest.
 * @note A set lane holds 0x80, and adding 0x7F to it fills the lane; a clear lane contributes nothing.
 * @note Use this when whole bytes are wanted, such as for selecting between two words lane by lane.
 */
MMGR_INLINE mmgr_word scrut_spread(const ScrutMaskCtx *c)
{
    return (mmgr_word)(c->mask + (c->mask - (c->mask >> 7)));
}

/**
 * @brief Clears the lowest set lane of c->mask.
 *
 * @param[in] c The lane mask to reduce [BORROWS].
 * @return      The mask with that lane cleared, or 0 when it held only one.
 * @note Subtracting one turns the lowest set bit into zeros below it, so the and clears exactly that bit.
 * @note An empty mask stays empty, so stepping a mask down repeatedly ends rather than wrapping.
 * @note The mask table binds this to drop_first on a little endian target and to drop_last on a big endian one.
 */
MMGR_INLINE mmgr_word scrut_drop_lo(const ScrutMaskCtx *c)
{
    return (mmgr_word)(c->mask & (c->mask - 1u));
}

/**
 * @brief Clears the highest set lane of c->mask.
 *
 * @param[in] c The lane mask to reduce [BORROWS].
 * @return      The mask with that lane cleared, or 0 when it held only one.
 * @note The smear runs from the highest set lane down, so exclusive or-ing it with itself shifted one lane
 *       down leaves just that top lane, which the and then removes.
 * @note The mask table binds this to drop_last on a little endian target and to drop_first on a big endian one.
 */
MMGR_INLINE mmgr_word scrut_drop_hi(const ScrutMaskCtx *c)
{
    const mmgr_word s = MMGR_CALL(scrut_smear, ScrutMaskCtx, .mask = c->mask);

    return c->mask & ~(s ^ (s >> 8));
}

/**
 * @brief Builds a mask covering the first c->bytes bytes of a word, in memory order.
 *
 * @param[in] c The byte count to cover [BORROWS].
 * @return      A word of ones over those bytes and zeros over the rest.
 * @note Shifts one way on a little endian target and the other on a big endian one, so the bytes covered are
 *       always the ones that come first in memory.
 * @note A count of 0 gives an empty mask and a count of MMGR_SWAR_BYTES or more gives a full one, which is
 *       also what keeps the shift count below the word width.
 */
MMGR_INLINE mmgr_word scrut_bytes_below(const ScrutMaskCtx *c)
{
    const mmgr_word all = (mmgr_word) ~(mmgr_word)0;

    if (c->bytes == 0u)
    {
        return 0;
    }
    if (c->bytes >= MMGR_SWAR_BYTES)
    {
        return all;
    }
#if MMGR_HW_BIG_ENDIAN
    return all << ((MMGR_SWAR_BYTES - c->bytes) * 8u);
#else
    return all >> ((MMGR_SWAR_BYTES - c->bytes) * 8u);
#endif
}

/**
 * @brief Builds a lane mask covering the first c->bytes lanes of a word, in memory order.
 *
 * @param[in] c The lane count to cover [BORROWS].
 * @return      A lane mask holding those lanes.
 * @note scrut_bytes_below reduced to lane bits, so this suits and-ing against another lane mask.
 * @note This is what keeps a scan from reading past its count when the last word is only partly wanted.
 */
MMGR_INLINE mmgr_word scrut_lanes_below(const ScrutMaskCtx *c)
{
    return MMGR_CALL(scrut_bytes_below, ScrutMaskCtx, .bytes = c->bytes) & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief Builds the lane mask for the last partial word of a scan of c->bytes bytes.
 *
 * @param[in] c The total byte count and the whole words already done [BORROWS].
 * @return      A lane mask over the bytes still wanted, or 0 once the count is reached.
 * @note Takes the words already done off the total, then hands what is left to scrut_lanes_below.
 * @note A word that is wanted in full gets a full mask, so this can be applied on every pass of a loop.
 * @note The only backend that reads c->wi.
 */
MMGR_INLINE mmgr_word scrut_tail_mask(const ScrutMaskCtx *c)
{
    const size_t done = c->wi * MMGR_SWAR_BYTES;

    if (done >= c->bytes)
    {
        return 0;
    }
    return MMGR_CALL(scrut_lanes_below, ScrutMaskCtx, .bytes = c->bytes - done);
}

/**
 * @brief Returns the lanes that come before the first set lane of c->mask, in memory order.
 *
 * @param[in] c The lane mask to examine [BORROWS].
 * @return      A lane mask holding those lanes.
 * @note On a little endian target the earlier bytes are the lower lanes, so scrut_below_lo gives them.
 * @note On a big endian target they are the higher lanes, so the complement of the smear gives them instead.
 * @note An empty c->mask reports every lane on either branch, since nothing comes first.
 */
MMGR_INLINE mmgr_word scrut_lanes_before(const ScrutMaskCtx *c)
{
#if MMGR_HW_BIG_ENDIAN
    return ~MMGR_CALL(scrut_smear, ScrutMaskCtx, .mask = c->mask) & MMGR_VERBUM_SCRUTOR_HIGH;
#else
    return MMGR_CALL(scrut_below_lo, ScrutMaskCtx, .mask = c->mask);
#endif
}

/**
 * @brief Marks the lanes of c->mask that begin a run of c->bytes set lanes.
 *
 * @param[in] c The lane mask and the run length wanted [BORROWS].
 * @return      A lane mask holding the lanes each run starts at, or 0 when c->bytes exceeds one word.
 * @note Ands the mask with itself shifted along, doubling the reach each pass, so a run of eight takes
 *       three passes rather than seven.
 * @note The step is held to what is still wanted, so a run length that is not a power of two lands exactly.
 * @note Shifts toward the earlier bytes on either byte order, so a surviving lane is where a run begins.
 * @note A c->bytes of 0 or 1 returns c->mask as it stands, since the loop runs no passes.
 */
MMGR_INLINE mmgr_word scrut_run(const ScrutMaskCtx *c)
{
    mmgr_word m = c->mask;
    size_t have = 1u;

    if (c->bytes > MMGR_SWAR_BYTES)
    {
        return 0;
    }
    while (have < c->bytes)
    {
        const size_t step = (have < c->bytes - have) ? have : c->bytes - have;
#if MMGR_HW_BIG_ENDIAN
        m &= (m << (step * 8u));
#else
        m &= (m >> (step * 8u));
#endif
        have += step;
    }
    return m;
}

/**
 * @brief Marks the lanes too near the end of a word for a run of c->bytes to fit inside it.
 *
 * @param[in] c The run length wanted [BORROWS].
 * @return      A lane mask holding those lanes, or 0 when no lane is too near.
 * @note A run of c->bytes can start at any of the first MMGR_SWAR_BYTES minus c->bytes plus one lanes, and
 *       this reports the rest.
 * @note Returns 0 for a c->bytes of 0 or 1, since any lane can start such a run, and for one past a word.
 * @note A caller matching across a word boundary uses this to tell which starts must be checked again.
 */
MMGR_INLINE mmgr_word scrut_run_edge(const ScrutMaskCtx *c)
{
    if ((c->bytes <= 1u) || (c->bytes > MMGR_SWAR_BYTES))
    {
        return 0;
    }
    return MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(scrut_lanes_below, ScrutMaskCtx, .bytes = MMGR_SWAR_BYTES - c->bytes + 1u);
}

/**
 * @brief Loads one mmgr_word from c->at.
 *
 * @param[in] c Address to load from [BORROWS].
 * @return      The bytes at c->at, one per lane, in the target's own order.
 * @note Goes through proxim.load, so c->at needs no particular alignment.
 * @warning c->at must be readable for MMGR_SWAR_BYTES bytes, even when fewer are wanted.
 */
MMGR_INLINE mmgr_word scrut_load(const ScrutWordCtx *c)
{
    return MMGR_CALL(proxim.load, ProximusCfg, .at = c->at);
}

/**
 * @brief Loads one mmgr_word from an aligned c->at.
 *
 * @param[in] c Address to load from [BORROWS].
 * @return      The bytes at c->at, one per lane, in the target's own order.
 * @note Goes through proxim.al_load, which keeps the word type's own alignment, unlike scrut_load.
 * @warning c->at must be readable for MMGR_SWAR_BYTES bytes and aligned for an mmgr_migro_word.
 */
MMGR_INLINE mmgr_word scrut_load_al(const ScrutWordCtx *c)
{
    return MMGR_CALL(proxim.al_load, ProximusCfg, .at = c->at);
}

/**
 * @brief Returns c->word with every letter lane turned to lower case.
 *
 * @param[in] c The word to fold [BORROWS].
 * @return      The word with the case bit set on its letter lanes and every other lane untouched.
 * @note scrut_alpha shifted down two gives the case bit of each letter lane, and or-ing it in forces lower case.
 * @note Only letter lanes are touched, so a digit or a symbol keeps bit five exactly as it was.
 * @note scrut_xor uses the same shifted mask, but clears the bit rather than setting it.
 */
MMGR_INLINE mmgr_word scrut_fold_lower(const ScrutWordCtx *c)
{
    return c->word | (MMGR_CALL(scrut_alpha, ScrutLaneCtx, .word = c->word) >> 2);
}

/**
 * @brief Returns how many whole words a scan of c->bytes bytes must read.
 *
 * @param[in] c The byte count to convert [BORROWS].
 * @return      The count rounded up, so a partial last word still counts as one.
 * @note Written as a divide plus a test of the low bits rather than adding before dividing, so a very large
 *       byte count cannot wrap on the way in.
 * @note A c->bytes of 0 gives 0, so a caller loops no times rather than reading one word.
 */
MMGR_INLINE size_t scrut_words(const ScrutWordCtx *c)
{
    return (c->bytes / MMGR_SWAR_BYTES) + (((c->bytes & (MMGR_SWAR_BYTES - 1u)) != 0u) ? 1u : 0u);
}


/**
 * @brief Binds this module's fixed arguments to GENERIC_ENTRY, with the two types per entry.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] ctx  Context type this entry's backend takes.
 * @param[in] cfg  Argument type the caller passes.
 * @param[in] name Name after the mmgr_scrut_ and scrut_ prefixes, which the two share.
 * @note Both types are parameters. The module carries three of each, one per view: a lane view over
 *       the bytes of a word, a mask view over the bits a lane test produced, and a word view over the
 *       memory a scan walks. The three dispatch tables in verbum_scrutor.h divide the same way.
 */
#define SCRUT_ENTRY(ret, ctx, cfg, name, ...) GENERIC_ENTRY(mmgr_scrut_, scrut_, ctx, cfg, ret, name, __VA_ARGS__)

/**
 * @brief The lane view, one line per entry point.
 *
 * @note Each is documented at its declaration in verbum_scrutor.h.
 * @note The fields each line forwards are the ones that entry reads; MMGR_CALL zeroes the rest. Only
 *       eq and xor forward ci, and only fam_eq forwards fam.
 */
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, ge, .word = c->word, .byte = c->byte)
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, le, .word = c->word, .byte = c->byte)
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, sub7, .word = c->word, .byte = c->byte)
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, has_zero, .word = c->word)
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, eq, .word = c->word, .byte = c->byte, .ci = c->ci)
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, xor, .word = c->word, .val = c->val, .ci = c->ci)
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, fam_eq, .word = c->word, .fam = c->fam, .byte = c->byte)
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, any_upper, .word = c->word)
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, any_digit, .word = c->word)
SCRUT_ENTRY(mmgr_word, ScrutLaneCtx, ScrutLaneCfg, alpha, .word = c->word)
SCRUT_ENTRY(size_t, ScrutLaneCtx, ScrutLaneCfg, lane_count, .mask = c->mask)
SCRUT_ENTRY(size_t, ScrutLaneCtx, ScrutLaneCfg, lane_lo, .mask = c->mask)
SCRUT_ENTRY(size_t, ScrutLaneCtx, ScrutLaneCfg, lane_hi, .mask = c->mask)

/**
 * @brief The mask view, one line per entry point.
 *
 * @note Each is documented at its declaration in verbum_scrutor.h.
 * @note tail_mask is the only entry that forwards wi, and run the only one that forwards both a mask
 *       and a byte count.
 */
SCRUT_ENTRY(mmgr_word, ScrutMaskCtx, ScrutMaskCfg, spread, .mask = c->mask)
SCRUT_ENTRY(mmgr_word, ScrutMaskCtx, ScrutMaskCfg, drop_lo, .mask = c->mask)
SCRUT_ENTRY(mmgr_word, ScrutMaskCtx, ScrutMaskCfg, drop_hi, .mask = c->mask)
SCRUT_ENTRY(mmgr_word, ScrutMaskCtx, ScrutMaskCfg, bytes_below, .bytes = c->bytes)
SCRUT_ENTRY(mmgr_word, ScrutMaskCtx, ScrutMaskCfg, lanes_below, .bytes = c->bytes)
SCRUT_ENTRY(mmgr_word, ScrutMaskCtx, ScrutMaskCfg, lanes_before, .mask = c->mask)
SCRUT_ENTRY(mmgr_word, ScrutMaskCtx, ScrutMaskCfg, tail_mask, .bytes = c->bytes, .wi = c->wi)
SCRUT_ENTRY(mmgr_word, ScrutMaskCtx, ScrutMaskCfg, run, .mask = c->mask, .bytes = c->bytes)
SCRUT_ENTRY(mmgr_word, ScrutMaskCtx, ScrutMaskCfg, run_edge, .bytes = c->bytes)

/**
 * @brief The word view, one line per entry point.
 *
 * @note Each is documented at its declaration in verbum_scrutor.h.
 * @note load and load_al differ in the alignment each promises, not in what they forward.
 */
SCRUT_ENTRY(mmgr_word, ScrutWordCtx, ScrutWordCfg, load, .at = c->at)
SCRUT_ENTRY(mmgr_word, ScrutWordCtx, ScrutWordCfg, load_al, .at = c->at)
SCRUT_ENTRY(mmgr_word, ScrutWordCtx, ScrutWordCfg, fold_lower, .word = c->word)
SCRUT_ENTRY(size_t, ScrutWordCtx, ScrutWordCfg, words, .bytes = c->bytes)
