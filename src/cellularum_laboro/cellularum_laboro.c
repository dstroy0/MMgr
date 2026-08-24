/**
 * @brief Bounded string work over SWAR words: length, compare, search, copy and numeric conversion.
 */
#include "cellularum_laboro/cellularum_laboro.h"
#include "impensa_ancorae_acus/impensa_ancorae_acus.h"
#include "transformo/transformo.h"
#include "verbum_scrutor/verbum_scrutor.h"
#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"

/**
 * @brief Arguments for every cellul backend, grouped by the calls that read them.
 *
 * @note Each backend reads one group; MMGR_CALL zeroes the members it is not given.
 */
typedef struct
{
    const char *const src;      /**< Bytes to read [BORROWS]. */
    const size_t cap;           /**< Bytes readable from src. */
    const char *const other;    /**< Second operand for compare and search [BORROWS]. */
    const size_t other_cap;     /**< Bytes readable from other. */
    char *const dst;            /**< Destination for copy [BORROWS]. */
    const size_t at;            /**< Offset into src where the call starts. */
    const uint8_t byte;         /**< Byte sought by chr. */
    const mmgr_bool ci;         /**< Fold case while comparing. */
    const mmgr_bool end_wins;   /**< A terminator in the same lane counts as a match. */
    const uint8_t **const out;  /**< Set by rd_str to the payload start [BORROWS]. */
    uint32_t *const slen;       /**< Set by rd_str to the payload length [BORROWS]. */

    const mmgr_word wa;         /**< First word for step_word. */
    const mmgr_word wb;         /**< Second word for step_word. */
    const uint8_t ca;           /**< First byte for step_byte. */
    const uint8_t cb;           /**< Second byte for step_byte. */

    const size_t nlen;          /**< Needle length for pick_rows. */
    size_t *const rows;         /**< Needle offsets chosen by pick_rows [BORROWS]. */
    const size_t k;             /**< Needle offset read by ancorae_fold. */

    const char **const end;     /**< Set by the to_ calls past the last byte read [BORROWS]. */
    const char **const cur;     /**< Cursor advanced by expo [BORROWS]. */
    mmgr_iword *const exp;      /**< Set by expo to the signed exponent [BORROWS]. */

    const uint8_t *const mpint; /**< Big-endian integer for mpint_fixed [BORROWS]. */
    const uint32_t mlen;        /**< Bytes in mpint. */
    uint8_t *const field;       /**< Fixed-width output for mpint_fixed [BORROWS]. */
    const size_t fieldlen;      /**< Bytes in field. */
} CellulCtx;

/**
 * @brief Compares one word pair case sensitively and reports whether the walk continues.
 *
 * @param[in] c Words wa and wb, with end_wins [BORROWS].
 * @return      MMGR_SWAR_GO when the words agree and carry no terminator, else YES or NO.
 * @note YES when the terminator lane precedes the first differing lane, or ties it when end_wins.
 */
