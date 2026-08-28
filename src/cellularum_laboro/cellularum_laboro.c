/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Bounded string work over SWAR words: length, compare, search, copy and numeric conversion.
 *
 * @note Strings only. Lengths that arrive off a wire belong to byteio, which is where rd_str and
 *       mpint_fixed live.
 * @note Every call is bounded by a cap the caller states, so no walk runs past it even when the bytes
 *       carry no terminator.
 */
#include "cellularum_laboro/cellularum_laboro.h"
#include "impensa_ancorae_acus/impensa_ancorae_acus.h"
#include "transformo/transformo.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @brief Arguments for every cellul backend, grouped by the calls that read them.
 *
 * @note Each backend reads one group; MMGR_CALL zeroes the members it is not given.
 */
typedef struct
{
    const char *const src;    /**< Bytes to read [BORROWS]. */
    const size_t cap;         /**< Bytes readable from src. */
    const char *const other;  /**< Second operand for compare and search [BORROWS]. */
    const size_t other_cap;   /**< Bytes readable from other. */
    char *const dst;          /**< Destination for copy [BORROWS]. */
    const size_t at;          /**< Offset into src where the call starts. */
    const uint8_t byte;       /**< Byte sought by chr. */
    const mmgr_bool ci;       /**< Fold case while comparing. */
    const mmgr_bool end_wins; /**< A terminator in the same lane counts as a match. */

    const mmgr_word wa; /**< First word for step_word. */
    const mmgr_word wb; /**< Second word for step_word. */
    const uint8_t ca;   /**< First byte for step_byte. */
    const uint8_t cb;   /**< Second byte for step_byte. */

    const size_t nlen;     /**< Needle length for pick_rows. */
    size_t *const rows;    /**< Needle offsets chosen by pick_rows [BORROWS]. */
    const size_t k;        /**< Needle offset read by ancorae_fold. */
    const mmgr_word fmask; /**< Mask carried with the search group. */
    size_t *const off;     /**< Offset target carried with the search group [BORROWS]. */

    const char **const end; /**< Set by the to_ calls past the last byte read [BORROWS]. */
    const char **const cur; /**< Cursor advanced by expo [BORROWS]. */
    mmgr_iword *const exp;  /**< Set by expo to the signed exponent [BORROWS]. */
} CellulCtx;

/**
 * @brief Compares one word pair case sensitively and reports whether the walk continues.
 *
 * @param[in] args Words wa and wb, with end_wins [BORROWS].
 * @return      MMGR_SWAR_GO when the words agree and carry no terminator, MMGR_SWAR_YES or MMGR_SWAR_NO otherwise.
 * @note MMGR_SWAR_YES when the terminator lane precedes the first differing lane, or ties it when end_wins.
 */
MMGR_INLINE mmgr_iword cellul_step_word_cs(const CellulCtx *args)
{
    const mmgr_word x = args->wa ^ args->wb;
    const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = args->wa);

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
    if (args->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

/**
 * @brief Compares one word pair with case folded and reports whether the walk continues.
 *
 * @param[in] args Words wa and wb, with end_wins [BORROWS].
 * @return      MMGR_SWAR_GO when the words agree and carry no terminator, MMGR_SWAR_YES or MMGR_SWAR_NO otherwise.
 * @note Differs from cellul_step_word_cs only in taking the difference through lane.xor_ with ci set.
 */
MMGR_INLINE mmgr_iword cellul_step_word_ci(const CellulCtx *args)
{
    const mmgr_word x = MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = args->wa, .val = args->wb, .ci = MMGR_TRUE);
    const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = args->wa);

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
    if (args->end_wins)
    {
        return (el <= dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    return (el < dl) ? MMGR_SWAR_YES : MMGR_SWAR_NO;
}

/**
 * @brief Compares one byte pair case sensitively and reports whether the walk continues.
 *
 * @param[in] args Bytes ca and cb, with end_wins [BORROWS].
 * @return      MMGR_SWAR_GO when the bytes match and ca is not the terminator, MMGR_SWAR_YES or MMGR_SWAR_NO otherwise.
 * @note A terminating ca gives MMGR_SWAR_YES when cb also terminates, or when end_wins is set.
 */
MMGR_INLINE mmgr_iword cellul_step_byte_cs(const CellulCtx *args)
{
    if (args->ca == 0)
    {
        if (args->ca == args->cb)
        {
            return MMGR_SWAR_YES;
        }
        return args->end_wins ? MMGR_SWAR_YES : MMGR_SWAR_NO;
    }
    if (args->ca != args->cb)
    {
        return MMGR_SWAR_NO;
    }
    return MMGR_SWAR_GO;
}

/**
 * @brief Compares one byte pair with case folded and reports whether the walk continues.
 *
 * @param[in] args Bytes ca and cb, with end_wins [BORROWS].
 * @return      MMGR_SWAR_GO when the bytes match and ca is not the terminator, MMGR_SWAR_YES or MMGR_SWAR_NO otherwise.
 * @note Takes the difference through lane.xor_ with ci set, so only the folded result is tested.
 */
MMGR_INLINE mmgr_iword cellul_step_byte_ci(const CellulCtx *args)
{
    // Explicit casts widen the two bytes to mmgr_word, so the lane compare sees one occupied lane each
    const mmgr_word d =
        MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = (mmgr_word)args->ca, .val = (mmgr_word)args->cb, .ci = MMGR_TRUE);

    if (args->ca == 0)
    {
        if (d == 0)
        {
            return MMGR_SWAR_YES;
        }
        return args->end_wins ? MMGR_SWAR_YES : MMGR_SWAR_NO;
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
    // Explicit cast reads the byte unsigned before the subtraction, so anything below tab wraps high
    // and fails the range rather than passing it as a negative
    const unsigned code = (unsigned)(unsigned char)ch;

    // Tab, newline, vertical tab, form feed and carriage return are 9 through 13 with nothing else
    // between them, so one unsigned range takes all five and space is the only test left. The six
    // comparisons this replaces were joined by short circuits, so a byte that is not whitespace -
    // which is most of them - ran and failed every one. Measured 2.16x on an ESP32-S3 over a buffer
    // holding none
    return (mmgr_bool)(((code - 9u) <= 4u) || (code == 32u));
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
 * @brief Bytes between p and the first word boundary at or after it, capped at cap.
 *
 * @param[in] p   Address a walk is about to start from [BORROWS].
 * @param[in] cap Bytes readable at p, which the answer never exceeds.
 * @return        Bytes to step one at a time before whole aligned words can be read.
 * @note Normally zero. This library is built for memory that arrives aligned, and an aligned address
 *       is already on a boundary. It is computed rather than assumed because the entries are also
 *       reached on interior pointers - find verifies a candidate at hay + k, which is any address.
 * @note The aligned load is one instruction on every target. The unaligned one is ten on Xtensa and
 *       eleven on RISC-V, because neither has the instruction and the compiler assembles the word
 *       out of byte loads and shifts, in the middle of the walk.
 */
MMGR_INLINE size_t cellul_head_bytes(const char *p, size_t cap)
{
    // Explicit cast reads the address as an integer so its low bits can be tested; the value is
    // never dereferenced through it and never converted back
    const size_t off = (size_t)((uintptr_t)p & (uintptr_t)(MMGR_SWAR_BYTES - 1u));
    const size_t need = (off == 0u) ? 0u : (MMGR_SWAR_BYTES - off);

    return (need > cap) ? cap : need;
}

/**
 * @brief Returns the offset of the first zero byte in src, or cap when there is none.
 *
 * @param[in] args Bytes src and the readable extent cap [BORROWS].
 * @return      Bytes before the terminator, at most cap.
 * @note Scans whole words with no mask at all, then masks the one short word at the end. The bound
 *       is known before the loop, so the lanes past cap can only ever fall in that last word, and
 *       building mask.tail per word costs six instructions an iteration to change nothing.
 */
MMGR_INLINE size_t cellul_len(const CellulCtx *args)
{
    // Bytes between src and the first word boundary at or after it. Normally none: this library is
    // built for memory that arrives aligned. It is walked rather than assumed because an entry is
    // also reached on an interior pointer - find verifies a candidate at hay + k - and the body
    // below reads through the aligned load, which is one instruction where the unaligned one is ten.
    const size_t lead = cellul_head_bytes(args->src, args->cap);
    const size_t full = lead + (((args->cap - lead) / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES);
    const size_t rest = args->cap - full;
    size_t at = 0u;

    while (at != lead)
    {
        if (args->src[at] == '\0')
        {
            return at;
        }
        // Advance separated from the test above so the loop body carries no side effect
        at += 1u;
    }

    // Two words a pass while two remain. The load and the arithmetic that reads it are a dependent
    // pair, and neither part issues them back to back without stalling; taking two lets the second
    // load be in flight while the first word is examined. The single-word loop below finishes the
    // odd word.
    while ((full - at) >= (2u * MMGR_SWAR_BYTES))
    {
        const mmgr_word w0 = MMGR_CALL(word.load_al, ScrutWordCfg, .at = args->src + at);
        const mmgr_word w1 = MMGR_CALL(word.load_al, ScrutWordCfg, .at = args->src + at + MMGR_SWAR_BYTES);
        const mmgr_word m0 = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w0);
        const mmgr_word m1 = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w1);

        if (m0 != 0u)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m0);
        }
        if (m1 != 0u)
        {
            return at + MMGR_SWAR_BYTES + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m1);
        }
        // Advance separated from the tests above so the loop body carries no side effect
        at += 2u * MMGR_SWAR_BYTES;
    }

    while (at != full)
    {
        const mmgr_word m =
            MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = MMGR_CALL(word.load_al, ScrutWordCfg, .at = args->src + at));
        if (m != 0u)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
        // Advance separated from the test above so the loop body carries no side effect
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word m =
            MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at)) &
            MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        if (m != 0u)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return args->cap;
}

