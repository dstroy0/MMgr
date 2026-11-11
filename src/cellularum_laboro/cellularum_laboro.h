/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file cellularum_laboro.h
 * @brief Bounded string work: the three argument types, the calls, and the cellul dispatch table.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note A cap the caller states bounds the calls that take CatenaFinitaCfg. So no walk runs
 *       past it even where the bytes carry no terminator. That bound makes these usable
 *       where a libc string call is not. An absent terminator stops a walk and does not
 *       run it off the end.
 * @note ws and digit are the exception. Each reads the single byte at args->at and never consults
 *       cap, because a one byte test has nothing to run past.
 * @note The TransfiguroCfg conversions carry no bound at all. Each reads until the first byte that is
 *       not part of the number, and so the caller owes them such a byte inside readable storage. A
 *       terminator is the usual one.
 * @note The VerboProgrediorCfg pair take their operands by value and read no memory, and so
 *       neither carries a bound.
 * @warning Nothing here allocates or hands back ownership. Every pointer in and out is the caller's,
 *          and the addresses find and chr return point into the src they received.
 */
#ifndef MMGR_CELLULARUM_LABORO_H
#define MMGR_CELLULARUM_LABORO_H

#include "verbum_scrutor/verbum_scrutor.h"

#include "mmgr.h"

EMBED_BEGIN_DECLS

/**
 * @brief Most needle offsets the search sieve tests per candidate word.
 *
 * @note A ceiling and not a count. cellul_pick_rows takes the smallest of this, the needle length
 *       and MMGR_SWAR_BYTES. A short needle gets a sieve on fewer offsets than this allows.
 * @note cellul_find_core sizes two arrays by it, the chosen offsets and their broadcasts, and reads
 *       element 0 of each outside the loop that walks the rest.
 * @warning Taken only when MMGR_SIEVE_ROWS is not already defined. A build may supply its own.
 * @warning A build's own value must be at least 1. At 0 both arrays have zero length and the
 *          search still reads element 0 of each, and nothing here asserts against it.
 */
#ifndef MMGR_SIEVE_ROWS

#define MMGR_SIEVE_ROWS 1u
#endif

/**
 * @brief Longest haystack over which a mask chain, and not a sieve, settles a one or two byte
 *        needle.
 *
 * @note Case sensitive searches only. The chain compares its broadcasts with no fold. A
 *       folded search takes the sieve at every length, whatever value this holds.
 * @note Defaults to no limit, which folds the test away: `read_cap <= SIZE_MAX` is true for every
 *       size_t. A default build emits no comparison and chooses no second path at run time.
 * @note The chain settles every start position in a word at once, with no anchor to choose and
 *       nothing to verify afterwards. A two byte needle costs it a second read a step and a one byte
 *       needle none. The sieve picks its offsets out of the cost table before it reads a haystack
 *       byte. It reads a word per offset on top of the one it tests for the terminator. Then it
 *       proves every lane that survives in full. Measured with a two byte needle, cycles for the whole call:
 *
 *           n              8      64    2048
 *           Xtensa chain 124     489   13391
 *           Xtensa sieve 187     607   15494
 *           RISC-V chain 124     488   13393
 *           RISC-V sieve 219     680   17059
 *
 *       The chain wins at every length on both parts, and so the default is no limit. The knob
 *       stays because that is a measurement and not a proof. A part or a workload that
 *       disagrees can say so and leave the walk alone.
 * @warning Taken only when MMGR_FIND_CHAIN_MAX is not already defined. A build may supply its own.
 *          Zero sends every needle through the sieve.
 */
#ifndef MMGR_FIND_CHAIN_MAX

#define MMGR_FIND_CHAIN_MAX SIZE_MAX
#endif

/**
 * @brief Arguments for the string calls, where each call reads only the members it needs.
 *
 * @note Members left unset are zero, and the calls that ignore them never read them.
 */
typedef struct
{
    const char *const src;   /**< Bytes to read [BORROWS]. */
    const size_t cap;        /**< Bytes readable from src, and for copy the bytes writable at dst. */
    const char *const other; /**< Second operand for diff, eq, starts, find and has [BORROWS]. */
    const size_t other_cap;  /**< Bytes readable from other. */
    const size_t other_len;  /**< Needle length find and has take when non-zero, in place of a measure. */
    char *const dst;         /**< Destination for copy [BORROWS]. */
    const size_t at;         /**< Offset into src for len, ws and digit. */
    const uint8_t byte;      /**< Byte sought by chr. */
    const embed_bool ci;     /**< Fold case in diff, eq, starts, find and has. */
} CatenaFinitaCfg;

