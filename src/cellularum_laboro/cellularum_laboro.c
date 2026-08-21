// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "cellularum_laboro/cellularum_laboro.h"
#include "impensa_ancorae_acus/impensa_ancorae_acus.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "transformo/transformo.h"
#include "fractio/fractio.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @file cellularum_laboro.c
 * @brief Bounded string work. Every entry takes a read cap and never runs past it.
 *
 * Every entry below takes one parameter, a pointer to CellulCtx. A scan is a string, how far it
 * may be read, and whatever it is looking for, so those are one context.
 *
 * Scans count words, not bytes. No alignment peel and no byte remainder: an unaligned load is the
 * same instruction as an aligned one, and the last word is masked rather than walked.
 *
 * Case folding is a compile time fact at every call site. The _cs and _ci pairs below exist so the
 * fold is constant inside the loop, not so the source says everything twice - which is why the
 * fold is never read out of the context in a loop. Which of the pair runs is decided once, before
 * the scan starts, and the context carries the answer only so the entries that dispatch can read
 * it.
 */

/** @brief One scan, compare, copy or parse. */
typedef struct
{
    /* the strings */
    const char *s;      /**< The string, the haystack, or the pattern. */
    const char *t;      /**< The second string, the prefix, or the needle. */
    char *dst;          /**< Destination, when copying. */
    size_t cap;         /**< How far @c s may be read. */
    size_t t_cap;       /**< How far @c t may be read. */
    uint8_t byte;       /**< The byte being looked for. */
    mmgr_bool ci;       /**< Fold case. Read once, before any loop. */
    int end_wins;       /**< Whether the pattern ending counts as a match. */

    /* one step of a compare */
    mmgr_scrut_word wa; /**< Word from the pattern. */
    mmgr_scrut_word wb; /**< Word from the subject. */
    unsigned char ca;   /**< Byte from the pattern. */
    unsigned char cb;   /**< Byte from the subject. */

    /* picking the sieve rows */
    size_t nlen;   /**< Needle length. */
    size_t *rows;  /**< Where the chosen offsets go. */
    size_t k;      /**< Offset being costed. */
    unsigned fmask;/**< Family bits, when the family stage is compiled in. */
    size_t *off;   /**< Where a family run starts. */

    /* the parsers */
    const char **end; /**< Where parsing stopped. May be NULL. */
    const char **p;   /**< Cursor, for the exponent. */
    int *out;         /**< The exponent, signed. */
} CellulCtx;

/**
 * @brief Length up to the terminator.
 * @param c The scan.
 * @return Offset of the terminator, or @c cap if there is none in range.
 */
MMGR_INLINE size_t cellul_len(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word m = scrut.has_zero(scrut.load(c->s + at)) & mmgr_scrut_tail_mask(c->cap, wi);
        if (m != 0)
        {
            return at + scrut.zero_lane(m);
        }
    }
    return c->cap;
}

/**
 * @brief First @c byte at or before the terminator.
 * @param c The scan.
 * @return Pointer to it, or NULL.
 *
 * One pass. Both questions are masks over the same loaded word, so there is no reason to walk the
 * string twice to ask them. lanes_before drops any hit past the terminator with no compare.
 *
 * A byte of zero is the one case the mask cannot answer, because then both masks are the same
 * mask. strchr is defined to find the terminator, so that is the length.
 */
MMGR_INLINE const char *cellul_chr(const CellulCtx *c)
{
    if (c->byte == 0u)
    {
        return c->s + cellul_len(c);
    }

    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word w = scrut.load(c->s + at);
        const mmgr_scrut_word keep = mmgr_scrut_tail_mask(c->cap, wi);
        const mmgr_scrut_word end = (mmgr_scrut_word)(scrut.has_zero(w) & keep);
        const mmgr_scrut_word hit =
            (mmgr_scrut_word)(scrut.eq(w, c->byte, MMGR_FALSE) & keep & mmgr_scrut_lanes_before(end));

        if (hit != 0)
        {
            return c->s + at + scrut.zero_lane(hit);
        }
        if (end != 0)
        {
            return NULL;
        }
    }
    return NULL;
}

/**
 * @brief Where two strings first differ.
 * @param c The compare.
 * @return Offset of the first difference, or @c cap if they agree that far.
 */