/**
 * @brief Settles one word that carried a match, a terminator, or both.
 *
 * @param[in] p   Address the word was read from [BORROWS].
 * @param[in] end Lanes holding a terminator.
 * @param[in] hit Lanes holding the sought byte.
 * @return        Address of the match, or NULL when the terminator came first [BORROWS].
 * @note mask.before drops lanes at or past the terminator, so a match beginning after the run ends
 *       is not reported. Of an empty terminator mask it keeps every lane.
 * @note Takes the address rather than a CellulCtx: two places in the walk reach it, and the point of
 *       it is that neither carries this arithmetic in the loop.
 * @note Plain static, not MMGR_INLINE. It runs once per call - the walk reaches it on the word that
 *       ended the scan and not before - so a call costs nothing measurable, while forcing it inline
 *       puts mask.before and lane.first in the loop body and cost 6% at 2048 bytes.
 */
static const char *cellul_chr_settle(const char *p, mmgr_word end, mmgr_word hit)
{
    const mmgr_word live = hit & MMGR_CALL(mask.before, ScrutMaskCfg, .mask = end);

    return (live != 0u) ? (p + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = live)) : NULL;
}

/**
 * @brief Finds the first occurrence of byte in src, stopping at the terminator.
 *
 * @param[in] args Bytes src, the extent cap and the byte sought [BORROWS].
 * @return      Address of the match, or NULL when none precedes the terminator [BORROWS].
 * @note A byte of 0 returns the terminator's own address, which is src plus cellul_len.
 * @note mask.before drops lanes at or past the terminator, so a later match is not reported. It is
 *       applied once, on the word that carried a hit or a terminator: until one of those turns up
 *       there is nothing for it to drop, and mask.before of an empty terminator mask is every lane.
 * @note Whole words carry no extent mask. cap can only cut the last word short, and that word is
 *       walked once below the loop.
 */
MMGR_INLINE const char *cellul_chr(const CellulCtx *args)
{
    if (args->byte == 0u)
    {
        return args->src + cellul_len(args);
    }

    // Bytes to the first word boundary, so the walk below reads through the aligned load. See
    // cellul_head_bytes: normally none, and never more than a word.
    const size_t lead = cellul_head_bytes(args->src, args->cap);
    const size_t full = lead + (((args->cap - lead) / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES);
    const size_t rest = args->cap - full;
    // The sought byte repeated into every lane, once, ahead of the walk. lane.eq answers the same
    // question but rebuilds the broadcast from a byte on every call, and it is large enough that the
    // inliner drops it back out of line as this function grows - which cost 2.5x when it happened.
    const mmgr_word bcast = MMGR_SWAR_ONES * (mmgr_word)args->byte;
    size_t at = 0u;

    while (at != lead)
    {
        // Explicit cast reads the byte as unsigned, matching CellulCtx::byte
        const uint8_t h = (uint8_t)args->src[at];

        if (h == 0u)
        {
            return NULL;
        }
        if (h == args->byte)
        {
            return args->src + at;
        }
        // Advance separated from the tests above so the loop body carries no side effect
        at += 1u;
    }

    // One word a pass, deliberately. Unrolling this the way cellul_len is unrolled was measured and
    // lost: 8261 cycles to 8277 at 2048 bytes, and 98 to 114 at eight. len has one has_zero in its
    // body and stalls waiting for the load; this has two, which is already enough work to cover the
    // load, so a second word buys nothing and the extra prologue costs.
    while (at != full)
    {
        const mmgr_word w = MMGR_CALL(word.load_al, ScrutWordCfg, .at = args->src + at);
        const mmgr_word end = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w);
        const mmgr_word hit = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w ^ bcast);

        if ((end | hit) != 0u)
        {
            return cellul_chr_settle(args->src + at, end, hit);
        }
        // Advance separated from the test above so the loop body carries no side effect
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word keep = MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        const mmgr_word w = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at);
        const mmgr_word end = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w) & keep;
        const mmgr_word hit = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w ^ bcast) & keep &
                              MMGR_CALL(mask.before, ScrutMaskCfg, .mask = end);

        if (hit != 0u)
        {
            return args->src + at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = hit);
        }
    }
    return NULL;
}

