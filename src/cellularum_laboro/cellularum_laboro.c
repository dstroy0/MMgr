// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "cellularum_laboro/cellularum_laboro.h"
#include "anchor_cost/anchor_cost.h"
#include "ascii_mask/ascii_mask.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @file cellularum_laboro.c
 * @brief Bounded string work. Every entry takes a read cap and never runs past it.
 *
 * Scans count words, not bytes. No alignment peel and no byte remainder: an unaligned load is the
 * same instruction as an aligned one, and the last word is masked rather than walked.
 *
 * Case folding is a compile time fact at every call site. The _cs and _ci pairs below exist so the
 * fold is constant inside the loop, not so the source says everything twice.
 */

/**
 * @brief Length up to the terminator.
 * @param s String.
 * @param nul_cap How far it may read.
 * @return Offset of the terminator, or @p nul_cap if there is none in range.
 */
static inline size_t MMGR_UNUSED len(const char *s, size_t nul_cap)
{
    const size_t nw = mmgr_scrut_words(nul_cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        mmgr_scrut_word m = scrut.has_zero(scrut.load(s + at)) & mmgr_scrut_tail_mask(nul_cap, wi);
        if (m != 0)
        {
            return at + scrut.zero_lane(m);
        }
    }
    return nul_cap;
}

/**
 * @brief First @p c at or before the terminator.
 * @param s String.
 * @param nul_cap How far it may read.
 * @param c Byte to find.
 * @return Pointer to it, or NULL.
 *
 * One pass. Both questions are masks over the same loaded word, so there is no reason to walk the
 * string twice to ask them. lanes_before drops any hit past the terminator with no compare.
 *
 * c == 0 is the one case the mask cannot answer, because then both masks are the same mask. strchr
 * is defined to find the terminator, so that is len.
 */
