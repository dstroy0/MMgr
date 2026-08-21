// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_VERBUM_SCRUTOR_H
#define MMGR_VERBUM_SCRUTOR_H

#include "proximus_operor/proximus_operor.h"

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

/**
 * @file verbum_scrutor.h
 * @brief SWAR lane operations. Every entry works on one machine word of byte lanes.
 *
 * No intrinsics, no builtins, no asm. Pure C in the spelling the compiler folds.
 *
 * The carrier is the machine word on every target. A narrower carrier is never faster: the same
 * cache line moves, the same load port is used, C promotes below int anyway, and you process fewer
 * lanes for it. Lane count follows the register.
 */

/** @brief The lane carrier. Always the machine word. */
typedef mmgr_word mmgr_scrut_word;

/** @brief Lanes per word. */
#define MMGR_SWAR_BYTES (sizeof(mmgr_scrut_word))
/** @brief Bits per word. */
#define MMGR_SWAR_LANE_BITS (MMGR_WORD_BITS)

MMGR_STATIC_ASSERT(sizeof(mmgr_scrut_word) == sizeof(mmgr_word),
                   "the lane carrier is the machine word - it is not separately sized");

/** @brief 0x01 in every lane. */
#define MMGR_SWAR_ONES (((mmgr_scrut_word) ~(mmgr_scrut_word)0) / 0xFFu)
/** @brief 0x80 in every lane. Every mask below is built from these bits. */
#define MMGR_VERBUM_SCRUTOR_HIGH (MMGR_SWAR_ONES * 0x80u)
/** @brief 0x7F in every lane. */
#define MMGR_SWAR_LOW7 (MMGR_SWAR_ONES * 0x7Fu)

/** @brief Step verdict: keep going. */
#define MMGR_SWAR_GO 0
/** @brief Step verdict: matched. */
#define MMGR_SWAR_YES 1
/** @brief Step verdict: did not match. */
#define MMGR_SWAR_NO 2

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_scrut_word (*ge)(mmgr_scrut_word a, mmgr_scrut_word v);
    mmgr_scrut_word (*le)(mmgr_scrut_word a, mmgr_scrut_word v);
    mmgr_scrut_word (*spread)(mmgr_scrut_word m);
    mmgr_scrut_word (*sub7)(mmgr_scrut_word a, mmgr_scrut_word lo);
    mmgr_scrut_word (*has_zero)(mmgr_scrut_word w);
    mmgr_scrut_word (*eq)(mmgr_scrut_word w, uint8_t c, mmgr_bool ci);
    mmgr_scrut_word (*xor_)(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci);
    size_t (*zero_lane)(mmgr_scrut_word m);
    mmgr_scrut_word (*load)(const char *p);
    mmgr_scrut_word (*load_al)(const char *p);
} VerbumScrutorNs;
MMGR_NS_LAYOUT(VerbumScrutorNs, ge, le, spread, sub7, has_zero, eq, xor_, zero_lane, load, load_al);

/**
 * @brief Lanes of @p a at or above @p v.
 * @param a Word of lanes.
 * @param v Byte to compare against, broadcast.
 * @return High bit set in each lane that passes.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_ge(mmgr_scrut_word a, mmgr_scrut_word v)
{
    return ((a | MMGR_VERBUM_SCRUTOR_HIGH) - v * MMGR_SWAR_ONES) & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief Lanes of @p a at or below @p v.
 * @param a Word of lanes.
 * @param v Byte to compare against, broadcast.
 * @return High bit set in each lane that passes.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_le(mmgr_scrut_word a, mmgr_scrut_word v)
{
    return ((v * MMGR_SWAR_ONES | MMGR_VERBUM_SCRUTOR_HIGH) - a) & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief Widen a lane high bit to fill its whole lane.
 * @param m Lane mask.
 * @return 0xFF in each lane that was set.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_spread(mmgr_scrut_word m)
{
    return (mmgr_scrut_word)(m + (m - (m >> 7)));
}

/**
 * @brief Per-lane subtract that keeps the result in the low 7 bits.
 * @param a Word of lanes.
 * @param lo Byte to subtract, broadcast.
 * @return Difference per lane, high bit cleared.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_sub7(mmgr_scrut_word a, mmgr_scrut_word lo)
{
    return ((a | MMGR_VERBUM_SCRUTOR_HIGH) - lo * MMGR_SWAR_ONES) & MMGR_SWAR_LOW7;
}

/**
 * @brief Which lanes are zero.
 * @param w Word of lanes.
 * @return High bit set in each zero lane. Exact - no false positives in higher lanes.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_has_zero(mmgr_scrut_word w)
{
    return ~(((w & MMGR_SWAR_LOW7) + MMGR_SWAR_LOW7) | w) & MMGR_VERBUM_SCRUTOR_HIGH;
}

/**
 * @brief Which lanes equal @p c.
 * @param w Word of lanes.
 * @param c Byte to match.
 * @return High bit set in each matching lane.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_eq(mmgr_scrut_word w, uint8_t c)
{
    return mmgr_scrut_has_zero(w ^ (MMGR_SWAR_ONES * (mmgr_scrut_word)c));
}

/**
 * @brief How many lanes are set.
 * @param m Lane mask.
 * @return Population count of the lanes.
 *
 * Horizontal byte sum. This is also the population count, so the sieve gets it free.
 *
 * The cast around the multiply is load bearing. Below 32 bits C promotes to int, the multiply does
 * not wrap, and the shift reads the wrong byte.
 */