/**
 * @brief Turns a lane-wise difference word into the mask of lanes that differ.
 *
 * @param[in] d Difference word, zero in every lane where the two sides agreed.
 * @return      One high bit per differing lane.
 * @note Takes the word rather than a CellulCtx. It is an expression the four compare walks share,
 *       not an entry anything dispatches to.
 */
MMGR_INLINE mmgr_word cellul_diff_lanes(mmgr_word d)
{
    return MMGR_VERBUM_SCRUTOR_HIGH & ~MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = d);
}

/**
 * @brief Returns the offset of the first byte where src and other differ, case sensitively.
 *
 * @param[in] args Bytes src and other, with the extent cap [BORROWS].
 * @return      Offset of the first difference, or cap when the two agree throughout.
 * @note Compares whole words with nothing but an inequality test, and resolves which lane differs
 *       once, after the loop has found the word that does. Which lane it is cannot matter until a
 *       word differs, and no word differs on all but one iteration of a scan.
 * @warning A terminator does not end the scan; cap is the only bound.
 */
MMGR_INLINE size_t cellul_diff_cs(const CellulCtx *args)
{
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = args->cap - full;
    size_t at = 0u;

    // Explicit casts read both addresses as integers so one mask answers for both. When the two
    // arrived on a boundary the run goes through the aligned load, which is one instruction where
    // the unaligned one is a sequence. The test is lifted out rather than carried in the body: as a
    // choice inside the loop the compiler keeps the branch per word and the run costs what the
    // unaligned one costs. memor_cmp took 6.27 cycles a byte to 2.02 on an ESP32-C6 this way
    const mmgr_bool level = (mmgr_bool)(((((uintptr_t)args->src) | ((uintptr_t)args->other)) &
                                         (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u);

    if (level)
    {
        while (at != full)
        {
            const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = args->src + at);
            const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = args->other + at);
            if (wa != wb)
            {
                return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = cellul_diff_lanes(wa ^ wb));
            }
            // Advance separated from the test above so the loop body carries no side effect
            at += MMGR_SWAR_BYTES;
        }
    }

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at);
        if (wa != wb)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = cellul_diff_lanes(wa ^ wb));
        }
        // Advance separated from the test above so the loop body carries no side effect
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word d = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at) ^
                            MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at);
        const mmgr_word m = cellul_diff_lanes(d) & MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        if (m != 0u)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return args->cap;
}

/**
 * @brief Returns the offset of the first byte where src and other differ, with case folded.
 *
 * @param[in] args Bytes src and other, with the extent cap [BORROWS].
 * @return      Offset of the first difference, or cap when the two agree throughout.
 * @note Differs from cellul_diff_cs only in taking the difference through lane.xor_ with ci set. The
 *       fold has to happen before the test, so the walk tests the folded word against zero rather
 *       than the two words against each other, and resolves the lane once on the way out.
 * @warning A terminator does not end the scan; cap is the only bound.
 */
MMGR_INLINE size_t cellul_diff_ci(const CellulCtx *args)
{
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = args->cap - full;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word d =
            MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at),
                      .val = MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at), .ci = MMGR_TRUE);
        if (d != 0u)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = cellul_diff_lanes(d));
        }
        // Advance separated from the test above so the loop body carries no side effect
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word d =
            MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at),
                      .val = MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at), .ci = MMGR_TRUE);
        const mmgr_word m = cellul_diff_lanes(d) & MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        if (m != 0u)
        {
            return at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
    }
    return args->cap;
}

/**
 * @brief Reports whether src reaches its terminator without differing from other, case sensitively.
 *
 * @param[in] args Bytes src and other, the extent cap, and end_wins [BORROWS].
 * @return      MMGR_TRUE when src's terminator precedes the first differing byte.
 * @note end_wins makes a terminator in the same lane as the difference count as agreement.
 * @note Reaching cap with neither a terminator nor a difference returns end_wins.
 */