MMGR_INLINE mmgr_iword cellul_step_word_cs(const CellulCtx *c)
{
    const mmgr_word x = c->wa ^ c->wb;
    const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = c->wa);

    if ((x | z) == 0)
    {
        return MMGR_SWAR_GO;
    }

    size_t dl = MMGR_SWAR_BYTES;
    if (x != 0)
    {
        dl = MMGR_CALL(lane.first, ScrutLaneCfg,
                       .mask = MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = x));
    }
    size_t el = MMGR_SWAR_BYTES;
    if (z != 0)
    {
        el = MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z);
    }
    if (c->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

/**
 * @brief Compares one word pair with case folded and reports whether the walk continues.
 *
 * @param[in] c Words wa and wb, with end_wins [BORROWS].
 * @return      MMGR_SWAR_GO when the words agree and carry no terminator, else YES or NO.
 * @note Differs from cellul_step_word_cs only in taking the difference through lane.xor_ with ci set.
 */
MMGR_INLINE mmgr_iword cellul_step_word_ci(const CellulCtx *c)
{
    const mmgr_word x = MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = c->wa, .val = c->wb, .ci = MMGR_TRUE);
    const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = c->wa);

    if ((x | z) == 0)
    {
        return MMGR_SWAR_GO;
    }

    size_t dl = MMGR_SWAR_BYTES;
    if (x != 0)
    {
        dl = MMGR_CALL(lane.first, ScrutLaneCfg,
                       .mask = MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = x));
    }
    size_t el = MMGR_SWAR_BYTES;
    if (z != 0)
    {
        el = MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z);
    }
    if (c->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

/**
 * @brief Compares one byte pair case sensitively and reports whether the walk continues.
 *
 * @param[in] c Bytes ca and cb, with end_wins [BORROWS].
 * @return      MMGR_SWAR_GO when the bytes match and ca is not the terminator, else YES or NO.
 * @note A terminating ca gives YES when cb also terminates, or when end_wins is set.
 */
MMGR_INLINE mmgr_iword cellul_step_byte_cs(const CellulCtx *c)
{
    if (c->ca == 0)
    {
        if (c->ca == c->cb)
        {
            return MMGR_SWAR_YES;
        }
        return c->end_wins ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    if (c->ca != c->cb)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

/**
 * @brief Compares one byte pair with case folded and reports whether the walk continues.
 *
 * @param[in] c Bytes ca and cb, with end_wins [BORROWS].
 * @return      MMGR_SWAR_GO when the bytes match and ca is not the terminator, else YES or NO.
 * @note Takes the difference through lane.xor_ with ci set, so only the folded result is tested.
 */
MMGR_INLINE mmgr_iword cellul_step_byte_ci(const CellulCtx *c)
{
    const mmgr_word d =
        MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = (mmgr_word)c->ca, .val = (mmgr_word)c->cb, .ci = MMGR_TRUE);

    if (c->ca == 0)
    {
        if (d == 0)
        {
            return MMGR_SWAR_YES;
        }
        return c->end_wins ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    if (d != 0)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

/**
 * @brief Returns whether ch is one of the six whitespace characters.
 *
 * @param[in] ch Character to test.
 * @return       MMGR_TRUE for space, tab, newline, carriage return, form feed or vertical tab.
 */
MMGR_INLINE mmgr_bool cellul_is_ws(char ch)
{
    return (ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r') || (ch == '\f') || (ch == '\v');
}

/**
 * @brief Returns whether ch lies between '0' and '9'.
 *
 * @param[in] ch Character to test.
 * @return       MMGR_TRUE for the ten decimal digits.
 */
MMGR_INLINE mmgr_bool cellul_is_digit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

/**
 * @brief Returns the offset of the first zero byte in src, or cap when there is none.
 *
 * @param[in] c Bytes src and the readable extent cap [BORROWS].
 * @return      Bytes before the terminator, at most cap.
 * @note Scans a word at a time; mask.tail keeps lanes past cap out of the result.
 */
MMGR_INLINE size_t cellul_len(const CellulCtx *c)
{
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word m =
            MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at)) &
            MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        if (m != 0)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return c->cap;
}

/**
 * @brief Finds the first occurrence of byte in src, stopping at the terminator.
 *
 * @param[in] c Bytes src, the extent cap and the byte sought [BORROWS].
 * @return      Address of the match, or NULL when none precedes the terminator [BORROWS].
 * @note A byte of 0 returns the terminator's own address, which is src plus cellul_len.
 * @note mask.before drops lanes at or past the terminator, so a later match is not reported.
 */
MMGR_INLINE const char *cellul_chr(const CellulCtx *c)
{
    if (c->byte == 0u)
    {
        return c->src + cellul_len(c);
    }

    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word w = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at);
        const mmgr_word keep = MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        const mmgr_word end = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w) & keep;
        const mmgr_word hit = MMGR_CALL(lane.eq, ScrutLaneCfg, .word = w, .byte = c->byte, .ci = MMGR_FALSE) &
                                    keep & MMGR_CALL(mask.before, ScrutMaskCfg, .mask = end);

        if (hit != 0)
        {
            return c->src + at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = hit);
        }
        if (end != 0)
        {
            return NULL;
        }
    }
    return NULL;
}

/**
 * @brief Returns the offset of the first byte where src and other differ, case sensitively.
 *
 * @param[in] c Bytes src and other, with the extent cap [BORROWS].
 * @return      Offset of the first difference, or cap when the two agree throughout.
 * @note Compares whole words; mask.tail keeps lanes past cap out of the result.
 * @warning Does not stop at a terminator; all cap bytes are compared.
 */