/**
 * @brief Arguments for the single-step compares used to drive a walk.
 *
 * @note step_word reads word_left and word_right. step_byte reads byte_left and byte_right.
 * @note Both operands come by value, and so neither call reads memory and neither needs a bound.
 */
typedef struct
{
    const embed_word word_left;  /**< First word for step_word. */
    const embed_word word_right; /**< Second word for step_word. */
    const uint8_t byte_left;     /**< First byte for step_byte. */
    const uint8_t byte_right;    /**< Second byte for step_byte. */
    const embed_bool ci;         /**< Fold case before the compare. */
    const embed_bool end_wins;   /**< A terminator in the same lane counts as a match. */
} VerboProgrediorCfg;

/**
 * @brief Arguments for the conversions from text to number.
 *
 * @note Every one of them reads src, and sets end when the caller passes one.
 */
typedef struct
{
    const char *const src;  /**< Text to convert [BORROWS]. */
    const char **const end; /**< Optional target set past the number, or back to src when no number came [BORROWS]. */
} TransfiguroCfg;

/**
 * @brief Type of the cellul dispatch table.
 *
 * @note EMBED_TABLE_LAYOUT asserts the seventeen members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets, with
 * nothing else.
 * @note Byte and wire verbs are not here. rd_str and mpint_fixed read a length off the wire instead
 *       of out of a string. So they belong to the byteio module and act on its spans.
 */
typedef struct
{
    CatenaFinitaCfg (*init)(const CatenaFinitaCfg *args);     /**< Copies the argument struct. */
    size_t (*len)(const CatenaFinitaCfg *args);               /**< Bytes before the terminator. */
    size_t (*diff)(const CatenaFinitaCfg *args);              /**< Offset of the first byte that differs. */
    embed_bool (*eq)(const CatenaFinitaCfg *args);            /**< Whether both end together with no difference. */
    embed_bool (*starts)(const CatenaFinitaCfg *args);        /**< Whether src begins with other. */
    const char *(*find)(const CatenaFinitaCfg *args);         /**< First occurrence of other in src. */
    embed_bool (*has)(const CatenaFinitaCfg *args);           /**< Whether find would report a match. */
    const char *(*chr)(const CatenaFinitaCfg *args);          /**< First occurrence of byte in src. */
    size_t (*copy)(const CatenaFinitaCfg *args);              /**< Bounded copy, terminated unless cap is 0. */
    embed_bool (*ws)(const CatenaFinitaCfg *args);            /**< Whether src[at] is whitespace. */
    embed_bool (*digit)(const CatenaFinitaCfg *args);         /**< Whether src[at] is a decimal digit. */
    embed_iword (*step_word)(const VerboProgrediorCfg *args); /**< One word compare that drives a walk. */
    embed_iword (*step_byte)(const VerboProgrediorCfg *args); /**< One byte compare that drives a walk. */
    embed_iword (*to_long)(const TransfiguroCfg *args);       /**< Text to signed integer. */
    embed_word (*to_ulong)(const TransfiguroCfg *args);       /**< Text to unsigned integer. */
    double (*to_double)(const TransfiguroCfg *args);          /**< Text to double. */
    float (*to_float)(const TransfiguroCfg *args);            /**< Text to float. */
} CellularumLaboroNs;
EMBED_TABLE_LAYOUT(CellularumLaboroNs, init, len, diff, eq, starts, find, has, chr, copy, ws, digit, step_word,
                   step_byte, to_long, to_ulong, to_double, to_float);

/**
 * @brief Returns a copy of the argument struct.
 *
 * @param[in] args Struct to copy [BORROWS].
 * @return         A copy of *args.
 * @note Copies the members only. It reads nothing they point at.
 */
CatenaFinitaCfg mmgr_cellul_init(const CatenaFinitaCfg *args);

/**
 * @brief Returns the bytes in src before its terminator, from args->at on.
 *
 * @param[in] args Bytes src, the extent cap, and the start offset at [BORROWS].
 * @return         Bytes before the terminator, at most cap minus at.
 * @note Returns cap minus at when no terminator lies in range.
 * @warning args->at must not exceed args->cap, and src must be readable to args->cap. The last partial
 *          word comes in whole and gets a mask after. So the read reaches up to MMGR_SWAR_BYTES - 1 bytes past cap.
 */