MMGR_INLINE size_t mmgr_scrut_lanes(mmgr_scrut_word m)
{
    return (size_t)((mmgr_scrut_word)((mmgr_scrut_word)(m >> 7) * MMGR_SWAR_ONES) >> (MMGR_SWAR_LANE_BITS - 8u));
}

/**
 * @brief Index of the lowest set lane.
 * @param m Lane mask, non-zero.
 * @return Lane index.
 *
 * Counts the trailing zero run. (m - 1) alone is not that run - with more than one lane set the
 * borrow stops at the lowest set bit and leaves the higher lanes standing, so it is masked by ~m.
 */
MMGR_INLINE size_t mmgr_scrut_lane_lo(mmgr_scrut_word m)
{
    return mmgr_scrut_lanes((mmgr_scrut_word)((mmgr_scrut_word)((mmgr_scrut_word)(m - 1u) & (mmgr_scrut_word)~m) &
                                              MMGR_VERBUM_SCRUTOR_HIGH));
}

/**
 * @brief Index of the highest set lane.
 * @param m Lane mask, non-zero.
 * @return Lane index.
 *
 * Smear down by lanes, then the count minus one. Loop bounds are constant so it unrolls and no
 * shift is ever wider than the word - a fixed ladder would need rewriting per width.
 */
MMGR_INLINE size_t mmgr_scrut_lane_hi(mmgr_scrut_word m)
{
    for (unsigned s = 8u; s < MMGR_SWAR_LANE_BITS; s <<= 1)
    {
        m = (mmgr_scrut_word)(m | (mmgr_scrut_word)(m >> s));
    }
    return mmgr_scrut_lanes(m) - 1u;
}

/**
 * @def mmgr_scrut_lane_first
 * @brief Lane of the first match in address order.
 * @def mmgr_scrut_lane_last
 * @brief Lane of the last match in address order.
 *
 * Byte order is one compile time fact and both derive from it. Little endian puts p+0 in the low
 * byte, so first is the lowest lane. Big endian puts p+0 in the high byte, so first is the highest.
 * Nothing decides this at run time and there is no second implementation.
 */
#if MMGR_HW_BIG_ENDIAN
#define mmgr_scrut_lane_first mmgr_scrut_lane_hi
#define mmgr_scrut_lane_last mmgr_scrut_lane_lo
#else
#define mmgr_scrut_lane_first mmgr_scrut_lane_lo
#define mmgr_scrut_lane_last mmgr_scrut_lane_hi
#endif

/**
 * @brief Clear the lowest set lane.
 * @param m Lane mask.
 * @return @p m without its lowest lane.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_drop_lo(mmgr_scrut_word m)
{
    return (mmgr_scrut_word)(m & (mmgr_scrut_word)(m - 1u));
}

/**
 * @brief Clear the highest set lane.
 * @param m Lane mask.
 * @return @p m without its highest lane.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_drop_hi(mmgr_scrut_word m)
{
    mmgr_scrut_word s = m;
    for (unsigned k = 8u; k < MMGR_SWAR_LANE_BITS; k <<= 1)
    {
        s = (mmgr_scrut_word)(s | (mmgr_scrut_word)(s >> k));
    }
    return (mmgr_scrut_word)(m & (mmgr_scrut_word) ~(mmgr_scrut_word)(s ^ (mmgr_scrut_word)(s >> 8)));
}

/**
 * @def mmgr_scrut_drop_first
 * @brief Clear the first lane in address order.
 * @def mmgr_scrut_drop_last
 * @brief Clear the last lane in address order.
 */