MMGR_INLINE size_t cellul_diff_cs(const CellulCtx *c)
{
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word d = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at) ^
                                  MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at);
        const mmgr_word m = (MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = d)) &
                                  MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        if (m != 0)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return c->cap;
}

/**
 * @brief Returns the offset of the first byte where src and other differ, with case folded.
 *
 * @param[in] c Bytes src and other, with the extent cap [BORROWS].
 * @return      Offset of the first difference, or cap when the two agree throughout.
 * @note Differs from cellul_diff_cs only in taking the difference through lane.xor_ with ci set.
 * @warning Does not stop at a terminator; all cap bytes are compared.
 */
MMGR_INLINE size_t cellul_diff_ci(const CellulCtx *c)
{
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word d = MMGR_CALL(lane.xor_, ScrutLaneCfg,
                                            .word = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at),
                                            .val = MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at), .ci = MMGR_TRUE);
        const mmgr_word m = (MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = d)) &
                                  MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        if (m != 0)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return c->cap;
}

/**
 * @brief Reports whether src reaches its terminator without differing from other, case sensitively.
 *
 * @param[in] c Bytes src and other, the extent cap, and end_wins [BORROWS].
 * @return      MMGR_TRUE when src's terminator precedes the first differing byte.
 * @note end_wins makes a terminator in the same lane as the difference count as agreement.
 * @note Reaching cap with neither a terminator nor a difference returns end_wins.
 */
MMGR_INLINE mmgr_bool cellul_agree_cs(const CellulCtx *c)
{
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word keep = MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word x =
            (MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa ^ wb)) & keep;

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(c->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return c->end_wins;
}

/**
 * @brief Reports whether src reaches its terminator without differing from other, case folded.
 *
 * @param[in] c Bytes src and other, the extent cap, and end_wins [BORROWS].
 * @return      MMGR_TRUE when src's terminator precedes the first differing byte.
 * @note Differs from cellul_agree_cs only in folding the two words through lane.xor_ with ci set.
 * @note Reaching cap with neither a terminator nor a difference returns end_wins.
 */
MMGR_INLINE mmgr_bool cellul_agree_ci(const CellulCtx *c)
{
    const size_t nw = MMGR_CALL(word.count, ScrutWordCfg, .bytes = c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word keep = MMGR_CALL(mask.tail, ScrutMaskCfg, .bytes = c->cap, .wi = wi);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = c->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = c->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word fold = MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = wa, .val = wb, .ci = MMGR_TRUE);
        const mmgr_word x =
            (MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = fold)) & keep;

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(c->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return c->end_wins;
}

/**
 * @brief Reads other[k] and folds it to lower case when ci is set.
 *
 * @param[in] c Needle bytes other, the offset k, and ci [BORROWS].
 * @return      The byte, with 'A' to 'Z' mapped to 'a' to 'z' when ci is set.
 */
MMGR_INLINE uint8_t cellul_ancorae_fold(const CellulCtx *c)
{
    const uint8_t b = (uint8_t)c->other[c->k];

    if (c->ci && (b >= (uint8_t)'A') && (b <= (uint8_t)'Z'))
    {
        // Explicit cast keeps the result in uint8_t; bit 5 is what separates the two cases
        return (uint8_t)(b | 0x20u);
    }
    return b;
}

/**
 * @brief Chooses the needle offsets whose bytes cost least, for the search sieve.
 *
 * @param[in,out] c Needle other, its length nlen, ci, and the rows array to fill [BORROWS].
 * @return          Number of offsets written to c->rows, at most MMGR_SIEVE_ROWS.
 * @note Only the first MMGR_SWAR_BYTES of the needle are candidates, since one word is tested at a time.
 * @note Cost comes from ancorae.impensa, so rarer bytes are preferred.
 */