size_t mmgr_cellul_len(const CatenaFinitaCfg *args);

/**
 * @brief Returns the offset of the first byte where src and other differ.
 *
 * @param[in] args Bytes src and other, the extent cap, and ci [BORROWS].
 * @return         Offset of the first difference, or cap when the two agree throughout.
 * @note A terminator does not end the scan. cap is the only bound.
 * @warning Both src and other must be readable for cap bytes. The last partial word comes in whole and
 *          gets a mask after. The read reaches up to MMGR_SWAR_BYTES - 1 bytes past cap in each.
 */
size_t mmgr_cellul_diff(const CatenaFinitaCfg *args);

/**
 * @brief Reports whether src and other hold the same terminated string.
 *
 * @param[in] args Bytes src and other, the extent cap, and ci [BORROWS].
 * @return         EMBED_TRUE when both reach a terminator with no difference before it.
 * @warning Both src and other must be readable for cap bytes. The last partial word comes in whole and
 *          gets a mask after. The read reaches up to MMGR_SWAR_BYTES - 1 bytes past cap in each.
 */
embed_bool mmgr_cellul_eq(const CatenaFinitaCfg *args);

/**
 * @brief Reports whether src begins with other.
 *
 * @param[in] args Bytes src and other, the extent cap, and ci [BORROWS].
 * @return         EMBED_TRUE when other reaches its terminator with no difference before it.
 * @note An empty other matches any src.
 * @warning Both src and other must be readable for cap bytes. The last partial word comes in whole and
 *          gets a mask after. The read reaches up to MMGR_SWAR_BYTES - 1 bytes past cap in each.
 */
embed_bool mmgr_cellul_starts(const CatenaFinitaCfg *args);

/**
 * @brief Finds the first occurrence of other within src.
 *
 * @param[in] args Haystack src with cap, needle other with other_cap, and ci [BORROWS].
 * @return         Address inside src, or NULL when there is no match [BORROWS].
 * @note An empty needle returns src. A needle longer than cap returns NULL.
 * @warning src must be readable for cap bytes and other for other_cap bytes. The needle comes in a
 *          whole word at a time and gets a mask after. So the read reaches up to MMGR_SWAR_BYTES - 1
 *          bytes past other_cap. The haystack walk holds itself inside cap.
 */
const char *mmgr_cellul_find(const CatenaFinitaCfg *args);

/**
 * @brief Reports whether other occurs within src.
 *
 * @param[in] args Haystack src with cap, needle other with other_cap, and ci [BORROWS].
 * @return         EMBED_TRUE when mmgr_cellul_find reports a match.
 * @warning src must be readable for cap bytes and other for other_cap bytes, with the needle read past
 *          other_cap exactly as mmgr_cellul_find describes.
 */
embed_bool mmgr_cellul_has(const CatenaFinitaCfg *args);

/**
 * @brief Finds the first occurrence of args->byte in src, before the terminator.
 *
 * @param[in] args Bytes src, the extent cap, and the byte sought [BORROWS].
 * @return         Address inside src, or NULL when the byte does not occur [BORROWS].
 * @note A byte of 0 returns the terminator's own address, or src plus cap when no terminator is in range.
 * @warning src must be readable for cap bytes. The last partial word comes in whole and gets a mask after.
 *          The read reaches up to MMGR_SWAR_BYTES - 1 bytes past cap.
 */
const char *mmgr_cellul_chr(const CatenaFinitaCfg *args);

/**
 * @brief Copies src into dst, and writes at most cap bytes, the terminator among them.
 *
 * @param[in,out] args Source src, destination dst, and the destination extent cap [BORROWS].
 * @return             Bytes copied, with the terminator not in the count.
 * @note A cap of 0 writes nothing at all and not even a terminator.
 * @warning dst must be writable for cap bytes and src readable for cap minus one.
 */
size_t mmgr_cellul_copy(const CatenaFinitaCfg *args);

/**
 * @brief Tests src[at] for whitespace.
 *
 * @param[in] args Bytes src and the offset at [BORROWS].
 * @return         EMBED_TRUE for space, tab, newline, carriage return, form feed or vertical tab.
 * @warning src[at] must be readable. cap does not bound this call.
 */
embed_bool mmgr_cellul_ws(const CatenaFinitaCfg *args);

/**
 * @brief Tests src[at] for a decimal digit.
 *
 * @param[in] args Bytes src and the offset at [BORROWS].
 * @return         EMBED_TRUE for the ten decimal digits, '0' through '9'.
 * @warning src[at] must be readable. cap does not bound this call.
 */