MMGR_INLINE size_t cellul_diff_cs(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word d = scrut.load(c->s + at) ^ scrut.load(c->t + at);
        const mmgr_scrut_word m =
            (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(d)) & mmgr_scrut_tail_mask(c->cap, wi));
        if (m != 0)
        {
            return at + scrut.zero_lane(m);
        }
    }
    return c->cap;
}

/**
 * @brief Where two strings first differ, ignoring case.
 * @param c The compare.
 * @return Offset of the first difference, or @c cap if they agree that far.
 */
MMGR_INLINE size_t cellul_diff_ci(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word d = scrut.xor_(scrut.load(c->s + at), scrut.load(c->t + at), MMGR_TRUE);
        const mmgr_scrut_word m =
            (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(d)) & mmgr_scrut_tail_mask(c->cap, wi));
        if (m != 0)
        {
            return at + scrut.zero_lane(m);
        }
    }
    return c->cap;
}

/**
 * @brief Advance one word of a prefix compare.
 * @param c The step.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int cellul_step_word_cs(const CellulCtx *c)
{
    const mmgr_scrut_word x = c->wa ^ c->wb;
    const mmgr_scrut_word z = scrut.has_zero(c->wa);

    if ((x | z) == 0)
    {
        return MMGR_SWAR_GO;
    }

    size_t dl = MMGR_SWAR_BYTES;
    if (x != 0)
    {
        dl = scrut.zero_lane(MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(x));
    }
    size_t el = MMGR_SWAR_BYTES;
    if (z != 0)
    {
        el = scrut.zero_lane(z);
    }
    if (c->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

/**
 * @brief Advance one word of a prefix compare, ignoring case.
 * @param c The step.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int cellul_step_word_ci(const CellulCtx *c)
{
    const mmgr_scrut_word x = scrut.xor_(c->wa, c->wb, MMGR_TRUE);
    const mmgr_scrut_word z = scrut.has_zero(c->wa);

    if ((x | z) == 0)
    {
        return MMGR_SWAR_GO;
    }

    size_t dl = MMGR_SWAR_BYTES;
    if (x != 0)
    {
        dl = scrut.zero_lane(MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(x));
    }
    size_t el = MMGR_SWAR_BYTES;
    if (z != 0)
    {
        el = scrut.zero_lane(z);
    }
    if (c->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

/**
 * @brief Advance one byte of a prefix compare.
 * @param c The step.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int cellul_step_byte_cs(const CellulCtx *c)
{
    if (c->ca == 0)
    {
        if (c->ca == c->cb)
        {
            return MMGR_SWAR_YES;
        }
        return (c->end_wins != 0) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    if (c->ca != c->cb)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

/**
 * @brief Advance one byte of a prefix compare, ignoring case.
 * @param c The step.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int cellul_step_byte_ci(const CellulCtx *c)
{
    const mmgr_scrut_word d = scrut.xor_((mmgr_scrut_word)c->ca, (mmgr_scrut_word)c->cb, MMGR_TRUE);

    if (c->ca == 0)
    {
        if (d == 0)
        {
            return MMGR_SWAR_YES;
        }
        return (c->end_wins != 0) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    if (d != 0)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

/**
 * @brief Does @c t agree with @c s up to s's terminator.
 * @param c The compare.
 * @return MMGR_TRUE if they agree.
 *
 * Two events race in each word - the pattern ends, or the two differ - and whichever comes first
 * in address order decides.
 *
 * Comparing lane indices rather than trailing bit masks is what removes the endian branch. A
 * trailing bit mask means below, and below is the wrong direction on a big endian load.
 */