MMGR_INLINE size_t cellul_pick_rows(const CellulCtx *c)
{
    const size_t limit = (c->nlen > MMGR_SWAR_BYTES) ? MMGR_SWAR_BYTES : c->nlen;
    const size_t want = (limit > MMGR_SIEVE_ROWS) ? MMGR_SIEVE_ROWS : limit;

    for (size_t r = 0; r < want; ++r)
    {
        size_t best = 0;
        uint8_t best_cost = 255;

        for (size_t k = 0; k < limit; ++k)
        {
            size_t taken = 0;

            for (size_t q = 0; q < r; ++q)
            {
                if (c->rows[q] == k)
                {
                    taken = 1;
                }
            }

            if (taken)
            {
                continue;
            }

            // Cost is looked up only for offsets still in play
            const uint8_t cost = MMGR_CALL(ancorae.impensa, AncoraeCfg,
                                           .byte = cellul_ancorae_fold(&(CellulCtx){.other = c->other, .k = k, .ci = c->ci}));
            if (cost < best_cost)
            {
                best_cost = cost;
                best = k;
            }
        }
        c->rows[r] = best;
    }
    return want;
}

/**
 * @brief Finds the first occurrence of the needle inside the haystack.
 *
 * @param[in] c  Haystack src with cap, and needle other with other_cap [BORROWS].
 * @param[in] ci Fold case while matching.
 * @return       Address of the match, or NULL when there is none [BORROWS].
 * @note An empty needle returns the haystack start; a needle longer than cap returns NULL.
 * @note Candidate words are sieved on the cheapest needle offsets, then verified in full.
 * @note Word scanning covers only the starts that stay in bounds; the rest are walked one byte at a time.
 */
MMGR_INLINE const char *cellul_find_core(const CellulCtx *c, mmgr_bool ci)
{
    const char *const hay = c->src;
    const char *const needle = c->other;
    const size_t read_cap = c->cap;

    const size_t nlen = cellul_len(&(CellulCtx){.src = needle, .cap = c->other_cap});

    if (nlen == 0u)
    {
        return hay;
    }
    if (nlen > read_cap)
    {
        return NULL;
    }

    size_t rows[MMGR_SIEVE_ROWS];
    const size_t nrows = cellul_pick_rows(&(CellulCtx){.other = needle, .nlen = nlen, .rows = rows, .ci = ci});

    const size_t take = (nlen > MMGR_SWAR_BYTES) ? MMGR_SWAR_BYTES : nlen;
    const mmgr_word nmask = MMGR_CALL(mask.bytes_below, ScrutMaskCfg, .bytes = take);
    const mmgr_word nraw = MMGR_CALL(word.load, ScrutWordCfg, .at = needle) & nmask;
    const mmgr_word nword =
        ci ? (MMGR_CALL(word.fold_lower, ScrutWordCfg, .word = nraw) & nmask) : nraw;

    const size_t starts = read_cap - nlen + 1u;

    size_t maxrow = rows[0];

    for (size_t r = 1; r < nrows; ++r)
    {
        if (rows[r] > maxrow)
        {
            maxrow = rows[r];
        }
    }

    const size_t tail =
        (nlen > take) ? (MMGR_CALL(word.count, ScrutWordCfg, .bytes = nlen - take) * MMGR_SWAR_BYTES) : 0u;
    const size_t verify_reach = (MMGR_SWAR_BYTES - 1u) + take + tail;
    const size_t ancorae_reach = maxrow + MMGR_SWAR_BYTES;
    const size_t reach = (ancorae_reach > verify_reach) ? ancorae_reach : verify_reach;

    size_t safe = (read_cap >= reach) ? ((read_cap - reach) + 1u) : 0u;

    if (safe > starts)
    {
        safe = starts;
    }

    const size_t nw = safe / MMGR_SWAR_BYTES;

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word end =
            MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + at));
        mmgr_word m;

        m = MMGR_CALL(lane.eq, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + at + rows[0]),
                      .byte = (uint8_t)needle[rows[0]], .ci = ci);

        for (size_t r = 1; r < nrows; ++r)
        {
            m &= MMGR_CALL(lane.eq, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + at + rows[r]),
                           .byte = (uint8_t)needle[rows[r]], .ci = ci);
        }

        if (end != 0)
        {
            m &= MMGR_CALL(mask.before, ScrutMaskCfg, .mask = end);
        }

        while (m != 0)
        {
            const size_t k = at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
            const mmgr_word cw = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + k);

            const mmgr_word syn =
                ((!ci || (MMGR_CALL(lane.any_upper, ScrutLaneCfg, .word = cw) == 0))
                     ? (cw ^ nword)
                     : MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = cw, .val = nword, .ci = MMGR_TRUE)) &
                nmask;

            if (syn == 0)
            {
                if (take == nlen)
                {
                    return hay + k;
                }

                const CellulCtx v = {.src = hay + k + take, .other = needle + take, .cap = nlen - take};
                const size_t d = ci ? cellul_diff_ci(&v) : cellul_diff_cs(&v);

                if (d == (nlen - take))
                {
                    return hay + k;
                }
            }
            m = MMGR_CALL(mask.drop_first, ScrutMaskCfg, .mask = m);
        }
        if (end != 0)
        {
            return NULL;
        }
    }

    for (size_t k = nw * MMGR_SWAR_BYTES; k < starts; ++k)
    {
        if (hay[k] == '\0')
        {
            return NULL;
        }

        size_t i = 0;
        while (i < nlen)
        {
            const uint8_t h = (uint8_t)hay[k + i];
            const CellulCtx b = {.ca = (uint8_t)needle[i], .cb = h, .end_wins = MMGR_FALSE};

            if ((h == 0u) || ((ci ? cellul_step_byte_ci(&b) : cellul_step_byte_cs(&b)) == MMGR_SWAR_NO))
            {
                break;
            }
            ++i;
        }
        if (i == nlen)
        {
            return hay + k;
        }
    }
    return NULL;
}