MMGR_INLINE mmgr_bool cellul_agree_cs(const CellulCtx *args)
{
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = args->cap - full;
    size_t at = 0u;

    // Explicit casts read both addresses as integers so one mask answers for both, and the aligned
    // run is lifted into its own loop rather than chosen inside the body - as a choice per word the
    // compiler keeps the branch and the run costs what the unaligned one costs. The same shape took
    // memor_cmp from 6.27 cycles a byte to 2.02 on an ESP32-C6
    const mmgr_bool level = (mmgr_bool)(((((uintptr_t)args->src) | ((uintptr_t)args->other)) &
                                         (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u);

    while (level && (at != full))
    {
        const mmgr_word wa = MMGR_CALL(word.load_al, ScrutWordCfg, .at = args->src + at);
        const mmgr_word wb = MMGR_CALL(word.load_al, ScrutWordCfg, .at = args->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            const mmgr_word x = cellul_diff_lanes(wa ^ wb);
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            // Explicit cast narrows the lane comparison into the mmgr_bool container
            return (mmgr_bool)(args->end_wins ? (lz <= lx) : (lz < lx));
        }
        // Advance separated from the tests above so the loop body carries no side effect
        at += MMGR_SWAR_BYTES;
    }

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);

        if ((z != 0u) || (wa != wb))
        {
            const mmgr_word x = cellul_diff_lanes(wa ^ wb);
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            // Explicit cast narrows the lane comparison into the mmgr_bool container
            return (mmgr_bool)(args->end_wins ? (lz <= lx) : (lz < lx));
        }
        // Advance separated from the test above so the loop body carries no side effect
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word keep = MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word x = cellul_diff_lanes(wa ^ wb) & keep;

        if ((x | z) != 0u)
        {
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            // Explicit cast narrows the lane comparison into the mmgr_bool container
            return (mmgr_bool)(args->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return args->end_wins;
}

/**
 * @brief Reports whether src reaches its terminator without differing from other, case folded.
 *
 * @param[in] args Bytes src and other, the extent cap, and end_wins [BORROWS].
 * @return      MMGR_TRUE when src's terminator precedes the first differing byte.
 * @note Differs from cellul_agree_cs only in folding the two words through lane.xor_ with ci set.
 * @note Reaching cap with neither a terminator nor a difference returns end_wins.
 */
MMGR_INLINE mmgr_bool cellul_agree_ci(const CellulCtx *args)
{
    const size_t full = (args->cap / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;
    const size_t rest = args->cap - full;
    size_t at = 0u;

    while (at != full)
    {
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa);
        const mmgr_word fold = MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = wa, .val = wb, .ci = MMGR_TRUE);

        if ((z | fold) != 0u)
        {
            const mmgr_word x = cellul_diff_lanes(fold);
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            // Explicit cast narrows the lane comparison into the mmgr_bool container
            return (mmgr_bool)(args->end_wins ? (lz <= lx) : (lz < lx));
        }
        // Advance separated from the test above so the loop body carries no side effect
        at += MMGR_SWAR_BYTES;
    }

    if (rest != 0u)
    {
        const mmgr_word keep = MMGR_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = rest);
        const mmgr_word wa = MMGR_CALL(word.load, ScrutWordCfg, .at = args->src + at);
        const mmgr_word wb = MMGR_CALL(word.load, ScrutWordCfg, .at = args->other + at);
        const mmgr_word z = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = wa) & keep;
        const mmgr_word fold = MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = wa, .val = wb, .ci = MMGR_TRUE);
        const mmgr_word x = cellul_diff_lanes(fold) & keep;

        if ((x | z) != 0u)
        {
            const size_t lz = (z != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = z) : MMGR_SWAR_BYTES;
            const size_t lx = (x != 0u) ? MMGR_CALL(lane.first, ScrutLaneCfg, .mask = x) : MMGR_SWAR_BYTES;
            // Explicit cast narrows the lane comparison into the mmgr_bool container
            return (mmgr_bool)(args->end_wins ? (lz <= lx) : (lz < lx));
        }
    }
    return args->end_wins;
}

/**
 * @brief Reads other[k] and folds it to lower case when ci is set.
 *
 * @param[in] args Needle bytes other, the offset k, and ci [BORROWS].
 * @return      The byte, with 'A' to 'Z' mapped to 'a' to 'z' when ci is set.
 */
MMGR_INLINE uint8_t cellul_ancorae_fold(const CellulCtx *args)
{
    // Explicit cast reads the needle byte as unsigned, so the range tests below do not depend on char's signedness
    const uint8_t b = (uint8_t)args->other[args->k];

    if (args->ci && (b >= (uint8_t)'A') && (b <= (uint8_t)'Z'))
    {
        // Explicit cast keeps the result in uint8_t; bit 5 is what separates the two cases
        return (uint8_t)(b | 0x20u);
    }
    return b;
}

/**
 * @brief Chooses the needle offsets whose bytes cost least, for the search sieve.
 *
 * @param[in,out] args Needle other, its length nlen, ci, and the rows array to fill [BORROWS].
 * @return          Number of offsets written to args->rows, at most MMGR_SIEVE_ROWS.
 * @note Only the first MMGR_SWAR_BYTES of the needle are candidates, since one word is tested at a time.
 * @note Cost comes from ancorae.impensa, so rarer bytes are preferred.
 */
/**
 * @brief Lanes of the word at p holding the byte already broadcast through every lane of b.
 *
 * @param[in] p  Haystack address the candidate word is read from [BORROWS].
 * @param[in] b  The sought byte, repeated in every lane.
 * @param[in] ci Fold case while comparing.
 * @return       One high bit per lane that matched.
 * @note Takes its arguments directly rather than a CellulCtx. lane.eq answers the same question,
 *       but it rebuilds the broadcast from a byte on every call, and the sieve's byte is fixed for
 *       the whole walk; passing the broadcast in is what lets it be built once.
 */
MMGR_INLINE mmgr_word cellul_sieve_hit(const char *p, mmgr_word b, mmgr_bool ci)
{
    const mmgr_word w = MMGR_CALL(word.load, ScrutWordCfg, .at = p);
    const mmgr_word x = ci ? MMGR_CALL(lane.xor_, ScrutLaneCfg, .word = w, .val = b, .ci = MMGR_TRUE) : (w ^ b);

    return MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = x);
}

MMGR_INLINE size_t cellul_pick_rows(const CellulCtx *args)
{
    const size_t limit = (args->nlen > MMGR_SWAR_BYTES) ? MMGR_SWAR_BYTES : args->nlen;
    const size_t want = (limit > MMGR_SIEVE_ROWS) ? MMGR_SIEVE_ROWS : limit;

    for (size_t r = 0; r < want; ++r)
    {
        size_t best = 0;
        // 255 is the largest cost a uint8_t holds, so it acts as the no-row-chosen sentinel and any
        // real cost from ancorae.impensa compares below it
        uint8_t best_cost = 255;

        for (size_t k = 0; k < limit; ++k)
        {
            size_t taken = 0;

            for (size_t q = 0; q < r; ++q)
            {
                if (args->rows[q] == k)
                {
                    taken = 1;
                }
            }

            const uint8_t cost =
                MMGR_CALL(ancorae.impensa, AncoraeCfg,
                          .byte = cellul_ancorae_fold(&(CellulCtx){.other = args->other, .k = k, .ci = args->ci}));
            if (!taken && (cost < best_cost))
            {
                best_cost = cost;
                best = k;
            }
        }
        args->rows[r] = best;
    }
    return want;
}

/**
 * @brief The word one byte along from w, taken from w and the byte past it rather than reloaded.
 *
 * @param[in] w    The word at some offset.
 * @param[in] next The byte at that offset plus MMGR_SWAR_BYTES.
 * @return         The word the load at that offset plus one would have returned.
 * @note A word load at an odd address goes through mmgr_proxim_word_t, which carries MMGR_ALIGN(1),
 *       and neither Xtensa nor RISC-V has an unaligned word load - the compiler assembles one out of
 *       MMGR_SWAR_BYTES byte loads and shifts. Deriving it costs one byte load, one shift and an or.
 * @note Branches on MMGR_HW_BIG_ENDIAN because this is lane order, not wire order: which end of the
 *       word byte zero sits at. verbum_scrutor decides the same question the same way. The endian
 *       module answers a different one - what order a value is written in - and does not apply.
 */