static inline const char *chr(const char *s, size_t nul_cap, uint8_t c)
{
    if (c == 0u)
    {
        return s + len(s, nul_cap);
    }

    const size_t nw = mmgr_scrut_words(nul_cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        mmgr_scrut_word w = scrut.load(s + at);
        mmgr_scrut_word keep = mmgr_scrut_tail_mask(nul_cap, wi);
        mmgr_scrut_word end = (mmgr_scrut_word)(scrut.has_zero(w) & keep);
        mmgr_scrut_word hit = (mmgr_scrut_word)(scrut.eq(w, c, MMGR_FALSE) & keep & mmgr_scrut_lanes_before(end));

        if (hit != 0)
        {
            return s + at + scrut.zero_lane(hit);
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
 * @param a First string.
 * @param b Second string.
 * @param read_cap How far it may read.
 * @return Offset of the first difference, or @p read_cap if they agree that far.
 */
static inline size_t MMGR_UNUSED diff_cs(const char *a, const char *b, size_t read_cap)
{
    const size_t nw = mmgr_scrut_words(read_cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        mmgr_scrut_word d = scrut.load(a + at) ^ scrut.load(b + at);
        mmgr_scrut_word m =
            (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(d)) & mmgr_scrut_tail_mask(read_cap, wi));
        if (m != 0)
        {
            return at + scrut.zero_lane(m);
        }
    }
    return read_cap;
}

/**
 * @brief Where two strings first differ, ignoring case.
 * @param a First string.
 * @param b Second string.
 * @param read_cap How far it may read.
 * @return Offset of the first difference, or @p read_cap if they agree that far.
 */
static inline size_t MMGR_UNUSED diff_ci(const char *a, const char *b, size_t read_cap)
{
    const size_t nw = mmgr_scrut_words(read_cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        mmgr_scrut_word d = scrut.xor_(scrut.load(a + at), scrut.load(b + at), MMGR_TRUE);
        mmgr_scrut_word m =
            (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(d)) & mmgr_scrut_tail_mask(read_cap, wi));
        if (m != 0)
        {
            return at + scrut.zero_lane(m);
        }
    }
    return read_cap;
}

/**
 * @brief Advance one word of a prefix compare.
 * @param wa Word from the pattern.
 * @param wb Word from the subject.
 * @param end_wins Whether the pattern ending counts as a match.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int step_word_cs(mmgr_scrut_word wa, mmgr_scrut_word wb, int end_wins)
{
    mmgr_scrut_word x = wa ^ wb;
    mmgr_scrut_word z = scrut.has_zero(wa);
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
    if (end_wins)
    {
        if (el <= dl)
        {
            return MMGR_SWAR_YES;
        }
        return MMGR_SWAR_NO;
    }
    if (el < dl)
    {
        return MMGR_SWAR_YES;
    }
    return MMGR_SWAR_NO;
}

/**
 * @brief Advance one word of a prefix compare, ignoring case.
 * @param wa Word from the pattern.
 * @param wb Word from the subject.
 * @param end_wins Whether the pattern ending counts as a match.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int step_word_ci(mmgr_scrut_word wa, mmgr_scrut_word wb, int end_wins)
{
    mmgr_scrut_word x = scrut.xor_(wa, wb, MMGR_TRUE);
    mmgr_scrut_word z = scrut.has_zero(wa);
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
    if (end_wins)
    {
        if (el <= dl)
        {
            return MMGR_SWAR_YES;
        }
        return MMGR_SWAR_NO;
    }
    if (el < dl)
    {
        return MMGR_SWAR_YES;
    }
    return MMGR_SWAR_NO;
}

/**
 * @brief Advance one byte of a prefix compare.
 * @param ca Byte from the pattern.
 * @param cb Byte from the subject.
 * @param end_wins Whether the pattern ending counts as a match.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int step_byte_cs(unsigned char ca, unsigned char cb, int end_wins)
{
    if (ca == 0)
    {
        if (ca == cb)
        {
            return MMGR_SWAR_YES;
        }
        if (end_wins != 0)
        {
            return MMGR_SWAR_YES;
        }
        return MMGR_SWAR_NO;
    }
    if (ca != cb)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

/**
 * @brief Advance one byte of a prefix compare, ignoring case.
 * @param ca Byte from the pattern.
 * @param cb Byte from the subject.
 * @param end_wins Whether the pattern ending counts as a match.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE int step_byte_ci(unsigned char ca, unsigned char cb, int end_wins)
{
    mmgr_scrut_word d = scrut.xor_((mmgr_scrut_word)ca, (mmgr_scrut_word)cb, MMGR_TRUE);
    if (ca == 0)
    {
        if (d == 0)
        {
            return MMGR_SWAR_YES;
        }
        if (end_wins != 0)
        {
            return MMGR_SWAR_YES;
        }
        return MMGR_SWAR_NO;
    }
    if (d != 0)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

/**
 * @brief Does @p b agree with @p a up to a's terminator.
 * @param a Pattern.
 * @param b Subject.
 * @param read_cap How far it may read.
 * @param end_wins What happens when the terminator and the difference land in the same lane.
 * @return MMGR_TRUE if they agree.
 *
 * Two events race in each word - a ends, or the two differ - and whichever comes first in address
 * order decides.
 *
 * Comparing lane indices rather than trailing bit masks is what removes the endian branch. A
 * trailing bit mask means below, and below is the wrong direction on a big endian load.
 */
static inline mmgr_bool MMGR_UNUSED agree_cs(const char *a, const char *b, size_t read_cap, int end_wins)
{
    const size_t nw = mmgr_scrut_words(read_cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word keep = mmgr_scrut_tail_mask(read_cap, wi);
        const mmgr_scrut_word wa = scrut.load(a + at);
        const mmgr_scrut_word wb = scrut.load(b + at);
        const mmgr_scrut_word z = (mmgr_scrut_word)(scrut.has_zero(wa) & keep);
        const mmgr_scrut_word x = (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(wa ^ wb)) & keep);

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? scrut.zero_lane(z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? scrut.zero_lane(x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return (mmgr_bool)(end_wins != 0);
}

/**
 * @brief Does @p b agree with @p a up to a's terminator, ignoring case.
 * @param a Pattern.
 * @param b Subject.
 * @param read_cap How far it may read.
 * @param end_wins What happens when the terminator and the difference land in the same lane.
 * @return MMGR_TRUE if they agree.
 */
static inline mmgr_bool MMGR_UNUSED agree_ci(const char *a, const char *b, size_t read_cap, int end_wins)
{
    const size_t nw = mmgr_scrut_words(read_cap);

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_scrut_word keep = mmgr_scrut_tail_mask(read_cap, wi);
        const mmgr_scrut_word wa = scrut.load(a + at);
        const mmgr_scrut_word wb = scrut.load(b + at);
        const mmgr_scrut_word z = (mmgr_scrut_word)(scrut.has_zero(wa) & keep);
        const mmgr_scrut_word x =
            (mmgr_scrut_word)((MMGR_VERBUM_SCRUTOR_HIGH & ~scrut.has_zero(scrut.xor_(wa, wb, MMGR_TRUE))) & keep);

        if ((x | z) != 0)
        {
            const size_t lz = (z != 0) ? scrut.zero_lane(z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0) ? scrut.zero_lane(x) : MMGR_SWAR_BYTES;
            return (mmgr_bool)(end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return (mmgr_bool)(end_wins != 0);
}

/**
 * @brief Whole string equality.
 * @param a First string.
 * @param b Second string.
 * @param read_cap How far it may read.
 * @return MMGR_TRUE if equal.
 */
static inline mmgr_bool MMGR_UNUSED eq_cs(const char *a, const char *b, size_t read_cap)
{
    return agree_cs(a, b, read_cap, 0);
}
/**
 * @brief Whole string equality, ignoring case.
 * @param a First string.
 * @param b Second string.
 * @param read_cap How far it may read.
 * @return MMGR_TRUE if equal.
 */
static inline mmgr_bool MMGR_UNUSED eq_ci(const char *a, const char *b, size_t read_cap)
{
    return agree_ci(a, b, read_cap, 0);
}
/**
 * @brief Does @p s begin with @p pre.
 * @param s String.
 * @param pre Prefix.
 * @param read_cap How far it may read.
 * @return MMGR_TRUE if it does.
 */
static inline mmgr_bool MMGR_UNUSED starts_cs(const char *s, const char *pre, size_t read_cap)
{
    return agree_cs(pre, s, read_cap, 1);
}
/**
 * @brief Does @p s begin with @p pre, ignoring case.
 * @param s String.
 * @param pre Prefix.
 * @param read_cap How far it may read.
 * @return MMGR_TRUE if it does.
 */
static inline mmgr_bool MMGR_UNUSED starts_ci(const char *s, const char *pre, size_t read_cap)
{
    return agree_ci(pre, s, read_cap, 1);
}

#ifndef MMGR_FAM_MIN_RUN
#define MMGR_FAM_MIN_RUN 0u
#endif

#ifndef MMGR_SIEVE_ROWS
#define MMGR_SIEVE_ROWS 1u
#endif

/**
 * @brief Needle byte at @p k, folded if the search is case insensitive.
 * @param needle Needle.
 * @param k Offset.
 * @param ci Fold case.
 * @return The byte to cost.
 *
 * A folded row matches both cases, so its frequency is the sum of the two. Cost the folded byte,
 * not the one that happens to be written.
 */
MMGR_INLINE uint8_t anchor_fold(const char *needle, size_t k, mmgr_bool ci)
{
    uint8_t c = (uint8_t)needle[k];
    if (ci && c >= (uint8_t)'A' && c <= (uint8_t)'Z')
    {
        return (uint8_t)(c | 0x20u);
    }
    return c;
}

/**
 * @brief The rarest bytes of the needle, by offset, rarest first.
 * @param needle Needle.
 * @param nlen Needle length.
 * @param ci Fold case.
 * @param rows Out. At least MMGR_SIEVE_ROWS entries.
 * @return How many rows were filled.
 *
 * One decision, at entry, amortized over the whole haystack. The needle is a handful of bytes and
 * the haystack is everything.
 *
 * Selection sort because the array is at most MMGR_SIEVE_ROWS long. Anything cleverer costs more
 * than it saves at that size.
 *
 * Offsets cap at MMGR_SWAR_BYTES so a row's load stays within one word of the candidate, which is
 * what bounds how far past read_cap the scan can read.
 */
MMGR_INLINE size_t pick_rows(const char *needle, size_t nlen, mmgr_bool ci, size_t *rows)
{
    size_t limit = nlen > MMGR_SWAR_BYTES ? MMGR_SWAR_BYTES : nlen;
    size_t want = limit > MMGR_SIEVE_ROWS ? MMGR_SIEVE_ROWS : limit;

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
                if (rows[q] == k)
                {
                    taken = 1;
                }
            }
            /* GCOVR_EXCL_STOP */
            if (!taken && mmgr_anchor_cost[anchor_fold(needle, k, ci)] < best_cost) /* GCOVR_EXCL_BR_LINE */
            {
                best_cost = mmgr_anchor_cost[anchor_fold(needle, k, ci)];
                best = k;
            }
        }
        rows[r] = best;
    }
    return want;
}

/**
 * @brief Copy a string, always terminated.
 * @param dst Destination.
 * @param src Source.
 * @param dst_cap Size of @p dst including the terminator.
 * @return Length written.
 */
static inline size_t MMGR_UNUSED copy(char *dst, const char *src, size_t dst_cap)
{
    if (dst_cap == 0)
    {
        return 0;
    }

    size_t n = len(src, dst_cap - 1);
    proxim.read(dst, src, n);
    dst[n] = '\0';
    return n;
}

/**
 * @brief Where two strings first differ.
 * @param a First string.
 * @param b Second string.
 * @param read_cap How far it may read.
 * @param ci Fold case.
 * @return Offset of the first difference, or @p read_cap if they agree that far.
 */
static size_t MMGR_UNUSED diff(const char *a, const char *b, size_t read_cap, mmgr_bool ci)
{
    if (ci)
    {
        return diff_ci(a, b, read_cap);
    }
    return diff_cs(a, b, read_cap);
}

/**
 * @brief Whole string equality.
 * @param a First string.
 * @param b Second string.
 * @param read_cap How far it may read.
 * @param ci Fold case.
 * @return MMGR_TRUE if equal.
 */
static mmgr_bool MMGR_UNUSED eq(const char *a, const char *b, size_t read_cap, mmgr_bool ci)
{
    if (ci)
    {
        return eq_ci(a, b, read_cap);
    }
    return eq_cs(a, b, read_cap);
}

/**
 * @brief Does @p s begin with @p pre.
 * @param s String.
 * @param pre Prefix.
 * @param read_cap How far it may read.
 * @param ci Fold case.
 * @return MMGR_TRUE if it does.
 */
static mmgr_bool MMGR_UNUSED starts(const char *s, const char *pre, size_t read_cap, mmgr_bool ci)
{
    if (ci)
    {
        return starts_ci(s, pre, read_cap);
    }
    return starts_cs(s, pre, read_cap);
}

#if MMGR_FAM_MIN_RUN != 0u
/**
 * @brief Longest stretch of the needle whose bytes share one ASCII family.
 * @param needle Needle.
 * @param nlen Needle length.
 * @param fmask Family bits to test.
 * @param off Out. Where that stretch starts.
 * @return Its length.
 *
 * Homogeneity is what would make a family filter cheap: when every byte of a stretch wants the same
 * family, one comparison covers the stretch and a run reduction turns it into "does any lane begin
 * a window of that shape".
 */
MMGR_INLINE size_t fam_run(const char *needle, size_t nlen, unsigned fmask, size_t *off)
{
    const size_t lim = nlen > MMGR_SWAR_BYTES ? MMGR_SWAR_BYTES : nlen;
    size_t best_off = 0, best_len = 1, cur_off = 0, cur_len = 1;

    for (size_t k = 1; k < lim; ++k)
    {
        if (((uint8_t)needle[k] & fmask) == ((uint8_t)needle[k - 1u] & fmask))
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
    *off = best_off;
    return best_len;
}
#endif

/**
 * @brief Find @p needle in @p hay.
 * @param hay Haystack.
 * @param read_cap How far it may read.
 * @param needle Needle.
 * @param needle_cap How far the needle may be read.
 * @param ci Fold case. Must be a constant here.
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
 * @p ci has to be a constant. As an ordinary parameter the eq in the row loop carries a live branch
 * on it, once per row per word. Measured, that cost 1.349 cycles per byte against 0.887. find()
 * below is what makes it constant.
 */
MMGR_INLINE const char *find_core(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, mmgr_bool ci)
{
    const size_t nlen = len(needle, needle_cap);

    if (nlen == 0u)
    {
        return hay;
    }
    if (nlen > read_cap)
    {
        return NULL;
    }

    size_t rows[MMGR_SIEVE_ROWS];
    const size_t nrows = pick_rows(needle, nlen, ci, rows);

#if MMGR_FAM_MIN_RUN != 0u
    const unsigned fmask = ci ? MMGR_FAM_CI : MMGR_FAM_CS;
    size_t fam_off = 0;
    const size_t fam_len = fam_run(needle, nlen, fmask, &fam_off);
    const uint8_t fam_want = (uint8_t)((uint8_t)needle[fam_off] & fmask);
    const mmgr_scrut_word fam_edge = mmgr_scrut_run_edge(fam_len);
    const int use_fam = (fam_len >= MMGR_FAM_MIN_RUN);
#else
    const int use_fam = 0;
#endif

    const size_t take = nlen > MMGR_SWAR_BYTES ? MMGR_SWAR_BYTES : nlen;
    const mmgr_scrut_word nmask = mmgr_scrut_bytes_below(take);
    const mmgr_scrut_word nraw = (mmgr_scrut_word)(scrut.load(needle) & nmask);
    const mmgr_scrut_word nword = ci ? (mmgr_scrut_word)(mmgr_scrut_fold_lower(nraw) & nmask) : nraw;

    const size_t starts = read_cap - nlen + 1u;
    const size_t nw = mmgr_scrut_words(starts);

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
        if (wi + 1u == nw)
        {
            m &= mmgr_scrut_tail_mask(starts, wi);
        }

        while (m != 0)
        {
            const size_t k = at + scrut.zero_lane(m);
            const mmgr_scrut_word cw = scrut.load(hay + k);

            const mmgr_scrut_word syn =
                (mmgr_scrut_word)((!ci || mmgr_scrut_any_upper(cw) == 0 ? (mmgr_scrut_word)(cw ^ nword)
                                                                        : scrut.xor_(cw, nword, MMGR_TRUE)) &
                                  nmask);

            if (syn == 0 && (take == nlen || diff(hay + k + take, needle + take, nlen - take, ci) == nlen - take))
            {
                return hay + k;
            }
            m = mmgr_scrut_drop_first(m);
        }
        if (end != 0)
        {
            return NULL;
        }
    }
    return NULL;
}

/**
 * @brief Find @p needle in @p hay.
 * @param hay Haystack.
 * @param read_cap How far it may read.
 * @param needle Needle.
 * @param needle_cap How far the needle may be read.
 * @param ci Fold case.
 * @return Pointer to the first match, or NULL.
 *
 * The source says it once and the compiler emits it twice. find_core is always_inline and both
 * calls pass a constant, so each expansion folds @p ci away and neither has a branch left.
 */
static const char *find(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, mmgr_bool ci)
{
    if (ci)
    {
        return find_core(hay, read_cap, needle, needle_cap, MMGR_TRUE);
    }
    return find_core(hay, read_cap, needle, needle_cap, MMGR_FALSE);
}

/**
 * @brief Is @p needle in @p hay.
 * @param hay Haystack.
 * @param read_cap How far it may read.
 * @param needle Needle.
 * @param needle_cap How far the needle may be read.
 * @param ci Fold case.
 * @return MMGR_TRUE if found.
 */
static mmgr_bool MMGR_UNUSED has(const char *hay, size_t read_cap, const char *needle, size_t needle_cap, mmgr_bool ci)
{
    return (mmgr_bool)(find(hay, read_cap, needle, needle_cap, ci) != NULL);
}

/**
 * @brief Advance one word of a prefix compare.
 * @param wa Word from the pattern.
 * @param wb Word from the subject.
 * @param ci Fold case.
 * @param end_wins Whether the pattern ending counts as a match.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
static int MMGR_UNUSED step_word(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci, int end_wins)
{
    if (ci)
    {
        return step_word_ci(wa, wb, end_wins);
    }
    return step_word_cs(wa, wb, end_wins);
}

/**
 * @brief Advance one byte of a prefix compare.
 * @param ca Byte from the pattern.
 * @param cb Byte from the subject.
 * @param ci Fold case.
 * @param end_wins Whether the pattern ending counts as a match.
 * @return MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
static int MMGR_UNUSED step_byte(unsigned char ca, unsigned char cb, mmgr_bool ci, int end_wins)
{
    if (ci)
    {
        return step_byte_ci(ca, cb, end_wins);
    }
    return step_byte_cs(ca, cb, end_wins);
}

/**
 * @brief Is @p c whitespace.
 * @param c Byte.
 * @return MMGR_TRUE if it is.
 *
 * Was six compares and a branch. One shift and one and now.
 */
static mmgr_bool MMGR_UNUSED ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/**
 * @brief Is @p c a decimal digit.
 * @param c Byte.
 * @return MMGR_TRUE if it is.
 */
static mmgr_bool MMGR_UNUSED digit(char c)
{
    return c >= '0' && c <= '9';
}

/**
 * @brief Parse a signed decimal.
 * @param s String.
 * @param end Out. Where parsing stopped. May be NULL.
 * @return The value, or 0 if nothing parsed.
 */
static long MMGR_UNUSED to_long(const char *s, const char **end)
{
    const char *p = s;
    while (ws(*p))
    {
        p++;
    }
    mmgr_bool neg = MMGR_FALSE;
    if (*p == '+' || *p == '-')
    {
        neg = (*p++ == '-');
    }
    const char *ds = p;
    unsigned long v = 0;
    while (digit(*p))
    {
        v = v * 10UL + (unsigned long)(*p++ - '0');
    }
    if (end)
    {
        *end = s;
        if (p != ds)
        {
            *end = p;
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
 * @param s String.
 * @param end Out. Where parsing stopped. May be NULL.
 * @return The value, or 0 if nothing parsed.
 */
static unsigned long MMGR_UNUSED to_ulong(const char *s, const char **end)
{
    const char *p = s;
    while (ws(*p))
    {
        p++;
    }
    if (*p == '+')
    {
        p++;
    }
    const char *ds = p;
    unsigned long v = 0;
    while (digit(*p))
    {
        v = v * 10UL + (unsigned long)(*p++ - '0');
    }
    if (end)
    {
        *end = s;
        if (p != ds)
        {
            *end = p;
        }
    }
    return v;
}

/**
 * @brief Consume a fractional part and fold it into @p val.
 * @param p In/out. Cursor.
 * @param val In/out. Running value.
 * @param any Out. Set when at least one digit was consumed.
 */
static void MMGR_UNUSED frac(const char **p, double *val, mmgr_bool *any)
{
    (*p)++;
    double scale = 1.0;
    while (digit(**p))
    {
        scale *= 10.0;
        *val += (double)(*(*p)++ - '0') / scale;
        *any = MMGR_TRUE;
    }
}

/**
 * @brief Consume an exponent and apply it to @p val.
 * @param p In/out. Cursor.
 * @param val In/out. Running value.
 */
static void MMGR_UNUSED expo(const char **p, double *val)
{
    (*p)++;
    mmgr_bool eneg = MMGR_FALSE;
    if (**p == '+' || **p == '-')
    {
        eneg = (*(*p)++ == '-');
    }
    int ex = 0;
    while (digit(**p))
    {
        if (ex < 400)
        {
            ex = ex * 10 + (**p - '0');
        }
        (*p)++;
    }
    double m = 1.0;
    for (int k = 0; k < ex; k++)
    {
        m *= 10.0;
    }
    if (eneg)
    {
        *val = *val / m;
        return;
    }
    *val = *val * m;
}

/**
 * @brief Parse a double.
 * @param s String.
 * @param end Out. Where parsing stopped. May be NULL.
 * @return The value, or 0 if nothing parsed.
 */
static double MMGR_UNUSED to_double(const char *s, const char **end)
{
    const char *p = s;
    while (ws(*p))
    {
        p++;
    }
    mmgr_bool neg = MMGR_FALSE;
    if (*p == '+' || *p == '-')
    {
        neg = (*p++ == '-');
    }
    mmgr_bool any = MMGR_FALSE;
    double val = 0.0;
    while (digit(*p))
    {
        val = val * 10.0 + (*p++ - '0');
        any = MMGR_TRUE;
    }
    if (*p == '.')
    {
        frac(&p, &val, &any);
    }
    if (any && (*p == 'e' || *p == 'E'))
    {
        expo(&p, &val);
    }
    if (end)
    {
        *end = s;
        if (any)
        {
            *end = p;
        }
    }
    if (neg)
    {
        return -val;
    }
    return val;
}

/**
 * @brief Parse a float.
 * @param s String.
 * @param end Out. Where parsing stopped. May be NULL.
 * @return The value, or 0 if nothing parsed.
 */
static float MMGR_UNUSED to_float(const char *s, const char **end)
{
    return (float)to_double(s, end);
}

#if defined(MMGR_ORACLE_LIBC) && MMGR_ORACLE_LIBC
#include "mmgr_oracle_libc.h"

/**
 * @brief Module namespace, pointed at libc where libc has an equivalent.
 *
 * Test only. diff, step_word and step_byte keep this library's own implementation because libc
 * has nothing to compare them against - see mmgr_oracle_libc.h.
 */
const CellularumLaboroNs cellul = {
    .len = mmgr_oracle_len,
    .diff = diff,
    .eq = mmgr_oracle_eq,
    .starts = mmgr_oracle_starts,
    .find = mmgr_oracle_find,
    .has = mmgr_oracle_has,
    .chr = mmgr_oracle_strchr,
    .copy = mmgr_oracle_copy,
    .step_word = step_word,
    .step_byte = step_byte,
    .ws = mmgr_oracle_ws,
    .digit = mmgr_oracle_digit,
    .to_long = mmgr_oracle_to_long,
    .to_ulong = mmgr_oracle_to_ulong,
    .to_double = mmgr_oracle_to_double,
    .to_float = mmgr_oracle_to_float,
};
#else
const CellularumLaboroNs cellul = {
    .len = len,
    .diff = diff,
    .eq = eq,
    .starts = starts,
    .find = find,
    .has = has,
    .chr = chr,
    .copy = copy,
    .step_word = step_word,
    .step_byte = step_byte,
    .ws = ws,
    .digit = digit,
    .to_long = to_long,
    .to_ulong = to_ulong,
    .to_double = to_double,
    .to_float = to_float,
};
#endif