/**
 * @brief Copies src into dst and terminates it, writing at most cap bytes in total.
 *
 * @param[in,out] c Source src, destination dst, and the destination extent cap [BORROWS].
 * @return          Bytes copied, not counting the terminator.
 * @note A cap of 0 copies nothing and writes no terminator.
 * @note The source is measured against cap minus one, leaving room for the terminator.
 */
MMGR_INLINE size_t cellul_copy(const CellulCtx *c)
{
    if (c->cap == 0u)
    {
        return 0u;
    }

    const size_t n = cellul_len(&(CellulCtx){.src = c->src, .cap = c->cap - 1u});

    MMGR_CALL(proxim.read, ProximusCfg, .dst = c->dst, .at = c->src, .size = n);
    c->dst[n] = '\0';
    return n;
}

/**
 * @brief Reads a big-endian 32-bit length at at, then points out and slen at the payload.
 *
 * @param[in,out] c Buffer src with extent cap, the offset at, and the out and slen targets [BORROWS].
 * @return          MMGR_TRUE when the length and its payload both fit within cap.
 * @note Returns MMGR_FALSE when fewer than four bytes remain, or when the payload would pass cap.
 * @note Writes nothing through out or slen unless it returns MMGR_TRUE.
 */
MMGR_INLINE mmgr_bool cellul_rd_str(const CellulCtx *c)
{
    const uint8_t *const buf = (const uint8_t *)c->src;
    size_t at = c->at;
    if ((at > c->cap) || ((c->cap - at) < 4u))
    {
        return MMGR_FALSE;
    }

    const uint32_t n = (uint32_t)MMGR_CALL(magna_extremitas.rd, EndianCfg, .src = buf + at, .width = MMGR_ENDIAN_32);
    at += 4u;

    if (n > (c->cap - at))
    {
        return MMGR_FALSE;
    }
    *c->out = buf + at;
    *c->slen = n;
    return MMGR_TRUE;
}

/**
 * @brief Reads an optionally signed decimal integer from src.
 *
 * @param[in,out] c Text src, and the optional end target [BORROWS].
 * @return          The value, negated when a minus sign was read.
 * @note Leading whitespace is skipped, then one optional '+' or '-'.
 * @note When end is not NULL it is set past the last digit, or back to src when no digit was read.
 * @warning Digits are accumulated without an overflow test; the caller bounds the input length.
 */
MMGR_INLINE mmgr_iword cellul_to_long(const CellulCtx *c)
{
    const char *p = c->src;

    while (cellul_is_ws(*p))
    {
        p++;
    }

    mmgr_bool neg = MMGR_FALSE;
    if ((*p == '+') || (*p == '-'))
    {
        // Sign is read before the advance, keeping the increment out of the compare
        neg = (*p == '-');
        p++;
    }

    const char *const ds = p;
    mmgr_word v = 0;
    while (cellul_is_digit(*p))
    {
        // Digit is taken before the advance, keeping the increment out of the accumulate
        const mmgr_word d = (mmgr_word)(*p - '0');
        p++;
        v = (mmgr_word)(v * 10u) + d;
    }

    if (c->end != NULL)
    {
        *c->end = (p != ds) ? p : c->src;
    }
    if (neg)
    {
        return (mmgr_iword)(0u - v);
    }
    return (mmgr_iword)v;
}

