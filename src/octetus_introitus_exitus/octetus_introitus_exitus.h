/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file octetus_introitus_exitus.h
 * @brief Byte verbs over a span: append into one being filled, take out of one being read.
 *
 * @note These act on a caller's span and hold nothing of their own. The span carries the cursor and
 *       the sticky flag, so a caller may append a whole message and test once at the end.
 * @note The appends and the takes fail differently on purpose. An append that does not fit is a build
 *       failure - what a writer emits and how big its buffer is are both fixed before the build - so
 *       it asserts, stores nothing, and latches overflow to keep a wrong program off the end. That is
 *       why no append returns anything: there is no answer for a caller to act on.
 * @note A take that reaches past the end sets the read span's err, leaves the cursor and the output
 *       where they were, and answers MMGR_FALSE. That is a runtime fact, not a build failure, because
 *       how much a reader is handed is settled by whatever sent it. Reads do not advance on failure,
 *       because a caller that keeps reading after one wants the cursor to still mean something.
 * @note Whole values move a word at a time: the big endian entries reverse once and then store or
 *       load at the widest step the count allows, rather than walking bytes.
 */
#ifndef MMGR_OCTETUS_INTROITUS_EXITUS_H
#define MMGR_OCTETUS_INTROITUS_EXITUS_H

#include "spatium/spatium.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Arguments for the byteio calls; each reads only what it needs.
 *
 * @note put reads w and byte; put_be reads w, val and bytes; raw reads w, src and bytes;
 *       take_be reads r, bytes and out.
 */
typedef struct
{
    mmgr_span *const w;         /**< Span an append writes into, or the field mpint_fixed fills [BORROWS]. */
    mmgr_cspan *const r;        /**< Span a take reads from [BORROWS]. */
    const uint8_t *const src;   /**< Bytes raw appends, or the integer mpint_fixed reads [BORROWS]. */
    uint64_t *const out;        /**< Where take_be stores the value it read [BORROWS]. */
    const uint8_t **const blob; /**< Where rd_str points at the payload it found [BORROWS]. */
    size_t *const blen;         /**< Where rd_str stores that payload's length [BORROWS]. */
    const uint64_t val;         /**< Value put_be writes, taken from its low bytes. */
    const size_t bytes;         /**< Bytes the call moves; 1 through 8 for the big endian entries. */
    const uint8_t byte;         /**< The single byte put appends. */
} OctetusCfg;

/**
 * @brief Type of the byteio dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the six members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note The first three append and answer nothing, because a span latches its own failure. The last
 *       three answer, because a read that did not happen has to be distinguishable from one that read
 *       a zero.
 */
typedef struct
{
    void (*put)(const OctetusCfg *args);              /**< Appends one byte. */
    void (*put_be)(const OctetusCfg *args);           /**< Appends bytes of a value, most significant first. */
    void (*raw)(const OctetusCfg *args);              /**< Appends a run of bytes as they are. */
    mmgr_bool (*take_be)(const OctetusCfg *args);     /**< Reads a big endian value and advances past it. */
    mmgr_bool (*rd_str)(const OctetusCfg *args);      /**< Reads a length-prefixed run and points at it. */
    mmgr_bool (*mpint_fixed)(const OctetusCfg *args); /**< Right-aligns an integer into a fixed field. */
} OctetusIntroitusExitusNs;
MMGR_NS_LAYOUT(OctetusIntroitusExitusNs, put, put_be, raw, take_be, rd_str, mpint_fixed);

/**
 * @brief Appends args->byte to args->w.
 *
 * @param[in,out] args Span and the byte to append [BORROWS].
 * @warning Appending past the span's cap is a build failure. It asserts, stores nothing and latches
 *          overflow; pos counts the byte either way.
 */
void mmgr_byteio_put(const OctetusCfg *args);

/**
 * @brief Appends the low args->bytes of args->val to args->w, most significant byte first.
 *
 * @param[in,out] args Span, value and byte count [BORROWS].
 * @note The value is reversed once and then stored at the widest step the count allows, so a count of
 *       eight is one store and a count of seven is three.
 * @warning Appending past the span's cap is a build failure. It asserts, stores nothing and latches
 *          overflow; pos advances either way.
 * @warning args->bytes must be 1 through 8.
 */
void mmgr_byteio_put_be(const OctetusCfg *args);

/**
 * @brief Appends args->bytes from args->src to args->w as they are.
 *
 * @param[in,out] args Span, source and byte count [BORROWS].
 * @warning Appending past the span's cap is a build failure. It asserts, stores nothing and latches
 *          overflow; pos advances either way.
 * @warning args->src must be readable for args->bytes, and must not overlap the span's buffer.
 */
void mmgr_byteio_raw(const OctetusCfg *args);

/**
 * @brief Reads a big endian value of args->bytes at args->r's cursor and advances past it.
 *
 * @param[in,out] args Span, byte count and where to store the value [BORROWS].
 * @return          MMGR_TRUE when the bytes were there, MMGR_FALSE when the span was short.
 * @note Reads at the cursor and nowhere else: a codec that leads with a tag advances past it itself.
 * @note A read reaching past the end sets the span's err and leaves the cursor and args->out untouched.
 * @warning args->bytes must be 1 through 8.
 */
mmgr_bool mmgr_byteio_take_be(const OctetusCfg *args);

/**
 * @brief Reads a big endian 32-bit length at args->r's cursor, then points args->blob at the run behind it.
 *
 * @param[in,out] args Span, and where to report the run and its length [BORROWS].
 * @return          MMGR_TRUE when the length and its run both lay within the span.
 * @note Nothing is copied: args->blob points into the span's own bytes, so it lives only as long as they
 *       do [BORROWS].
 * @note The cursor advances past the length and the run together, so a caller reading a sequence of
 *       these needs to track nothing between them.
 * @note A run reaching past the end leaves the cursor where it started, sets the span's err, and
 *       writes nothing through args->blob or args->blen. The length alone having been read does not move
 *       the cursor either: a partial read is not a read.
 */
mmgr_bool mmgr_byteio_rd_str(const OctetusCfg *args);

/**
 * @brief Right-aligns the big endian integer at args->src into args->w's whole buffer, zero filling ahead of it.
 *
 * @param[in,out] args The integer with its length in args->src and args->bytes, and the field as args->w [BORROWS].
 * @return          MMGR_TRUE when the integer fits the field, MMGR_FALSE when it does not.
 * @note Leading zero bytes of the integer are skipped before the width is tested, so a value carrying
 *       a sign byte still fits a field of its own size.
 * @note The field is written whole, not appended to: on success args->w's cursor ends at its cap.
 * @note Nothing is written to the field when it returns MMGR_FALSE.
 */
mmgr_bool mmgr_byteio_mpint_fixed(const OctetusCfg *args);

/**
 * @brief Dispatch table instance named byteio; each member calls the matching mmgr_byteio_ function.
 */
MMGR_NS OctetusIntroitusExitusNs byteio MMGR_UNUSED = {
    .put = mmgr_byteio_put,
    .put_be = mmgr_byteio_put_be,
    .raw = mmgr_byteio_raw,
    .take_be = mmgr_byteio_take_be,
    .rd_str = mmgr_byteio_rd_str,
    .mpint_fixed = mmgr_byteio_mpint_fixed,
};

MMGR_FINIS_DECLS

#endif