#if !MMGR_HW_FAST_UNALIGNED
MMGR_INLINE mmgr_word cellul_word_next(mmgr_word w, uint8_t next)
{
#if MMGR_HW_BIG_ENDIAN
    return (mmgr_word)((w << 8u) | (mmgr_word)next);
#else
    return (mmgr_word)((w >> 8u) | ((mmgr_word)next << (MMGR_SWAR_BITS - 8u)));
#endif
}
#endif

/**
 * @brief Finds a needle of one or two bytes, case sensitively.
 *
 * @param[in] hay      Haystack [BORROWS].
 * @param[in] needle   Needle, of length nlen [BORROWS].
 * @param[in] nlen     Needle length, 1 or 2.
 * @param[in] read_cap Bytes readable at hay.
 * @param[in] starts   Start positions to consider, one past the last.
 * @return             Address of the match, or NULL when none precedes the terminator [BORROWS].
 * @note One broadcast per needle byte settles every start in a word at once: a lane matches when its
 *       byte equals the first and the byte after it equals the second. There is no anchor to choose
 *       and nothing to verify afterwards, which is the whole of what the sieve does.
 * @note Self-contained rather than folded into the sieve walk, tail and all. The two walks answer to
 *       different bounds - this one reads nlen - 1 bytes past its word, the sieve reads a whole
 *       verify span - and every previous attempt to share their structure cost more than it saved.
 * @warning Lanes at or past the terminator are dropped through mask.before, so a match that begins
 *          after the run ends is not reported.
 */
MMGR_INLINE const char *cellul_find_short(const char *hay, const char *needle, size_t nlen, size_t read_cap,
                                          size_t starts)
{
    // Explicit casts read the needle bytes as unsigned before they are repeated into every lane
    const mmgr_word b0 = MMGR_SWAR_ONES * (mmgr_word)(uint8_t)needle[0];
    const mmgr_word b1 = (nlen == 2u) ? (MMGR_SWAR_ONES * (mmgr_word)(uint8_t)needle[1]) : 0u;

    // A word step reads the word at `at` and, for a two-byte needle, the one at `at + 1`, so it needs
    // MMGR_SWAR_BYTES + nlen - 1 bytes in hand.
    const size_t span = MMGR_SWAR_BYTES + (nlen - 1u);
    const size_t safe = (read_cap >= span) ? ((read_cap - span) + 1u) : 0u;
    const size_t reach = (safe > starts) ? starts : safe;

    // Bytes to the first word boundary, so the walk below reads through the aligned load. See
    // cellul_head_bytes. Normally none, and the starts it covers are taken one at a time first.
    const size_t lead = cellul_head_bytes(hay, reach);
    const size_t nw = (reach - lead) / MMGR_SWAR_BYTES;

    for (size_t k = 0; k < lead; ++k)
    {
        // Explicit casts read both bytes as unsigned, so neither test depends on char's signedness
        const uint8_t h = (uint8_t)hay[k];

        if (h == 0u)
        {
            return NULL;
        }
        if ((h == (uint8_t)needle[0]) && ((nlen == 1u) || ((uint8_t)hay[k + 1u] == (uint8_t)needle[1])))
        {
            return hay + k;
        }
    }

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = lead + (wi * MMGR_SWAR_BYTES);
        const mmgr_word w0 = MMGR_CALL(word.load_al, ScrutWordCfg, .at = hay + at);
        const mmgr_word end = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w0);
        mmgr_word m = MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w0 ^ b0);

        if (nlen == 2u)
        {
            // Where the hardware loads a word from any address in one instruction, that is cheaper
            // than deriving it; where it does not, the load is a dozen instructions and deriving it
            // from the word already in hand costs three.
#if MMGR_HW_FAST_UNALIGNED
            const mmgr_word w1 = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + at + 1u);
#else
            // Explicit cast reads the byte past this word as unsigned, matching the lane it fills
            const mmgr_word w1 = cellul_word_next(w0, (uint8_t)hay[at + MMGR_SWAR_BYTES]);
#endif
            m &= MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w1 ^ b1);
        }
        if (end != 0u)
        {
            m &= MMGR_CALL(mask.before, ScrutMaskCfg, .mask = end);
        }
        if (m != 0u)
        {
            return hay + at + MMGR_CALL(lane.first, ScrutLaneCfg, .mask = m);
        }
        if (end != 0u)
        {
            return NULL;
        }
    }

    for (size_t k = lead + (nw * MMGR_SWAR_BYTES); k < starts; ++k)
    {
        // Explicit casts read both bytes as unsigned, so neither test depends on char's signedness
        const uint8_t h = (uint8_t)hay[k];

        if (h == 0u)
        {
            return NULL;
        }
        if ((h == (uint8_t)needle[0]) && ((nlen == 1u) || ((uint8_t)hay[k + 1u] == (uint8_t)needle[1])))
        {
            return hay + k;
        }
    }
    return NULL;
}

/**
 * @brief Finds the first occurrence of the needle inside the haystack.
 *
 * @param[in] args  Haystack src with cap, and needle other with other_cap [BORROWS].
 * @param[in] ci Fold case while matching.
 * @return       Address of the match, or NULL when there is none [BORROWS].
 * @note An empty needle returns the haystack start; a needle longer than cap returns NULL.
 * @note Candidate words are sieved on the cheapest needle offsets, then verified in full.
 * @note Word scanning covers only the starts that stay in bounds; the rest are walked one byte at a time.
 * @note args->nlen when the caller knows the needle's length, and only otherwise a measure of it. A
 *       needle is nearly always a literal, so its length is settled before the build and measuring it
 *       on every call is work the caller already did.
 */