/**
 * @brief Reads an unsigned decimal integer from src.
 *
 * @param[in,out] c Text src, and the optional end target [BORROWS].
 * @return          The accumulated value.
 * @note Leading whitespace is skipped, then one optional '+'; a '-' is not accepted and stops the read.
 * @note When end is not NULL it is set past the last digit, or back to src when no digit was read.
 * @warning Digits are accumulated without an overflow test; the caller bounds the input length.
 */
MMGR_INLINE mmgr_word cellul_to_ulong(const CellulCtx *c)
{
    const char *p = c->src;

    while (cellul_is_ws(*p))
    {
        p++;
    }
    if (*p == '+')
    {
        p++;
    }

    const char *const ds = p;
    mmgr_word v = 0;
    while (cellul_is_digit(*p))
    {
        // Digit is taken before the advance, keeping the increment out of the accumulate
        const mmgr_word d = (mmgr_word)(*p - '0');
        p++;
        v = (mmgr_word)(v * 10u) + d;
    }

    if (c->end != NULL)
    {
        *c->end = (p != ds) ? p : c->src;
    }
    return v;
}

/**
 * @brief Reads a signed decimal exponent, advancing the cursor past it.
 *
 * @param[in,out] c Cursor cur and the exponent target exp [BORROWS].
 * @note The cursor sits on the 'e' or 'E', which is consumed first.
 * @note When no digit follows, the cursor is put back where it started and exp is left alone.
 * @note Digits beyond MMGR_MUTO_EXP_LIMIT are consumed but stop changing the value.
 */
MMGR_INLINE void cellul_expo(const CellulCtx *c)
{
    const char *const mark = *c->cur;

    (*c->cur)++;

    mmgr_bool eneg = MMGR_FALSE;
    if ((**c->cur == '+') || (**c->cur == '-'))
    {
        // Sign is read before the advance, keeping the increment out of the compare
        eneg = (**c->cur == '-');
        (*c->cur)++;
    }
    if (!cellul_is_digit(**c->cur))
    {
        *c->cur = mark;
        return;
    }

    mmgr_iword ex = 0;
    while (cellul_is_digit(**c->cur))
    {
        if (ex < MMGR_MUTO_EXP_LIMIT)
        {
            ex = (mmgr_iword)((ex * 10) + (**c->cur - '0'));
        }
        (*c->cur)++;
    }
    *c->exp = eneg ? -ex : ex;
}

/**
 * @brief Reads a decimal floating point number from src.
 *
 * @param[in,out] c Text src, and the optional end target [BORROWS].
 * @return          The value assembled by muto.scale from the mantissa and exponent.
 * @note Accepts leading whitespace, one optional sign, digits, one optional point, then an optional exponent.
 * @note Digits that no longer fit the mantissa advance the exponent instead, and a non-zero one sets the sticky rest.
 * @note An exponent is read only when at least one digit was seen before it.
 * @note When end is not NULL it is set past the number, or back to src when no digit was read.
 */
MMGR_INLINE double cellul_to_double(const CellulCtx *c)
{
    const char *p = c->src;

    while (cellul_is_ws(*p))
    {
        p++;
    }

    mmgr_bool neg = MMGR_FALSE;
    if ((*p == '+') || (*p == '-'))
    {
        // Sign is read before the advance, keeping the increment out of the compare
        neg = (*p == '-');
        p++;
    }

    mmgr_bool any = MMGR_FALSE;
    mmgr_u64 mant = 0;
    mmgr_iword drop = 0;
    mmgr_iword over = 0;
    mmgr_iword lost = 0;

    while (cellul_is_digit(*p))
    {
        if (!MMGR_CALL(muto.take, TransformoCfg, .mant = &mant, .digit = *p))
        {
            ++over;
            lost |= (*p != '0') ? 1 : 0;
        }
        any = MMGR_TRUE;
        ++p;
    }
    if (*p == '.')
    {
        ++p;
        while (cellul_is_digit(*p))
        {
            if (MMGR_CALL(muto.take, TransformoCfg, .mant = &mant, .digit = *p))
            {
                ++drop;
            }
            else
            {
                lost |= (*p != '0') ? 1 : 0;
            }
            any = MMGR_TRUE;
            ++p;
        }
    }

    mmgr_iword ex = 0;
    if (any && ((*p == 'e') || (*p == 'E')))
    {
        cellul_expo(&(CellulCtx){.cur = &p, .exp = &ex});
    }

    const double val =
        MMGR_CALL(muto.scale, TransformoCfg, .mant = &mant, .ex = (mmgr_iword)(ex + over - drop), .rest = lost, .neg = neg);

    if (c->end != NULL)
    {
        *c->end = any ? p : c->src;
    }
    return val;
}