#if MMGR_HW_BIG_ENDIAN
#define mmgr_scrut_drop_first mmgr_scrut_drop_hi
#define mmgr_scrut_drop_last mmgr_scrut_drop_lo
#else
#define mmgr_scrut_drop_first mmgr_scrut_drop_lo
#define mmgr_scrut_drop_last mmgr_scrut_drop_hi
#endif

/**
 * @brief First matching byte in address order.
 * @param m Lane mask, non-zero.
 * @return Byte offset within the word.
 *
 * Named for the bytes, not the bits. There is no general count-trailing-zeros here and no bit scan
 * builtin anywhere in the library - a lane index is a horizontal byte sum, same speed as tzcnt, and
 * it links on a freestanding target where __builtin_popcount does not.
 */
MMGR_INLINE size_t mmgr_scrut_zero_lane(mmgr_scrut_word m)
{
    return mmgr_scrut_lane_first(m);
}

/**
 * @brief How many loads a scan of @p bytes needs.
 * @param bytes Byte cap.
 * @return Word count.
 *
 * This is the whole plan of every scan. No alignment peel and no byte remainder: an unaligned load
 * is the same instruction as an aligned one, and the last word is masked rather than walked.
 *
 * Divide then adjust, not (bytes + BYTES - 1) / BYTES. The rounding-up form overflows at SIZE_MAX
 * and returns zero, which would scan nothing at all.
 */
MMGR_INLINE size_t mmgr_scrut_words(size_t bytes)
{
    return (bytes / MMGR_SWAR_BYTES) + (((bytes & (MMGR_SWAR_BYTES - 1u)) != 0u) ? 1u : 0u);
}

/**
 * @brief Worst case load count for any bounded scan.
 *
 * The largest tenant is fixed at compile time and so is the word width, so this is a constant. The
 * two asserts are the derivation: the first says the cover is complete, the second says it is
 * tight. Neither a short scan nor a wasted load gets past them.
 */
#define MMGR_SCAN_MAX_WORDS ((MMGR_CONFIN_MAX + (MMGR_SWAR_BYTES - 1u)) / MMGR_SWAR_BYTES)

MMGR_STATIC_ASSERT(MMGR_SCAN_MAX_WORDS *MMGR_SWAR_BYTES >= MMGR_CONFIN_MAX,
                   "the worst-case scan does not cover the largest tenant");
MMGR_STATIC_ASSERT((MMGR_SCAN_MAX_WORDS - 1u) * MMGR_SWAR_BYTES < MMGR_CONFIN_MAX,
                   "the worst-case scan is padded by a whole word - the word count is not tight");

/**
 * @brief Every bit of the first @p n lanes in address order.
 * @param n Lane count. At or above MMGR_SWAR_BYTES keeps everything.
 * @return Byte mask.
 *
 * Masks data. Use mmgr_scrut_lanes_below to mask a result.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_bytes_below(size_t n)
{
    if (n >= MMGR_SWAR_BYTES)
    {
        return (mmgr_scrut_word) ~(mmgr_scrut_word)0;
    }
#if MMGR_HW_BIG_ENDIAN
    return (mmgr_scrut_word)((mmgr_scrut_word) ~(mmgr_scrut_word)0 << ((MMGR_SWAR_BYTES - n) * 8u));
#else
    return (mmgr_scrut_word)((mmgr_scrut_word) ~(mmgr_scrut_word)0 >> ((MMGR_SWAR_BYTES - n) * 8u));
#endif
}

/**
 * @brief High bit of the first @p n lanes in address order.
 * @param n Lane count.
 * @return Lane mask.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_lanes_below(size_t n)
{
    return (mmgr_scrut_word)(mmgr_scrut_bytes_below(n) & MMGR_VERBUM_SCRUTOR_HIGH);
}

/**
 * @brief Lane mask for word @p wi of a scan bounded at @p cap bytes.
 * @param cap Byte cap.
 * @param wi Word index.
 * @return Lane mask. Every word but the last keeps all lanes.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_tail_mask(size_t cap, size_t wi)
{
    return mmgr_scrut_lanes_below(cap - (wi * MMGR_SWAR_BYTES));
}

/**
 * @brief Lanes before the first set lane of @p m in address order.
 * @param m Lane mask. Zero keeps everything.
 * @return Lane mask.
 *
 * AND a match mask against this to drop every hit past the terminator, no compare and no branch.
 *
 * Endian derived like lane_first. Little endian: before means below, and (m-1) & ~m is that run.
 * Big endian: before means above, the complement of the downward smear.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_lanes_before(mmgr_scrut_word m)
{
#if MMGR_HW_BIG_ENDIAN
    mmgr_scrut_word s = m;
    for (unsigned k = 8u; k < MMGR_SWAR_LANE_BITS; k <<= 1)
    {
        s = (mmgr_scrut_word)(s | (mmgr_scrut_word)(s >> k));
    }
    return (mmgr_scrut_word)(~s & MMGR_VERBUM_SCRUTOR_HIGH);
#else
    return (mmgr_scrut_word)((mmgr_scrut_word)((mmgr_scrut_word)(m - 1u) & (mmgr_scrut_word)~m) &
                             MMGR_VERBUM_SCRUTOR_HIGH);
#endif
}

/**
 * @def MMGR_FAM_CS
 * @brief Family bits, case sensitive. Bits 6 and 5.
 * @def MMGR_FAM_CI
 * @brief Family bits, case insensitive. Bit 6 only - bit 5 is the case bit.
 *
 * ASCII is laid out in blocks and bits 6:5 say which:
 *
 * @verbatim
 * 00xx xxxx   control
 * 001x xxxx   space, punctuation, digits      0x20
 * 010x xxxx   upper, and @ [ \ ] ^ _          0x40
 * 011x xxxx   lower, and ` { | } ~            0x60
 * @endverbatim
 *
 * 'A' and 'a' are the same byte apart from bit 5.
 */