MMGR_INLINE mmgr_bool cellul_agree_cs(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word keep = mmgr_scrut_tail_mask(c->cap, wi);
        const mmgr_scrut_word wa = scrut.load(c->s + at);
        const mmgr_scrut_word wb = scrut.load(c->t + at);
        const mmgr_scrut_word z = (mmgr_scrut_word)(scrut.has_zero(wa) & keep);
        const mmgr_scrut_word x = (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(wa ^ wb)) & keep);

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? scrut.zero_lane(z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? scrut.zero_lane(x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(c->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return (mmgr_bool)(c->end_wins != 0);
}

/**
 * @brief Does @c t agree with @c s up to s's terminator, ignoring case.
 * @param c The compare.
 * @return MMGR_TRUE if they agree.
 */
MMGR_INLINE mmgr_bool cellul_agree_ci(const CellulCtx *c)
{
    const size_t nw = mmgr_scrut_words(c->cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word keep = mmgr_scrut_tail_mask(c->cap, wi);
        const mmgr_scrut_word wa = scrut.load(c->s + at);
        const mmgr_scrut_word wb = scrut.load(c->t + at);
        const mmgr_scrut_word z = (mmgr_scrut_word)(scrut.has_zero(wa) & keep);
        const mmgr_scrut_word x =
            (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(scrut.xor_(wa, wb, MMGR_TRUE))) & keep);

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? scrut.zero_lane(z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? scrut.zero_lane(x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(c->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return (mmgr_bool)(c->end_wins != 0);
}

#ifndef MMGR_FAM_MIN_RUN
#define MMGR_FAM_MIN_RUN 0u
#endif

#ifndef MMGR_SIEVE_ROWS
#define MMGR_SIEVE_ROWS 1u
#endif

/**
 * @brief Needle byte at @c k, folded if the search is case insensitive.
 * @param c The pick.
 * @return The byte to cost.
 *
 * A folded row matches both cases, so its frequency is the sum of the two. Cost the folded byte,
 * not the one that happens to be written.
 */
MMGR_INLINE uint8_t cellul_ancorae_fold(const CellulCtx *c)
{
    const uint8_t b = (uint8_t)c->t[c->k];

    if (c->ci && (b >= (uint8_t)'A') && (b <= (uint8_t)'Z'))
    {
        return (uint8_t)(b | 0x20u);
    }
    return b;
}

/**
 * @brief The rarest bytes of the needle, by offset, rarest first.
 * @param c In/out. The pick. @c rows takes at least MMGR_SIEVE_ROWS entries.
 * @return How many rows were filled.
 *
 * One decision, at entry, amortized over the whole haystack. The needle is a handful of bytes and
 * the haystack is everything.
 *
 * Selection sort because the array is at most MMGR_SIEVE_ROWS long. Anything cleverer costs more
 * than it saves at that size.
 *
 * Offsets cap at MMGR_SWAR_BYTES so a row's load stays within one word of the candidate, which is
 * what bounds how far past the cap the scan can read.
 */
MMGR_INLINE size_t cellul_pick_rows(CellulCtx *c)
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
            /* GCOVR_EXCL_START - MMGR_SIEVE_ROWS is 1, so r is 0 on the only pass and there is no
               earlier row to have taken this position. Measured: four rows lost to newlib on three
               of four needles, which is why the default is one. Raise MMGR_SIEVE_ROWS and this is
               live again. */
            for (size_t q = 0; q < r; ++q)
            {
                if (c->rows[q] == k)
                {
                    taken = 1;
                }
            }
            /* GCOVR_EXCL_STOP */
            c->k = k;
            const uint8_t cost = mmgr_ancorae_impensa(cellul_ancorae_fold(c));
            if (!taken && (cost < best_cost)) /* GCOVR_EXCL_BR_LINE */
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
 * @brief Copy a string, always terminated.
 * @param c The copy. @c cap is the size of @c dst including the terminator.
 * @return Length written.
 */
MMGR_INLINE size_t cellul_copy(CellulCtx *c)
{
    if (c->cap == 0)
    {
        return 0;
    }

    const char *const src = c->s;
    c->s = src;
    c->cap -= 1u;
    const size_t n = cellul_len(c);

    proxim.read(c->dst, src, n);
    c->dst[n] = '\0';
    return n;
}

#if MMGR_FAM_MIN_RUN != 0u
/**
 * @brief Longest stretch of the needle whose bytes share one ASCII family.
 * @param c In/out. The pick. @c off takes where that stretch starts.
 * @return Its length.
 *
 * Homogeneity is what would make a family filter cheap: when every byte of a stretch wants the same
 * family, one comparison covers the stretch and a run reduction turns it into "does any lane begin
 * a window of that shape".
 */
MMGR_INLINE size_t cellul_fam_run(CellulCtx *c)
{
    const size_t lim = (c->nlen > MMGR_SWAR_BYTES) ? MMGR_SWAR_BYTES : c->nlen;
    size_t best_off = 0;
    size_t best_len = 1;
    size_t cur_off = 0;
    size_t cur_len = 1;

    for (size_t k = 1; k < lim; ++k)
    {
        if (((uint8_t)c->t[k] & c->fmask) == ((uint8_t)c->t[k - 1u] & c->fmask))
        {
            ++cur_len;
        }
        else
        {
            cur_off = k;
            cur_len = 1u;
        }
        if (cur_len > best_len)
        {
            best_len = cur_len;
            best_off = cur_off;
        }
    }
    *c->off = best_off;
    return best_len;
}
#endif

/**
 * @brief Where two strings first differ, folding or not as @c ci says.
 * @param c The compare.
 * @return Offset of the first difference, or @c cap if they agree that far.
 */
MMGR_INLINE size_t cellul_diff(const CellulCtx *c)
{
    if (c->ci)
    {
        return cellul_diff_ci(c);
    }
    return cellul_diff_cs(c);
}

/**
 * @brief Find @c t in @c s.
 * @param c In/out. The scan. @c ci must be a constant here.
 * @return Pointer to the first match, or NULL.
 *
 * Three stages, cheapest and least selective first, so the expensive ones only run on what the
 * cheap ones could not rule out:
 *
 *   family    optional. One load, one compare and a doubling run reduction says whether any window
 *             in this word has the right shape. Off by default - see MMGR_FAM_MIN_RUN.
 *   rows      the rarest bytes of the needle, each loaded at its own offset so lane k is already
 *             the byte that row has to match. Rows stacked and ANDed; the spacing is enforced by
 *             where each row was read from, not by a comparison.
 *   verify    one xor against the needle's first word for whatever survives, and a diff past that
 *             only when the needle is longer than a word.
 *
 * This replaced find_cs and find_ci - 590 lines that were 92 percent the same text, each with a
 * w = 8/4/2/1 ladder, an alignment peel, three length-specific paths and a byte loop at each end.
 *
 * @c ci has to be a constant. Read out of the context as an ordinary value, the eq in the row loop
 * carries a live branch on it, once per row per word. Measured, that cost 1.349 cycles per byte
 * against 0.887. cellul_find below is what makes it constant: two call sites, each passing a
 * literal, and this body always_inline so each expansion folds the field away.
 */
MMGR_INLINE const char *cellul_find_core(CellulCtx *c, mmgr_bool ci)
{
    const char *const hay = c->s;
    const char *const needle = c->t;
    const size_t read_cap = c->cap;

    c->s = needle;
    c->cap = c->t_cap;
    const size_t nlen = cellul_len(c);
    c->s = hay;
    c->cap = read_cap;

    if (nlen == 0u)
    {
        return hay;
    }
    if (nlen > read_cap)
    {
        return NULL;
    }

    size_t rows[MMGR_SIEVE_ROWS];
    c->nlen = nlen;
    c->rows = rows;
    c->ci = ci;
    const size_t nrows = cellul_pick_rows(c);

#if MMGR_FAM_MIN_RUN != 0u
    const unsigned fmask = ci ? MMGR_FAM_CI : MMGR_FAM_CS;
    size_t fam_off = 0;
    c->fmask = fmask;
    c->off = &fam_off;
    const size_t fam_len = cellul_fam_run(c);
    const uint8_t fam_want = (uint8_t)((uint8_t)needle[fam_off] & fmask);
    const mmgr_scrut_word fam_edge = mmgr_scrut_run_edge(fam_len);
    const int use_fam = (fam_len >= MMGR_FAM_MIN_RUN);
#else
    const int use_fam = 0;
#endif

    const size_t take = (nlen > MMGR_SWAR_BYTES) ? MMGR_SWAR_BYTES : nlen;
    const mmgr_scrut_word nmask = mmgr_scrut_bytes_below(take);
    const mmgr_scrut_word nraw = (mmgr_scrut_word)(scrut.load(needle) & nmask);
    const mmgr_scrut_word nword = ci ? (mmgr_scrut_word)(mmgr_scrut_fold_lower(nraw) & nmask) : nraw;

    const size_t starts = read_cap - nlen + 1u;

    /* Furthest byte, one past, that any load in an iteration at `at` reaches:
     *
     *   the terminator load        at + W
     *   the anchor load            at + row + W        for the highest row
     *   the verify load            at + (W - 1) + W    a candidate in the last lane
     *
     * All three are known here, before a byte is touched, because the rows are picked and the word
     * is a compile time width. The loop runs while at + reach is inside the cap, so every load it
     * makes is a load of bytes the caller said were there. */
    size_t maxrow = rows[0];
    /* GCOVR_EXCL_START - nrows is 1 while MMGR_SIEVE_ROWS is 1, so row 0 is the whole sieve and
       there is no second row to be further out than it. */
    for (size_t r = 1; r < nrows; ++r)
    {
        if (rows[r] > maxrow)
        {
            maxrow = rows[r];
        }
    }
    /* GCOVR_EXCL_STOP */
#if MMGR_FAM_MIN_RUN != 0u
    if (use_fam && (fam_off > maxrow))
    {
        maxrow = fam_off;
    }
#endif
    /* A candidate in the last lane of the pass sits at at + W - 1. Verifying it reads `take`
     * bytes as a word and then, when the needle is longer than that, hands the remainder to diff,
     * which rounds its own read up to a word. That is the furthest anything in a pass goes. */
    const size_t tail = (nlen > take) ? (mmgr_scrut_words(nlen - take) * MMGR_SWAR_BYTES) : 0u;
    const size_t verify_reach = (MMGR_SWAR_BYTES - 1u) + take + tail;
    const size_t ancorae_reach = maxrow + MMGR_SWAR_BYTES;
    const size_t reach = (ancorae_reach > verify_reach) ? ancorae_reach : verify_reach;

    size_t safe = (read_cap >= reach) ? ((read_cap - reach) + 1u) : 0u;
    /* GCOVR_EXCL_START - reach is never less than nlen, so safe is never more than starts and the
       clamp has nothing to do. Below a word, reach is at least (W - 1) + nlen; above one it is at
       least (W - 1) + W + (nlen - W), which is nlen + W - 1. Kept because safe and starts are
       derived from different quantities and reading the loop should not require proving they
       cannot cross. */
    if (safe > starts)
    {
        safe = starts;
    }
    /* GCOVR_EXCL_STOP */
    /* Whole words only. What is left over is the epilogue's, so the loop needs no tail mask. */
    const size_t nw = safe / MMGR_SWAR_BYTES;

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word end = scrut.has_zero(scrut.load(hay + at));
        mmgr_scrut_word m;

#if MMGR_FAM_MIN_RUN != 0u
        if (use_fam)
        {
            m = mmgr_scrut_run(mmgr_scrut_fam_eq(scrut.load(hay + at + fam_off), fmask, fam_want), fam_len);
            m |= fam_edge;
            if (m == 0)
            {
                continue;
            }
            for (size_t r = 0; r < nrows; ++r)
            {
                m &= scrut.eq(scrut.load(hay + at + rows[r]), (uint8_t)needle[rows[r]], ci);
            }
        }
        else
#else
        (void)use_fam;
#endif
        {
            m = scrut.eq(scrut.load(hay + at + rows[0]), (uint8_t)needle[rows[0]], ci);
            /* GCOVR_EXCL_START - same reason: nrows is 1 while MMGR_SIEVE_ROWS is 1, so row 0 is
               the whole sieve and there is no second row to fold in. */
            for (size_t r = 1; r < nrows; ++r)
            {
                m &= scrut.eq(scrut.load(hay + at + rows[r]), (uint8_t)needle[rows[r]], ci);
            }
            /* GCOVR_EXCL_STOP */
        }

        if (end != 0)
        {
            m &= mmgr_scrut_lanes_before(end);
        }

        while (m != 0)
        {
            const size_t k = at + scrut.zero_lane(m);
            const mmgr_scrut_word cw = scrut.load(hay + k);

            const mmgr_scrut_word syn =
                (mmgr_scrut_word)(((!ci || (mmgr_scrut_any_upper(cw) == 0)) ? (mmgr_scrut_word)(cw ^ nword)
                                                                           : scrut.xor_(cw, nword, MMGR_TRUE)) &
                                  nmask);

            if (syn == 0)
            {
                if (take == nlen)
                {
                    return hay + k;
                }
                c->s = hay + k + take;
                c->t = needle + take;
                c->cap = nlen - take;
                const size_t d = ci ? cellul_diff_ci(c) : cellul_diff_cs(c);
                c->s = hay;
                c->t = needle;
                c->cap = read_cap;
                if (d == (nlen - take))
                {
                    return hay + k;
                }
            }
            m = mmgr_scrut_drop_first(m);
        }
        if (end != 0)
        {
            return NULL;
        }
    }

    /* The candidates the word loop could not reach without loading bytes past the cap. At most
     * `reach` of them, which is two words and change, so this is a short walk off the end of a
     * long scan and the whole of a scan too short to have had a word loop at all. Every read here
     * is a single byte inside the candidate's own window, and a candidate window ends at
     * read_cap - 1 by the definition of starts, so there is nothing to bound that is not already
     * bounded. */
    for (size_t k = nw * MMGR_SWAR_BYTES; k < starts; ++k)
    {
        if (hay[k] == '\0')
        {
            return NULL;
        }

        size_t i = 0;
        while (i < nlen)
        {
            const unsigned char h = (unsigned char)hay[k + i];

            c->ca = (unsigned char)needle[i];
            c->cb = h;
            c->end_wins = 0;
            if ((h == 0u) || ((ci ? cellul_step_byte_ci(c) : cellul_step_byte_cs(c)) == MMGR_SWAR_NO))
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
 * @brief Find @c t in @c s.
 * @param c In/out. The scan.
 * @return Pointer to the first match, or NULL.
 *
 * The source says it once and the compiler emits it twice. cellul_find_core is always_inline and
 * both calls pass a literal, so each expansion folds the fold away and neither has a branch left.
 */
MMGR_INLINE const char *cellul_find(CellulCtx *c)
{
    if (c->ci)
    {
        return cellul_find_core(c, MMGR_TRUE);
    }
    return cellul_find_core(c, MMGR_FALSE);
}

/**
 * @brief Is @c t in @c s.
 * @param c In/out. The scan.
 * @return MMGR_TRUE if found.
 */
MMGR_INLINE mmgr_bool cellul_has(CellulCtx *c)
{
    return (mmgr_bool)(cellul_find(c) != NULL);
}

/**
 * @brief Whole string equality.
 * @param c The compare.
 * @return MMGR_TRUE if equal.
 */
MMGR_INLINE mmgr_bool cellul_eq(CellulCtx *c)
{
    c->end_wins = 0;
    if (c->ci)
    {
        return cellul_agree_ci(c);
    }
    return cellul_agree_cs(c);
}

/**
 * @brief Does @c s begin with @c t.
 * @param c In/out. The compare.
 * @return MMGR_TRUE if it does.
 *
 * agree walks the pattern, so the two swap: the prefix is what ends the compare.
 */
MMGR_INLINE mmgr_bool cellul_starts(CellulCtx *c)
{
    const char *const subject = c->s;

    c->s = c->t;
    c->t = subject;
    c->end_wins = 1;

    const mmgr_bool r = c->ci ? cellul_agree_ci(c) : cellul_agree_cs(c);

    c->t = c->s;
    c->s = subject;
    return r;
}

/**
 * @brief Advance one word of a prefix compare, folding or not as @c ci says.
 * @param c The step.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int cellul_step_word(const CellulCtx *c)
{
    if (c->ci)
    {
        return cellul_step_word_ci(c);
    }
    return cellul_step_word_cs(c);
}

/**
 * @brief Advance one byte of a prefix compare, folding or not as @c ci says.
 * @param c The step.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int cellul_step_byte(const CellulCtx *c)
{
    if (c->ci)
    {
        return cellul_step_byte_ci(c);
    }
    return cellul_step_byte_cs(c);
}

/**
 * @brief Is @p ch whitespace.
 * @param ch The byte.
 * @return MMGR_TRUE if it is.
 *
 * No context. One byte in, one answer out - a struct to carry it would be a store and a load to
 * reach what was already in a register, and the unnamed fields of a designated initializer are
 * zeroed first.
 */
MMGR_INLINE mmgr_bool cellul_is_ws(char ch)
{
    return (ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r') || (ch == '\f') || (ch == '\v');
}

/**
 * @brief Is @p ch a decimal digit.
 * @param ch The byte.
 * @return MMGR_TRUE if it is.
 */
MMGR_INLINE mmgr_bool cellul_is_digit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

/**
 * @brief Parse a signed decimal.
 * @param c The parse.
 * @return The value, or 0 if nothing parsed.
 */
MMGR_INLINE long cellul_to_long(const CellulCtx *c)
{
    const char *p = c->s;

    while (cellul_is_ws(*p))
    {
        p++;
    }

    mmgr_bool neg = MMGR_FALSE;
    if ((*p == '+') || (*p == '-'))
    {
        neg = (*p++ == '-');
    }

    const char *const ds = p;
    unsigned long v = 0;
    while (cellul_is_digit(*p))
    {
        v = (v * 10UL) + (unsigned long)(*p++ - '0');
    }

    if (c->end != NULL)
    {
        *c->end = c->s;
        if (p != ds)
        {
            *c->end = p;
        }
    }
    if (neg)
    {
        return (long)(0UL - v);
    }
    return (long)v;
}

/**
 * @brief Parse an unsigned decimal.
 * @param c The parse.
 * @return The value, or 0 if nothing parsed.
 */
MMGR_INLINE unsigned long cellul_to_ulong(const CellulCtx *c)
{
    const char *p = c->s;

    while (cellul_is_ws(*p))
    {
        p++;
    }
    if (*p == '+')
    {
        p++;
    }

    const char *const ds = p;
    unsigned long v = 0;
    while (cellul_is_digit(*p))
    {
        v = (v * 10UL) + (unsigned long)(*p++ - '0');
    }

    if (c->end != NULL)
    {
        *c->end = c->s;
        if (p != ds)
        {
            *c->end = p;
        }
    }
    return v;
}

/**
 * @brief Consume an exponent.
 * @param c In/out. The parse. @c p is the cursor, left where it was when there is none to take.
 *          @c out is untouched when there is none.
 *
 * An e with no digits behind it is not an exponent, it is the byte that ended the number. The
 * cursor is put back on it, because what the caller reads to find out whether the whole string
 * parsed is where the cursor stopped.
 */
MMGR_INLINE void cellul_expo(CellulCtx *c)
{
    const char *const mark = *c->p;

    (*c->p)++;

    mmgr_bool eneg = MMGR_FALSE;
    if ((**c->p == '+') || (**c->p == '-'))
    {
        eneg = (*(*c->p)++ == '-');
    }
    if (!cellul_is_digit(**c->p))
    {
        *c->p = mark;
        return;
    }

    int ex = 0;
    while (cellul_is_digit(**c->p))
    {
        if (ex < MMGR_MUTO_EXP_LIMIT)
        {
            ex = (ex * 10) + (**c->p - '0');
        }
        (*c->p)++;
    }
    *c->out = eneg ? -ex : ex;
}

/**
 * @brief Parse a double.
 * @param c In/out. The parse.
 * @return The value, or 0 if nothing parsed.
 */
MMGR_INLINE double cellul_to_double(CellulCtx *c)
{
    const char *p = c->s;

    while (cellul_is_ws(*p))
    {
        p++;
    }

    mmgr_bool neg = MMGR_FALSE;
    if ((*p == '+') || (*p == '-'))
    {
        neg = (*p++ == '-');
    }

    /* The digits are an integer and the point is a count of how far it has to come back down.
       Nothing is scaled until every digit is in, so there is one rounding at the end rather than
       three at every step. */
    mmgr_bool any = MMGR_FALSE;
    mmgr_u64 mant = 0;
    int drop = 0;
    int over = 0;
    int lost = 0;

    while (cellul_is_digit(*p))
    {
        if (!mmgr_muto_take(&mant, *p))
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
            if (mmgr_muto_take(&mant, *p))
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

    int ex = 0;
    if (any && ((*p == 'e') || (*p == 'E')))
    {
        c->p = &p;
        c->out = &ex;
        cellul_expo(c);
    }

    const double val = mmgr_muto_scale(mant, ex + over - drop, lost, neg);

    if (c->end != NULL)
    {
        *c->end = c->s;
        if (any)
        {
            *c->end = p;
        }
    }
    return val;
}

/**
 * @brief Parse a float.
 * @param c In/out. The parse.
 * @return The value, or 0 if nothing parsed.
 *
 * Rounds twice - once to a double and once down again - so a value on the boundary can land on the
 * wrong neighbour where one rounding would not.
 */
MMGR_INLINE float cellul_to_float(CellulCtx *c)
{
    return (float)cellul_to_double(c);
}

/* The namespace is a table of function pointers with the caller's argument lists in their types,
   so these are what it points at. Each builds the context and hands it to the body above.

   They are nameable rather than file local because a static const table in the header has to be
   able to point at them, and a static const table is what gcc devirtualizes. Through an extern one
   every call from another translation unit is a load of the table, a load of the entry, and an
   indirect call it cannot see through. */

size_t mmgr_cellul_len(const char *s, size_t nul_cap)
{
    return MMGR_CALL(cellul_len, CellulCtx, .s = s, .cap = nul_cap);
}

const char *mmgr_cellul_chr(const char *s, size_t nul_cap, uint8_t c)
{
    return MMGR_CALL(cellul_chr, CellulCtx, .s = s, .cap = nul_cap, .byte = c);
}

size_t mmgr_cellul_diff(const char *a, const char *b, size_t read_cap, mmgr_bool ci)
{
    return MMGR_CALL(cellul_diff, CellulCtx, .s = a, .t = b, .cap = read_cap, .ci = ci);
}

mmgr_bool mmgr_cellul_eq(const char *a, const char *b, size_t read_cap, mmgr_bool ci)
{
    return MMGR_CALL(cellul_eq, CellulCtx, .s = a, .t = b, .cap = read_cap, .ci = ci);
}

mmgr_bool mmgr_cellul_starts(const char *s, const char *pre, size_t read_cap, mmgr_bool ci)
{
    return MMGR_CALL(cellul_starts, CellulCtx, .s = s, .t = pre, .cap = read_cap, .ci = ci);
}

const char *mmgr_cellul_find(const char *hay, size_t read_cap, const char *needle, size_t needle_cap,
                                       mmgr_bool ci)
{
    return MMGR_CALL(cellul_find, CellulCtx, .s = hay, .cap = read_cap, .t = needle, .t_cap = needle_cap, .ci = ci);
}

mmgr_bool mmgr_cellul_has(const char *hay, size_t read_cap, const char *needle, size_t needle_cap,
                                    mmgr_bool ci)
{
    return MMGR_CALL(cellul_has, CellulCtx, .s = hay, .cap = read_cap, .t = needle, .t_cap = needle_cap, .ci = ci);
}

size_t mmgr_cellul_copy(char *dst, const char *src, size_t dst_cap)
{
    return MMGR_CALL(cellul_copy, CellulCtx, .dst = dst, .s = src, .cap = dst_cap);
}

int mmgr_cellul_step_word(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci, int end_wins)
{
    return MMGR_CALL(cellul_step_word, CellulCtx, .wa = wa, .wb = wb, .ci = ci, .end_wins = end_wins);
}

int mmgr_cellul_step_byte(unsigned char ca, unsigned char cb, mmgr_bool ci, int end_wins)
{
    return MMGR_CALL(cellul_step_byte, CellulCtx, .ca = ca, .cb = cb, .ci = ci, .end_wins = end_wins);
}

mmgr_bool mmgr_cellul_ws(char c)
{
    return cellul_is_ws(c);
}

mmgr_bool mmgr_cellul_digit(char c)
{
    return cellul_is_digit(c);
}

long mmgr_cellul_to_long(const char *s, const char **end)
{
    return MMGR_CALL(cellul_to_long, CellulCtx, .s = s, .end = end);
}

unsigned long mmgr_cellul_to_ulong(const char *s, const char **end)
{
    return MMGR_CALL(cellul_to_ulong, CellulCtx, .s = s, .end = end);
}

double mmgr_cellul_to_double(const char *s, const char **end)
{
    return MMGR_CALL(cellul_to_double, CellulCtx, .s = s, .end = end);
}

float mmgr_cellul_to_float(const char *s, const char **end)
{
    return MMGR_CALL(cellul_to_float, CellulCtx, .s = s, .end = end);
}