/**
 * @brief Reads a decimal floating point number from src and narrows it to float.
 *
 * @param[in,out] c Text src, and the optional end target [BORROWS].
 * @return          The value from cellul_to_double, narrowed to float.
 * @note Rounding happens once, on the narrowing; the parse itself is done at double width.
 */
MMGR_INLINE float cellul_to_float(const CellulCtx *c)
{
    // Explicit cast narrows the double result into the float container
    return (float)cellul_to_double(c);
}

/**
 * @brief Right-aligns a big-endian integer into a fixed-width field, zero filling the front.
 *
 * @param[in,out] c Integer mpint with length mlen, and the field with length fieldlen [BORROWS].
 * @return          MMGR_TRUE when the integer fits, MMGR_FALSE when it does not.
 * @note Leading zero bytes of mpint are skipped before the width is checked.
 * @note The field is cleared first, so the bytes ahead of the value are zero.
 * @note Nothing is written to the field when it returns MMGR_FALSE.
 */
MMGR_INLINE mmgr_bool cellul_mpint_fixed(const CellulCtx *c)
{
    uint32_t off = 0;

    while ((off < c->mlen) && (c->mpint[off] == 0))
    {
        off++;
    }

    const uint32_t vlen = c->mlen - off;
    if (vlen > c->fieldlen)
    {
        return MMGR_FALSE;
    }
    MMGR_CALL(memor.set, MemoriaCfg, .dst = c->field, .val = (uint8_t)0, .bytes = c->fieldlen);
    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = c->field + (c->fieldlen - vlen), .src = c->mpint + off, .bytes = (size_t)vlen);
    return MMGR_TRUE;
}

/**
 * @brief Returns a copy of the argument struct.
 *
 * @note The name is parenthesised so a like-named macro from mmgr_string_shim.h cannot expand here.
 * @note Documented at the declaration in cellularum_laboro.h.
 */
CatenaFinitaCfg (mmgr_cellul_init)(const CatenaFinitaCfg *c)
{
    return *c;
}