MMGR_INLINE const char *cellul_find_core(const CellulCtx *args, mmgr_bool ci)
{
    const char *const hay = args->src;
    const char *const needle = args->other;
    const size_t read_cap = args->cap;

    const size_t nlen =
        (args->nlen != 0u) ? args->nlen : cellul_len(&(CellulCtx){.src = needle, .cap = args->other_cap});

    if (nlen == 0u)
    {
        return hay;
    }
    if (nlen > read_cap)
    {
        return NULL;
    }

    const size_t take = (nlen > MMGR_SWAR_BYTES) ? MMGR_SWAR_BYTES : nlen;
    const size_t starts = read_cap - nlen + 1u;

    // A needle this short is settled by a mask chain, with no anchor to choose and nothing to verify.
    // The sieve below earns its prologue by finding a rare byte in a long needle and proving the rest
    // once; over one or two bytes there is no rare byte to find and no rest to prove, and the
    // prologue is most of the call.
    if ((nlen <= 2u) && !ci && (read_cap <= MMGR_FIND_CHAIN_MAX))
    {
        return cellul_find_short(hay, needle, nlen, read_cap, starts);
    }

    const size_t tail =
        (nlen > take) ? (MMGR_CALL(word.count, ScrutWordCfg, .bytes = nlen - take) * MMGR_SWAR_BYTES) : 0u;
    const size_t verify_reach = (MMGR_SWAR_BYTES - 1u) + take + tail;

    size_t rows[MMGR_SIEVE_ROWS];
    const size_t nrows = cellul_pick_rows(&(CellulCtx){.other = needle, .nlen = nlen, .rows = rows, .ci = ci});

    const mmgr_word nmask = MMGR_CALL(mask.bytes_below, ScrutMaskCfg, .bytes = take);
    const mmgr_word nraw = MMGR_CALL(word.load, ScrutWordCfg, .at = needle) & nmask;
    const mmgr_word nword = ci ? (MMGR_CALL(word.fold_lower, ScrutWordCfg, .word = nraw) & nmask) : nraw;

    size_t maxrow = rows[0];

    for (size_t r = 1; r < nrows; ++r)
    {
        if (rows[r] > maxrow)
        {
            maxrow = rows[r];
        }
    }

    const size_t ancorae_reach = maxrow + MMGR_SWAR_BYTES;
    const size_t reach = (ancorae_reach > verify_reach) ? ancorae_reach : verify_reach;

    size_t safe = (read_cap >= reach) ? ((read_cap - reach) + 1u) : 0u;

    if (safe > starts)
    {
        safe = starts;
    }

    const size_t nw = safe / MMGR_SWAR_BYTES;

    // The sieve's bytes broadcast once, ahead of the walk. They depend only on the needle, which
    // does not move, and rebuilding a broadcast is a multiply that would otherwise land on every
    // haystack word.
    mmgr_word bcast[MMGR_SIEVE_ROWS];

    for (size_t r = 0; r < nrows; ++r)
    {
        // Explicit casts read the needle byte as unsigned, then widen it into the lane it fills
        bcast[r] = MMGR_SWAR_ONES * (mmgr_word)(uint8_t)needle[rows[r]];
    }

    for (size_t wi = 0; wi < nw; ++wi)
    {
        const size_t at = wi * MMGR_SWAR_BYTES;
        const mmgr_word end =
            MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = MMGR_CALL(word.load, ScrutWordCfg, .at = hay + at));
        mmgr_word m = cellul_sieve_hit(hay + at + rows[0], bcast[0], ci);

        for (size_t r = 1; r < nrows; ++r)
        {
            m &= cellul_sieve_hit(hay + at + rows[r], bcast[r], ci);
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
            // Explicit casts read both bytes as unsigned, matching CellulCtx::ca and ::cb
            const uint8_t h = (uint8_t)hay[k + i];
            const uint8_t nb = (uint8_t)needle[i];

            if (h == 0u)
            {
                break;
            }
            // The case-sensitive step is a byte compare once the terminator is out of the way, so it
            // is written as one. This walk covers the starts the word loop could not reach, which on
            // a short haystack is most of them, and reaching cellul_step_byte_cs through a CellulCtx
            // for every byte of every start is what made find cost twice libc at eight bytes.
            if (ci)
            {
                const CellulCtx b = {.ca = nb, .cb = h, .end_wins = MMGR_FALSE};

                if (cellul_step_byte_ci(&b) == MMGR_SWAR_NO)
                {
                    break;
                }
            }
            else if (h != nb)
            {
                break;
            }
            // Advance separated from the tests above so the loop body carries no side effect
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
 * @param[in,out] args Source src, destination dst, and the destination extent cap [BORROWS].
 * @return          Bytes copied, not counting the terminator.
 * @note A cap of 0 copies nothing and writes no terminator.
 * @note The source is measured against cap minus one, leaving room for the terminator.
 */
MMGR_INLINE size_t cellul_copy(const CellulCtx *args)
{
    if (args->cap == 0u)
    {
        return 0u;
    }

    const size_t limit = args->cap - 1u;
    size_t at = 0u;

    // One walk rather than two. Measuring with cellul_len and then copying with proxim.read reads
    // every byte twice; a word that holds no terminator is one this can store as it goes. Measured
    // 1.46x to 1.66x on an ESP32-S3, which also takes it past the strncpy it is compared with.
    // Explicit casts read both addresses as integers so one mask answers for both: the store is as
    // wide as the load, so the word run needs the two on a boundary together
    if (((((uintptr_t)args->dst) | ((uintptr_t)args->src)) & (uintptr_t)(MMGR_SWAR_BYTES - 1u)) == 0u)
    {
        const size_t full = (limit / MMGR_SWAR_BYTES) * MMGR_SWAR_BYTES;

        while (at != full)
        {
            const mmgr_word w = MMGR_CALL(word.load_al, ScrutWordCfg, .at = args->src + at);

            if (MMGR_CALL(lane.has_zero, ScrutLaneCfg, .word = w) != 0u)
            {
                break;
            }
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = args->dst + at, .val = (uint64_t)w);
            // Advance separated from the store above so the loop body carries no side effect
            at += MMGR_SWAR_BYTES;
        }
    }

    // Whatever the word run did not take, which is the tail of a run that met a terminator, the
    // bytes below a boundary the two did not share, or the whole string when they never did
    while ((at != limit) && (args->src[at] != '\0'))
    {
        args->dst[at] = args->src[at];
        at += 1u;
    }
    args->dst[at] = '\0';
    return at;
}

/**
 * @brief Reads an optionally signed decimal integer from src.
 *
 * @param[in,out] args Text src, and the optional end target [BORROWS].
 * @return          The value, negated when a minus sign was read.
 * @note Leading whitespace is skipped, then one optional '+' or '-'.
 * @note When end is not NULL it is set past the last digit, or back to src when no digit was read.
 * @warning The digit accumulator is mmgr_word wide and wraps on a longer run.
 */
MMGR_INLINE mmgr_iword cellul_to_long(const CellulCtx *args)
{
    const char *p = args->src;

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
    mmgr_word v = 0;
    while (cellul_is_digit(*p))
    {
        // Explicit casts hold the running value and the digit at mmgr_word width
        // The p++ belongs on a line of its own: an increment folded into this expression is the side
        // effect the standard bans, and the cursor advance does not depend on the arithmetic
        v = (mmgr_word)(v * 10u) + (mmgr_word)(*p++ - '0');
    }

    if (args->end != NULL)
    {
        *args->end = (p != ds) ? p : args->src;
    }
    if (neg)
    {
        // Explicit cast carries the unsigned negation into mmgr_iword; 0u - v wraps at mmgr_word width first
        return (mmgr_iword)(0u - v);
    }
    return (mmgr_iword)v;
}

/**
 * @brief Reads an unsigned decimal integer from src.
 *
 * @param[in,out] args Text src, and the optional end target [BORROWS].
 * @return          The accumulated value.
 * @note Leading whitespace is skipped, then one optional '+'; a '-' is not accepted and stops the read.
 * @note When end is not NULL it is set past the last digit, or back to src when no digit was read.
 * @warning The digit accumulator is mmgr_word wide and wraps on a longer run.
 */
MMGR_INLINE mmgr_word cellul_to_ulong(const CellulCtx *args)
{
    const char *p = args->src;

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
        // Explicit casts hold the running value and the digit at mmgr_word width
        // The p++ belongs on a line of its own: an increment folded into this expression is the side
        // effect the standard bans, and the cursor advance does not depend on the arithmetic
        v = (mmgr_word)(v * 10u) + (mmgr_word)(*p++ - '0');
    }

    if (args->end != NULL)
    {
        *args->end = (p != ds) ? p : args->src;
    }
    return v;
}

