/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Bounded string work: the three argument types, the calls, and the cellul dispatch table.
 */
#ifndef MMGR_CELLULARUM_LABORO_H
#define MMGR_CELLULARUM_LABORO_H

#include "verbum_scrutor/verbum_scrutor.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Number of needle offsets the search sieve tests per candidate word.
 *
 * @note cellul_find_core declares rows[MMGR_SIEVE_ROWS] and reads rows[0] before any loop.
 * @warning Taken only when MMGR_SIEVE_ROWS is not already defined; a build may supply its own.
 * @warning A build's own value must be at least 1. At 0 the search declares a zero length array and
 *          still reads rows[0], and nothing here asserts against it.
 */
#ifndef MMGR_SIEVE_ROWS

#define MMGR_SIEVE_ROWS 1u
#endif

/**
 * @brief Longest haystack a one or two byte needle is settled over by a mask chain rather than by
 *        building a sieve.
 *
 * @note Defaults to no limit, which folds the test away: `read_cap <= SIZE_MAX` is true for every
 *       size_t, so a default build emits no comparison and no second path is chosen at run time.
 * @note The chain reads one word and one byte a step and settles every start position in it at
 *       once; the sieve reads one word and pays a prologue picking an anchor out of the cost table
 *       before a haystack byte is read. Measured with a two byte needle, cycles for the whole call:
 *
 *           n              8      64    2048
 *           Xtensa chain 124     489   13391
 *           Xtensa sieve 187     607   15494
 *           RISC-V chain 124     488   13393
 *           RISC-V sieve 219     680   17059
 *
 *       The chain wins at every length on both parts, which is why the default is no limit. The knob
 *       is kept because that is a measurement rather than a proof, and a part or a workload that
 *       disagrees should be able to say so without editing the walk.
 * @warning Taken only when MMGR_FIND_CHAIN_MAX is not already defined; a build may supply its own.
 *          Zero sends every needle through the sieve.
 */
#ifndef MMGR_FIND_CHAIN_MAX

#define MMGR_FIND_CHAIN_MAX SIZE_MAX
#endif

/**
 * @brief Arguments for the string calls; each reads only the members it needs.
 *
 * @note Members left unset are zero, and the calls that ignore them never read them.
 */
typedef struct
{
    const char *const src;   /**< Bytes to read [BORROWS]. */
    const size_t cap;        /**< Bytes readable from src. */
    const char *const other; /**< Second operand for diff, eq, starts and find [BORROWS]. */
    const size_t other_cap;  /**< Bytes readable from other. */
    const size_t other_len;  /**< Needle length find and has take when non-zero, rather than measuring. */
    char *const dst;         /**< Destination for copy [BORROWS]. */
    const size_t at;         /**< Offset into src for len, ws and digit. */
    const uint8_t byte;      /**< Byte sought by chr. */
    const mmgr_bool ci;      /**< Fold case in diff, eq, starts and find. */
} CatenaFinitaCfg;

/**
 * @brief Arguments for the single-step compares used to drive a walk.
 *
 * @note step_word reads wa and wb; step_byte reads ca and cb.
 */
typedef struct
{
    const mmgr_word wa;       /**< First word for step_word. */
    const mmgr_word wb;       /**< Second word for step_word. */
    const uint8_t ca;         /**< First byte for step_byte. */
    const uint8_t cb;         /**< Second byte for step_byte. */
    const mmgr_bool ci;       /**< Fold case before comparing. */
    const mmgr_bool end_wins; /**< A terminator in the same lane counts as a match. */
} VerboProgrediorCfg;

/**
 * @brief Arguments for the conversions from text to number.
 *
 * @note Every one of them reads src, and sets end when it is given.
 */
typedef struct
{
    const char *const src;  /**< Text to convert [BORROWS]. */
    const char **const end; /**< Optional target set past the last byte read [BORROWS]. */
} TransfiguroCfg;

/**
 * @brief Type of the cellul dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the seventeen members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note Byte and wire verbs are not here. rd_str and mpint_fixed read a length off the wire rather
 *       than out of a string, so they belong to the byteio module and act on its spans.
 */