/**
 * @brief Measures src from offset at, within the remaining cap.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
size_t (mmgr_cellul_len)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_len, CellulCtx, .src = c->src + c->at, .cap = c->cap - c->at);
}

/**
 * @brief Picks the folded or exact difference walk on c->ci.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
size_t (mmgr_cellul_diff)(const CatenaFinitaCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_diff_ci, CellulCtx, .src = c->src, .other = c->other, .cap = c->cap);
    }
    return MMGR_CALL(cellul_diff_cs, CellulCtx, .src = c->src, .other = c->other, .cap = c->cap);
}

/**
 * @brief Compares src against other with end_wins clear, so both must terminate together.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_bool (mmgr_cellul_eq)(const CatenaFinitaCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_agree_ci, CellulCtx, .src = c->src, .other = c->other, .cap = c->cap, .end_wins = MMGR_FALSE);
    }
    return MMGR_CALL(cellul_agree_cs, CellulCtx, .src = c->src, .other = c->other, .cap = c->cap, .end_wins = MMGR_FALSE);
}

/**
 * @brief Tests whether src begins with other, using end_wins so the prefix may end early.
 *
 * @note The operands are swapped, so it is other that is measured for its terminator.
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_bool (mmgr_cellul_starts)(const CatenaFinitaCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_agree_ci, CellulCtx, .src = c->other, .other = c->src, .cap = c->cap, .end_wins = MMGR_TRUE);
    }
    return MMGR_CALL(cellul_agree_cs, CellulCtx, .src = c->other, .other = c->src, .cap = c->cap, .end_wins = MMGR_TRUE);
}

/**
 * @brief Searches src for other, folding case when c->ci is set.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
const char *(mmgr_cellul_find)(const CatenaFinitaCfg *c)
{
    const CellulCtx x = {.src = c->src, .cap = c->cap, .other = c->other, .other_cap = c->other_cap};

    if (c->ci)
    {
        return cellul_find_core(&x, MMGR_TRUE);
    }
    return cellul_find_core(&x, MMGR_FALSE);
}

/**
 * @brief Reports whether mmgr_cellul_find returns a match.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_bool (mmgr_cellul_has)(const CatenaFinitaCfg *c)
{
    return (mmgr_bool)((mmgr_cellul_find)(c) != NULL);
}

/**
 * @brief Searches src for c->byte, stopping at the terminator.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
const char *(mmgr_cellul_chr)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_chr, CellulCtx, .src = c->src, .cap = c->cap, .byte = c->byte);
}

/**
 * @brief Copies src into dst within cap, always terminating unless cap is 0.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
size_t (mmgr_cellul_copy)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_copy, CellulCtx, .dst = c->dst, .src = c->src, .cap = c->cap);
}

/**
 * @brief Tests the byte at src[at] for whitespace.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_bool (mmgr_cellul_ws)(const CatenaFinitaCfg *c)
{
    return cellul_is_ws(c->src[c->at]);
}

/**
 * @brief Tests the byte at src[at] for a decimal digit.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_bool (mmgr_cellul_digit)(const CatenaFinitaCfg *c)
{
    return cellul_is_digit(c->src[c->at]);
}

/**
 * @brief Reads a length-prefixed string at src[at] into out and slen.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_bool (mmgr_cellul_rd_str)(const CatenaFinitaCfg *c)
{
    return MMGR_CALL(cellul_rd_str, CellulCtx, .src = c->src, .cap = c->cap, .at = c->at, .out = c->out,
                     .slen = c->slen);
}

/**
 * @brief Picks the folded or exact word step on c->ci.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_iword (mmgr_cellul_step_word)(const VerboProgrediorCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_step_word_ci, CellulCtx, .wa = c->wa, .wb = c->wb, .end_wins = c->end_wins);
    }
    return MMGR_CALL(cellul_step_word_cs, CellulCtx, .wa = c->wa, .wb = c->wb, .end_wins = c->end_wins);
}

/**
 * @brief Picks the folded or exact byte step on c->ci.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_iword (mmgr_cellul_step_byte)(const VerboProgrediorCfg *c)
{
    if (c->ci)
    {
        return MMGR_CALL(cellul_step_byte_ci, CellulCtx, .ca = c->ca, .cb = c->cb, .end_wins = c->end_wins);
    }
    return MMGR_CALL(cellul_step_byte_cs, CellulCtx, .ca = c->ca, .cb = c->cb, .end_wins = c->end_wins);
}

/**
 * @brief Reads a signed decimal integer from c->src.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_iword (mmgr_cellul_to_long)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_long, CellulCtx, .src = c->src, .end = c->end);
}

/**
 * @brief Reads an unsigned decimal integer from c->src.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_word (mmgr_cellul_to_ulong)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_ulong, CellulCtx, .src = c->src, .end = c->end);
}

/**
 * @brief Reads a decimal floating point number from c->src.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
double (mmgr_cellul_to_double)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_double, CellulCtx, .src = c->src, .end = c->end);
}

/**
 * @brief Reads a decimal floating point number from c->src, narrowed to float.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
float (mmgr_cellul_to_float)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_to_float, CellulCtx, .src = c->src, .end = c->end);
}

/**
 * @brief Right-aligns c->mpint into c->field, zero filling the front.
 *
 * @note Documented at the declaration in cellularum_laboro.h.
 */
mmgr_bool (mmgr_cellul_mpint_fixed)(const TransfiguroCfg *c)
{
    return MMGR_CALL(cellul_mpint_fixed, CellulCtx, .mpint = c->mpint, .mlen = c->mlen, .field = c->field,
                     .fieldlen = c->fieldlen);
}