/**
 * @brief Reads a signed decimal exponent, advancing the cursor past it.
 *
 * @param[in,out] args Cursor cur and the exponent target exp [BORROWS].
 * @note The cursor sits on the 'e' or 'E', which is consumed first.
 * @note When no digit follows, the cursor is put back where it started and exp is left alone.
 * @note Digits beyond MMGR_MUTO_EXP_LIMIT are consumed but stop changing the value.
 */
MMGR_INLINE void cellul_expo(const CellulCtx *args)
{
    const char *const mark = *args->cur;

    (*args->cur)++;

    mmgr_bool eneg = MMGR_FALSE;
    if ((**args->cur == '+') || (**args->cur == '-'))
    {
        // The increment belongs on a line of its own, as at 707 and 728: folded through the double
        // indirection and into an assignment, it is the side effect the standard bans
        eneg = (*(*args->cur)++ == '-');
    }
    if (!cellul_is_digit(**args->cur))
    {
        *args->cur = mark;
        return;
    }

    mmgr_iword ex = 0;
    while (cellul_is_digit(**args->cur))
    {
        if (ex < MMGR_MUTO_EXP_LIMIT)
        {
            // Explicit cast holds the accumulate in mmgr_iword; the guard above keeps it below MMGR_MUTO_EXP_LIMIT
            ex = (mmgr_iword)((ex * 10) + (**args->cur - '0'));
        }
        (*args->cur)++;
    }
    *args->exp = eneg ? -ex : ex;
}

/**
 * @brief Reads a decimal floating point number from src.
 *
 * @param[in,out] args Text src, and the optional end target [BORROWS].
 * @return          The value assembled by muto.scale from the mantissa and exponent.
 * @note Accepts leading whitespace, one optional sign, digits, one optional point, then an optional exponent.
 * @note Digits that no longer fit the mantissa advance the exponent instead, and a non-zero one sets the sticky rest.
 * @note An exponent is read only when at least one digit was seen before it.
 * @note When end is not NULL it is set past the number, or back to src when no digit was read.
 */