typedef struct
{
    CatenaFinitaCfg (*init)(const CatenaFinitaCfg *args);    /**< Copies the argument struct. */
    size_t (*len)(const CatenaFinitaCfg *args);              /**< Bytes before the terminator. */
    size_t (*diff)(const CatenaFinitaCfg *args);             /**< Offset of the first differing byte. */
    mmgr_bool (*eq)(const CatenaFinitaCfg *args);            /**< Whether both end together with no difference. */
    mmgr_bool (*starts)(const CatenaFinitaCfg *args);        /**< Whether src begins with other. */
    const char *(*find)(const CatenaFinitaCfg *args);        /**< First occurrence of other in src. */
    mmgr_bool (*has)(const CatenaFinitaCfg *args);           /**< Whether find would report a match. */
    const char *(*chr)(const CatenaFinitaCfg *args);         /**< First occurrence of byte in src. */
    size_t (*copy)(const CatenaFinitaCfg *args);             /**< Bounded copy, always terminated. */
    mmgr_bool (*ws)(const CatenaFinitaCfg *args);            /**< Whether src[at] is whitespace. */
    mmgr_bool (*digit)(const CatenaFinitaCfg *args);         /**< Whether src[at] is a decimal digit. */
    mmgr_iword (*step_word)(const VerboProgrediorCfg *args); /**< One word compare driving a walk. */
    mmgr_iword (*step_byte)(const VerboProgrediorCfg *args); /**< One byte compare driving a walk. */
    mmgr_iword (*to_long)(const TransfiguroCfg *args);       /**< Text to signed integer. */
    mmgr_word (*to_ulong)(const TransfiguroCfg *args);       /**< Text to unsigned integer. */
    double (*to_double)(const TransfiguroCfg *args);         /**< Text to double. */
    float (*to_float)(const TransfiguroCfg *args);           /**< Text to float. */
} CellularumLaboroNs;
MMGR_NS_LAYOUT(CellularumLaboroNs, init, len, diff, eq, starts, find, has, chr, copy, ws, digit, step_word, step_byte,
               to_long, to_ulong, to_double, to_float);

/**
 * @brief Returns a copy of the argument struct.
 *
 * @param[in] args Struct to copy [BORROWS].
 * @return      A copy of *c.
 * @note Copies the members only; nothing they point at is read.
 */
CatenaFinitaCfg mmgr_cellul_init(const CatenaFinitaCfg *args);

/**
 * @brief Returns the bytes in src before its terminator, starting at args->at.
 *
 * @param[in] args Bytes src, the extent cap, and the start offset at [BORROWS].
 * @return      Bytes before the terminator, at most cap minus at.
 * @note Returns cap minus at when no terminator is found in range.
 * @warning args->at must not exceed args->cap, and src must be readable to args->cap.
 */
size_t mmgr_cellul_len(const CatenaFinitaCfg *args);

/**
 * @brief Returns the offset of the first byte where src and other differ.
 *
 * @param[in] args Bytes src and other, the extent cap, and ci [BORROWS].
 * @return      Offset of the first difference, or cap when the two agree throughout.
 * @note A terminator does not end the scan; cap is the only bound.
 * @warning Both src and other must be readable for cap bytes.
 */
size_t mmgr_cellul_diff(const CatenaFinitaCfg *args);

/**
 * @brief Reports whether src and other hold the same terminated string.
 *
 * @param[in] args Bytes src and other, the extent cap, and ci [BORROWS].
 * @return      MMGR_TRUE when both reach a terminator with no difference before it.
 * @warning Both src and other must be readable for cap bytes.
 */
mmgr_bool mmgr_cellul_eq(const CatenaFinitaCfg *args);

/**
 * @brief Reports whether src begins with other.
 *
 * @param[in] args Bytes src and other, the extent cap, and ci [BORROWS].
 * @return      MMGR_TRUE when other reaches its terminator with no difference before it.
 * @note An empty other matches any src.
 * @warning Both src and other must be readable for cap bytes.
 */
mmgr_bool mmgr_cellul_starts(const CatenaFinitaCfg *args);

/**
 * @brief Finds the first occurrence of other within src.
 *
 * @param[in] args Haystack src with cap, needle other with other_cap, and ci [BORROWS].
 * @return      Address inside src, or NULL when there is no match [BORROWS].
 * @note An empty needle returns src; a needle longer than cap returns NULL.
 * @warning src must be readable for cap bytes and other for other_cap bytes.
 */
const char *mmgr_cellul_find(const CatenaFinitaCfg *args);

/**
 * @brief Reports whether other occurs within src.
 *
 * @param[in] args Haystack src with cap, needle other with other_cap, and ci [BORROWS].
 * @return      MMGR_TRUE when mmgr_cellul_find reports a match.
 * @warning src must be readable for cap bytes and other for other_cap bytes.
 */
mmgr_bool mmgr_cellul_has(const CatenaFinitaCfg *args);

/**
 * @brief Finds the first occurrence of args->byte in src, before the terminator.
 *
 * @param[in] args Bytes src, the extent cap, and the byte sought [BORROWS].
 * @return      Address inside src, or NULL when the byte does not occur [BORROWS].
 * @note A byte of 0 returns the address of the terminator itself.
 * @warning src must be readable for cap bytes.
 */
const char *mmgr_cellul_chr(const CatenaFinitaCfg *args);