#define MMGR_FAM_CS 0x60u
#define MMGR_FAM_CI 0x40u

/**
 * @brief Which lanes are in family @p f.
 * @param w Word of lanes.
 * @param mask Family bits to test.
 * @param f Family to match.
 * @return High bit set in each lane in that family.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_fam_eq(mmgr_scrut_word w, unsigned mask, uint8_t f)
{
    const mmgr_scrut_word bits = (mmgr_scrut_word)(MMGR_SWAR_ONES * (mmgr_scrut_word)mask);
    return mmgr_scrut_has_zero((mmgr_scrut_word)((w & bits) ^ (MMGR_SWAR_ONES * (mmgr_scrut_word)(f & mask))));
}

/**
 * @brief Is there any upper case in this word.
 * @param w Word of lanes.
 * @return Non-zero if any lane is in the 0x40 block.
 *
 * Free at load - the word is already in a register, so this is a mask and a compare. Never scan
 * twice to ask it.
 *
 * Over-broad in the safe direction: also says yes to @ [ \ ] ^ _. A yes means maybe, a no means
 * definitely not, and the no is the useful half. Folding a word with no upper case in it cannot
 * change it, so the fold is skipped and the plain compare is exact.
 *
 * Gate only. Do not fold with this - see mmgr_scrut_fold_lower.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_any_upper(mmgr_scrut_word w)
{
    return mmgr_scrut_fam_eq(w, 0x60u, 0x40u);
}

/**
 * @brief Is there any digit in this word.
 * @param w Word of lanes.
 * @return Non-zero if any lane has high nibble 3.
 *
 * Over-broad the same way: also says yes to : ; < = > ?.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_any_digit(mmgr_scrut_word w)
{
    return mmgr_scrut_fam_eq(w, 0xF0u, 0x30u);
}

/**
 * @brief Which lanes hold a letter.
 * @param w Word of lanes.
 * @return High bit set in each letter lane.
 *
 * Family bits cannot do this one. The upper block also holds @ [ \ ] ^ _, so the low five bits have
 * to be range checked. A gate may be a superset because a false yes only costs work. A fold may
 * not, because a false yes corrupts the byte.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_alpha(mmgr_scrut_word w)
{
    const mmgr_scrut_word lo = (mmgr_scrut_word)(w | (MMGR_SWAR_ONES * 0x20u));
    return (mmgr_scrut_word)(mmgr_scrut_ge(lo, (mmgr_scrut_word)'a') & mmgr_scrut_le(lo, (mmgr_scrut_word)'z') & ~lo);
}

/**
 * @brief Force letters to lower case, leave everything else alone.
 * @param w Word of lanes.
 * @return Folded word.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_fold_lower(mmgr_scrut_word w)
{
    return (mmgr_scrut_word)(w | (mmgr_scrut_word)(mmgr_scrut_alpha(w) >> 2));
}

/**
 * @brief Reduce a lane mask to the lanes that begin a run of @p n set lanes.
 * @param m Lane mask.
 * @param n Run length.
 * @return Lane mask.
 *
 * Doubling, so a run of eight costs three steps and not seven. The shift goes toward the earlier
 * lane in address order, which is the same compile time fact lane_first derives from.
 *
 * Lanes near the end of the word see a run continuing into the next word as ending, so they come
 * back clear. Callers must OR in mmgr_scrut_run_edge to let those through.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_run(mmgr_scrut_word m, size_t n)
{
    size_t have = 1u;

    while (have < n)
    {
        const size_t step = (have < n - have) ? have : n - have;
#if MMGR_HW_BIG_ENDIAN
        m &= (mmgr_scrut_word)(m << (step * 8u));
#else
        m &= (mmgr_scrut_word)(m >> (step * 8u));
#endif
        have += step;
    }
    return m;
}

/**
 * @brief Lanes a run of @p n cannot be tested at, because it would leave the word.
 * @param n Run length.
 * @return Lane mask to OR into a run mask.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_run_edge(size_t n)
{
    if (n <= 1u || n > MMGR_SWAR_BYTES)
    {
        return 0;
    }
    return (mmgr_scrut_word)(MMGR_VERBUM_SCRUTOR_HIGH & ~mmgr_scrut_lanes_below(MMGR_SWAR_BYTES - n + 1u));
}

/**
 * @brief Load a word at any alignment.
 * @param p Source.
 * @return The word.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_load(const char *p)
{
    return (mmgr_scrut_word)mmgr_proxim_load(p, MMGR_SWAR_BYTES);
}

/**
 * @brief Load a word from an aligned address.
 * @param p Source, word aligned.
 * @return The word.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_load_al(const char *p)
{
    return (mmgr_scrut_word)mmgr_aequus_load(p, MMGR_SWAR_BYTES);
}

/**
 * @brief Per-lane difference.
 * @param wa First word.
 * @param wb Second word.
 * @return Non-zero in each lane that differs.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_xor(mmgr_scrut_word wa, mmgr_scrut_word wb)
{
    return wa ^ wb;
}

/**
 * @brief Per-lane difference, ignoring case.
 * @param wa First word.
 * @param wb Second word.
 * @return Non-zero in each lane that differs after folding.
 *
 * Clears the case bit in letter lanes only. Twelve operations against one for the plain xor, which
 * is why callers gate it on mmgr_scrut_any_upper.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_xor_ci(mmgr_scrut_word wa, mmgr_scrut_word wb)
{
    mmgr_scrut_word x = wa ^ wb;
    mmgr_scrut_word lo = wa | (MMGR_SWAR_ONES * 0x20u);
    mmgr_scrut_word alpha = mmgr_scrut_ge(lo, 'a') & mmgr_scrut_le(lo, 'z') & ~lo;
    return x & ~(alpha >> 2);
}

/**
 * @brief Which lanes equal @p c, ignoring case.
 * @param w Word of lanes.
 * @param c Byte to match.
 * @return High bit set in each matching lane.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_eq_ci(mmgr_scrut_word w, uint8_t c)
{
    return mmgr_scrut_has_zero(mmgr_scrut_xor_ci(w, MMGR_SWAR_ONES * (mmgr_scrut_word)c));
}

/**
 * @brief eq or eq_ci by @p ci.
 * @param w Word of lanes.
 * @param c Byte to match.
 * @param ci Fold case.
 * @return High bit set in each matching lane.
 *
 * @p ci is a compile time fact at every call site. Callers that reach this from a runtime value
 * must specialise first, or the branch lands in the scan loop.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_eq_sel(mmgr_scrut_word w, uint8_t c, mmgr_bool ci)
{
    if (ci)
    {
        return mmgr_scrut_eq_ci(w, c);
    }
    return mmgr_scrut_eq(w, c);
}

/**
 * @brief xor or xor_ci by @p ci.
 * @param wa First word.
 * @param wb Second word.
 * @param ci Fold case.
 * @return Non-zero in each lane that differs.
 */
MMGR_INLINE mmgr_scrut_word mmgr_scrut_xor_sel(mmgr_scrut_word wa, mmgr_scrut_word wb, mmgr_bool ci)
{
    if (ci)
    {
        return mmgr_scrut_xor_ci(wa, wb);
    }
    return mmgr_scrut_xor(wa, wb);
}

/** @brief Module namespace. const is what lets the compiler devirtualise a call through it. */
MMGR_NS VerbumScrutorNs scrut MMGR_UNUSED = {
    .ge = mmgr_scrut_ge,
    .le = mmgr_scrut_le,
    .spread = mmgr_scrut_spread,
    .sub7 = mmgr_scrut_sub7,
    .has_zero = mmgr_scrut_has_zero,
    .eq = mmgr_scrut_eq_sel,
    .xor_ = mmgr_scrut_xor_sel,
    .zero_lane = mmgr_scrut_zero_lane,
    .load = mmgr_scrut_load,
    .load_al = mmgr_scrut_load_al,
};

MMGR_END_DECLS

#endif