MMGR_INLINE double cellul_to_double(const CellulCtx *args)
{
    const char *p = args->src;

    while (cellul_is_ws(*p))
    {
        p++;
    }

    mmgr_bool neg = MMGR_FALSE;
    if ((*p == '+') || (*p == '-'))
    {
        // The increment belongs on a line of its own, as ++p does below: folded into this assignment
        // it is the side effect the standard bans
        neg = (*p++ == '-');
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

    // Explicit cast holds the combined exponent in mmgr_iword: ex from the suffix, over and drop from the mantissa
    const double val = MMGR_CALL(muto.scale, TransformoCfg, .mant = &mant, .ex = (mmgr_iword)(ex + over - drop),
                                 .rest = lost, .neg = neg);

    if (args->end != NULL)
    {
        *args->end = any ? p : args->src;
    }
    return val;
}

/**
 * @brief Reads a decimal floating point number from src and narrows it to float.
 *
 * @param[in,out] args Text src, and the optional end target [BORROWS].
 * @return          The value from cellul_to_double, narrowed to float.
 * @note Rounding happens once, on the narrowing; the parse itself is done at double width.
 */
MMGR_INLINE float cellul_to_float(const CellulCtx *args)
{
    // Explicit cast narrows the double result into the float container
    return (float)cellul_to_double(args);
}

/**
 * @brief Picks the folded or exact difference walk on args->ci.
 *
 * @param[in] args Bytes src and other, the extent cap, and ci [BORROWS].
 * @return      Offset of the first difference, or cap when the two agree.
 */
MMGR_INLINE size_t cellul_diff(const CellulCtx *args)
{
    return args->ci ? cellul_diff_ci(args) : cellul_diff_cs(args);
}

/**
 * @brief Picks the folded or exact agreement walk on args->ci.
 *
 * @param[in] args Bytes src and other, the extent cap, ci and end_wins [BORROWS].
 * @return      MMGR_TRUE when src's terminator precedes the first difference.
 * @note The caller sets end_wins: clear for eq, so both must end together, set for starts.
 */
MMGR_INLINE mmgr_bool cellul_eq(const CellulCtx *args)
{
    return args->ci ? cellul_agree_ci(args) : cellul_agree_cs(args);
}

/**
 * @brief The same walk as cellul_eq, reached under the name the starts entry pastes.
 *
 * @param[in] args Bytes src and other, the extent cap, ci and end_wins [BORROWS].
 * @return      MMGR_TRUE when src's terminator precedes the first difference.
 * @note starts swaps the operands and sets end_wins in its entry line, so the walk is the same one.
 */
MMGR_INLINE mmgr_bool cellul_starts(const CellulCtx *args)
{
    return cellul_eq(args);
}

/**
 * @brief Searches src for other, folding case on args->ci.
 *
 * @param[in] args Haystack src with cap, needle other with other_cap, and ci [BORROWS].
 * @return      Address of the match, or NULL when there is none [BORROWS].
 * @note Plain static, not MMGR_INLINE, and it is the only backend here that is. MMGR_INLINE carries
 *       always_inline, and cellul_has calls this one, so forcing it would emit the whole sieve search
 *       a second time inside has. Measured at 2880 bytes duplicated at -O2.
 */
static const char *cellul_find(const CellulCtx *args)
{
    return cellul_find_core(args, args->ci);
}

/**
 * @brief Reports whether cellul_find returns a match.
 *
 * @param[in] args Haystack src with cap, needle other with other_cap, and ci [BORROWS].
 * @return      MMGR_TRUE when the needle is present.
 */
MMGR_INLINE mmgr_bool cellul_has(const CellulCtx *args)
{
    // Explicit cast narrows the pointer test into the mmgr_bool container
    return (mmgr_bool)(cellul_find(args) != NULL);
}

/**
 * @brief Tests the byte at src[at] for whitespace.
 *
 * @param[in] args Bytes src and the offset at [BORROWS].
 * @return      MMGR_TRUE for one of the six whitespace characters.
 */
MMGR_INLINE mmgr_bool cellul_ws(const CellulCtx *args)
{
    return cellul_is_ws(args->src[args->at]);
}

/**
 * @brief Tests the byte at src[at] for a decimal digit.
 *
 * @param[in] args Bytes src and the offset at [BORROWS].
 * @return      MMGR_TRUE for '0' through '9'.
 */
MMGR_INLINE mmgr_bool cellul_digit(const CellulCtx *args)
{
    return cellul_is_digit(args->src[args->at]);
}

/**
 * @brief Picks the folded or exact word step on args->ci.
 *
 * @param[in] args Words wa and wb, with ci and end_wins [BORROWS].
 * @return      MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE mmgr_iword cellul_step_word(const CellulCtx *args)
{
    return args->ci ? cellul_step_word_ci(args) : cellul_step_word_cs(args);
}

/**
 * @brief Picks the folded or exact byte step on args->ci.
 *
 * @param[in] args Bytes ca and cb, with ci and end_wins [BORROWS].
 * @return      MMGR_SWAR_GO, MMGR_SWAR_YES or MMGR_SWAR_NO.
 */
MMGR_INLINE mmgr_iword cellul_step_byte(const CellulCtx *args)
{
    return args->ci ? cellul_step_byte_ci(args) : cellul_step_byte_cs(args);
}

/**
 * @brief Returns a copy of the argument struct.
 *
 * @note Hand-rolled rather than an entry line, as mmgr_infin_init is: it returns a CatenaFinitaCfg
 *       rather than forwarding one, so there is no argument pack for GENERIC_ENTRY to build.
 * @note The name is parenthesized so a like-named macro from mmgr_string_shim.h cannot expand here.
 * @note Documented at the declaration in cellularum_laboro.h.
 */
CatenaFinitaCfg(mmgr_cellul_init)(const CatenaFinitaCfg *args)
{
    return *args;
}

/**
 * @brief Binds the string calls to the entry shape, keeping the parenthesized name.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_cellul_ and cellul_ prefixes, which the two share.
 * @note Written out rather than reached for as GENERIC_ENTRY, and this is the only module that does
 *       so. GENERIC_ENTRY pastes the entry name bare, and these names must stay parenthesized so a
 *       like-named macro from mmgr_string_shim.h cannot expand over them. The shape is otherwise the
 *       same one carceribus and infinitas use.
 */
#define CELLUL_ENTRY(ret, name, ...)                                                                                   \
    ret(mmgr_cellul_##name)(const CatenaFinitaCfg *args)                                                               \
    {                                                                                                                  \
        return MMGR_CALL(cellul_##name, CellulCtx, __VA_ARGS__);                                                       \
    }

/**
 * @brief The same shape for the single-step compares, which take their own argument type.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_cellul_ and cellul_ prefixes.
 */
#define CELLUL_STEP_ENTRY(ret, name, ...)                                                                              \
    ret(mmgr_cellul_##name)(const VerboProgrediorCfg *args)                                                            \
    {                                                                                                                  \
        return MMGR_CALL(cellul_##name, CellulCtx, __VA_ARGS__);                                                       \
    }

/**
 * @brief The same shape for the conversions, which take their own argument type.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_cellul_ and cellul_ prefixes.
 */
#define CELLUL_CONV_ENTRY(ret, name, ...)                                                                              \
    ret(mmgr_cellul_##name)(const TransfiguroCfg *args)                                                                \
    {                                                                                                                  \
        return MMGR_CALL(cellul_##name, CellulCtx, __VA_ARGS__);                                                       \
    }

/**
 * @brief The public surface, one line per entry point.
 *
 * @note Each is documented at its declaration in cellularum_laboro.h.
 * @note The fields each line forwards are the ones that entry reads; MMGR_CALL zeroes the rest.
 * @note eq and starts reach the same backend. starts swaps src and other and sets end_wins, so it is
 *       other that is measured for its terminator and the prefix may end early.
 */
CELLUL_ENTRY(size_t, len, .src = args->src + args->at, .cap = args->cap - args->at)
CELLUL_ENTRY(size_t, diff, .src = args->src, .other = args->other, .cap = args->cap, .ci = args->ci)
CELLUL_ENTRY(mmgr_bool, eq, .src = args->src, .other = args->other, .cap = args->cap, .ci = args->ci,
             .end_wins = MMGR_FALSE)
CELLUL_ENTRY(mmgr_bool, starts, .src = args->other, .other = args->src, .cap = args->cap, .ci = args->ci,
             .end_wins = MMGR_TRUE)
CELLUL_ENTRY(const char *, find, .src = args->src, .cap = args->cap, .other = args->other, .other_cap = args->other_cap,
             .nlen = args->other_len, .ci = args->ci)
CELLUL_ENTRY(mmgr_bool, has, .src = args->src, .cap = args->cap, .other = args->other, .other_cap = args->other_cap,
             .nlen = args->other_len, .ci = args->ci)
CELLUL_ENTRY(const char *, chr, .src = args->src, .cap = args->cap, .byte = args->byte)
CELLUL_ENTRY(size_t, copy, .dst = args->dst, .src = args->src, .cap = args->cap)
CELLUL_ENTRY(mmgr_bool, ws, .src = args->src, .at = args->at)
CELLUL_ENTRY(mmgr_bool, digit, .src = args->src, .at = args->at)
CELLUL_STEP_ENTRY(mmgr_iword, step_word, .wa = args->wa, .wb = args->wb, .ci = args->ci, .end_wins = args->end_wins)
CELLUL_STEP_ENTRY(mmgr_iword, step_byte, .ca = args->ca, .cb = args->cb, .ci = args->ci, .end_wins = args->end_wins)
CELLUL_CONV_ENTRY(mmgr_iword, to_long, .src = args->src, .end = args->end)
CELLUL_CONV_ENTRY(mmgr_word, to_ulong, .src = args->src, .end = args->end)
CELLUL_CONV_ENTRY(double, to_double, .src = args->src, .end = args->end)
CELLUL_CONV_ENTRY(float, to_float, .src = args->src, .end = args->end)