/**
 * @brief Copies src into dst, writing at most cap bytes including the terminator.
 *
 * @param[in,out] args Source src, destination dst, and the destination extent cap [BORROWS].
 * @return          Bytes copied, not counting the terminator.
 * @note A cap of 0 writes nothing at all, not even a terminator.
 * @warning dst must be writable for cap bytes and src readable for cap minus one.
 */
size_t mmgr_cellul_copy(const CatenaFinitaCfg *args);

/**
 * @brief Tests src[at] for whitespace.
 *
 * @param[in] args Bytes src and the offset at [BORROWS].
 * @return      MMGR_TRUE for space, tab, newline, carriage return, form feed or vertical tab.
 * @warning src[at] must be readable; cap does not bound this call.
 */
mmgr_bool mmgr_cellul_ws(const CatenaFinitaCfg *args);

/**
 * @brief Tests src[at] for a decimal digit.
 *
 * @param[in] args Bytes src and the offset at [BORROWS].
 * @return      MMGR_TRUE when the byte lies between '0' and '9'.
 * @warning src[at] must be readable; cap does not bound this call.
 */
mmgr_bool mmgr_cellul_digit(const CatenaFinitaCfg *args);

/**
 * @brief Compares one word pair and reports whether a walk should continue.
 *
 * @param[in] args Words wa and wb, with ci and end_wins [BORROWS].
 * @return      MMGR_SWAR_GO to continue, MMGR_SWAR_YES on agreement, MMGR_SWAR_NO on difference.
 * @note MMGR_SWAR_YES means wa's terminator arrived before the first difference.
 * @note end_wins makes a terminator in the same lane as the difference count as agreement.
 */
mmgr_iword mmgr_cellul_step_word(const VerboProgrediorCfg *args);

/**
 * @brief Compares one byte pair and reports whether a walk should continue.
 *
 * @param[in] args Bytes ca and cb, with ci and end_wins [BORROWS].
 * @return      MMGR_SWAR_GO to continue, MMGR_SWAR_YES on agreement, MMGR_SWAR_NO on difference.
 * @note A terminating ca gives MMGR_SWAR_YES when cb also terminates, or when end_wins is set.
 */
mmgr_iword mmgr_cellul_step_byte(const VerboProgrediorCfg *args);

/**
 * @brief Reads a signed decimal integer from args->src.
 *
 * @param[in,out] args Text src and the optional end target [BORROWS].
 * @return          The value, negated when a minus sign was read.
 * @note Skips leading whitespace, then accepts one optional '+' or '-'.
 * @note When end is not NULL it is set past the last digit, or back to src when none was read.
 * @warning The read stops at the first byte that is not part of the number; no length bounds it.
 * @warning The digit accumulator is mmgr_word wide and wraps on a longer run.
 */
mmgr_iword mmgr_cellul_to_long(const TransfiguroCfg *args);

/**
 * @brief Reads an unsigned decimal integer from args->src.
 *
 * @param[in,out] args Text src and the optional end target [BORROWS].
 * @return          The accumulated value.
 * @note Skips leading whitespace, then accepts one optional '+'; a '-' stops the read.
 * @note When end is not NULL it is set past the last digit, or back to src when none was read.
 * @warning The read stops at the first byte that is not part of the number; no length bounds it.
 * @warning The digit accumulator is mmgr_word wide and wraps on a longer run.
 */
mmgr_word mmgr_cellul_to_ulong(const TransfiguroCfg *args);

/**
 * @brief Reads a decimal floating point number from args->src.
 *
 * @param[in,out] args Text src and the optional end target [BORROWS].
 * @return          The assembled value.
 * @note Accepts whitespace, one optional sign, digits, one optional point, then an optional exponent.
 * @note An exponent is read only when at least one digit was seen before it.
 * @note When end is not NULL it is set past the number, or back to src when no digit was read.
 * @warning The read stops at the first byte that is not part of the number; no length bounds it.
 */
double mmgr_cellul_to_double(const TransfiguroCfg *args);

/**
 * @brief Reads a decimal floating point number from args->src and narrows it to float.
 *
 * @param[in,out] args Text src and the optional end target [BORROWS].
 * @return          The value from mmgr_cellul_to_double, narrowed to float.
 * @note Accepts the same input as mmgr_cellul_to_double; only the result width differs.
 * @warning The read stops at the first byte that is not part of the number; no length bounds it.
 */
float mmgr_cellul_to_float(const TransfiguroCfg *args);

/**
 * @brief Dispatch table instance named cellul; each member calls the matching mmgr_cellul_ function.
 */
MMGR_NS CellularumLaboroNs cellul MMGR_UNUSED = {
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

MMGR_FINIS_DECLS

#endif