embed_bool mmgr_cellul_digit(const CatenaFinitaCfg *args);

/**
 * @brief Compares one word pair and reports whether a walk should continue.
 *
 * @param[in] args Words word_left and word_right, with ci and end_wins [BORROWS].
 * @return         MMGR_SWAR_GO to continue, MMGR_SWAR_YES on agreement, MMGR_SWAR_NO on difference.
 * @note MMGR_SWAR_YES means word_left's terminator arrived before the first difference.
 * @note end_wins makes a terminator in the same lane as the difference count as agreement.
 */
embed_iword mmgr_cellul_step_word(const VerboProgrediorCfg *args);

/**
 * @brief Compares one byte pair and reports whether a walk should continue.
 *
 * @param[in] args Bytes byte_left and byte_right, with ci and end_wins [BORROWS].
 * @return         MMGR_SWAR_GO to continue, MMGR_SWAR_YES on agreement, MMGR_SWAR_NO on difference.
 * @note A byte_left that terminates gives MMGR_SWAR_YES when byte_right also terminates, or when
 *       end_wins holds.
 */
embed_iword mmgr_cellul_step_byte(const VerboProgrediorCfg *args);

/**
 * @brief Reads a signed decimal integer from args->src.
 *
 * @param[in,out] args Text src and the optional end target [BORROWS].
 * @return             The value, negated when a minus sign came first.
 * @note Skips leading whitespace, then accepts one optional '+' or '-'.
 * @note When end is not NULL the call sets it past the last digit, or back to src when no digit came.
 * @warning The read stops at the first byte that is not part of the number. No length bounds it.
 * @warning The digit accumulator is embed_word wide and wraps on a longer run.
 */
embed_iword mmgr_cellul_to_long(const TransfiguroCfg *args);

/**
 * @brief Reads an unsigned decimal integer from args->src.
 *
 * @param[in,out] args Text src and the optional end target [BORROWS].
 * @return             The accumulated value.
 * @note Skips leading whitespace, then accepts one optional '+'. A '-' stops the read.
 * @note When end is not NULL the call sets it past the last digit, or back to src when no digit came.
 * @warning The read stops at the first byte that is not part of the number. No length bounds it.
 * @warning The digit accumulator is embed_word wide and wraps on a longer run.
 */
embed_word mmgr_cellul_to_ulong(const TransfiguroCfg *args);

/**
 * @brief Reads a decimal floating point number from args->src.
 *
 * @param[in,out] args Text src and the optional end target [BORROWS].
 * @return             The assembled value.
 * @note Accepts whitespace, one optional sign, digits, one optional point, then an optional exponent.
 * @note The exponent counts only when at least one digit came before it.
 * @note When end is not NULL the call sets it past the number, or back to src when no digit came.
 * @warning The read stops at the first byte that is not part of the number. No length bounds it.
 */
double mmgr_cellul_to_double(const TransfiguroCfg *args);

/**
 * @brief Reads a decimal floating point number from args->src and narrows it to float.
 *
 * @param[in,out] args Text src and the optional end target [BORROWS].
 * @return             The value from mmgr_cellul_to_double, narrowed to float.
 * @note Accepts the same input as mmgr_cellul_to_double, with only a different result width.
 * @warning The read stops at the first byte that is not part of the number. No length bounds it.
 */
float mmgr_cellul_to_float(const TransfiguroCfg *args);

/**
 * @brief Dispatch table instance named cellul, with each member set to its mmgr_cellul_ function.
 */
EMBED_TABLE_STORAGE CellularumLaboroNs cellul EMBED_UNUSED = {
    .init = mmgr_cellul_init,
    .len = mmgr_cellul_len,
    .diff = mmgr_cellul_diff,
    .eq = mmgr_cellul_eq,
    .starts = mmgr_cellul_starts,
    .find = mmgr_cellul_find,
    .has = mmgr_cellul_has,
    .chr = mmgr_cellul_chr,
    .copy = mmgr_cellul_copy,
    .ws = mmgr_cellul_ws,
    .digit = mmgr_cellul_digit,
    .step_word = mmgr_cellul_step_word,
    .step_byte = mmgr_cellul_step_byte,
    .to_long = mmgr_cellul_to_long,
    .to_ulong = mmgr_cellul_to_ulong,
    .to_double = mmgr_cellul_to_double,
    .to_float = mmgr_cellul_to_float,
};

EMBED_END_DECLS

#endif
