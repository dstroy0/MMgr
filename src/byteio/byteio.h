// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_BYTEIO_H
#define MMGR_BYTEIO_H

#include "cellularum_laboro/cellularum_laboro.h"
#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"
#include "spatium/spatium.h"

MMGR_BEGIN_DECLS

/**
 * @file byteio.h
 * @brief Read and write byte fields, big endian on the wire.
 */

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    void (*put)(mmgr_spat *w, uint8_t b);
    void (*put_be)(mmgr_spat *w, uint64_t val, int32_t nbytes);
    void (*raw)(mmgr_spat *w, const void *src, size_t n);
    void (*take_be)(const uint8_t *p, size_t len, size_t *off, uint64_t *out, size_t nbytes);
    void (*rd_u32)(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
    mmgr_bool (*rd_str)(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
    mmgr_bool (*mpint_fixed)(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);
} ByteioNs;
MMGR_NS_LAYOUT(ByteioNs, put, put_be, raw, take_be, rd_u32, rd_str, mpint_fixed);

/**
 * @brief Append one byte.
 * @param w Span writer.
 * @param b Byte.
 */
void mmgr_byteio_put(mmgr_spat *w, uint8_t b);
/**
 * @brief Append an integer, big endian.
 * @param w Span writer.
 * @param val Value.
 * @param nbytes How many bytes to write.
 */
void mmgr_byteio_put_be(mmgr_spat *w, uint64_t val, int32_t nbytes);
/**
 * @brief Append a run of bytes verbatim.
 * @param w Span writer.
 * @param src Source.
 * @param n Byte count.
 */
void mmgr_byteio_bytes(mmgr_spat *w, const void *src, size_t n);
/**
 * @brief Read an integer, big endian.
 * @param p Buffer.
 * @param len How far it may read.
 * @param off In/out. Cursor.
 * @param out Out. The value.
 * @param nbytes How many bytes to read.
 */
void mmgr_byteio_take_be(const uint8_t *p, size_t len, size_t *off, uint64_t *out, size_t nbytes);
/**
 * @brief Read a big endian uint32 at @p off and advance it.
 * @param p Buffer.
 * @param len Buffer length.
 * @param off In/out. Cursor.
 * @param out Out. The value.
 * @return MMGR_FALSE if the buffer is short.
 */
void mmgr_rd_u32(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
/**
 * @brief Read a length prefixed string at @p off and advance it.
 * @param p Buffer.
 * @param len Buffer length.
 * @param off In/out. Cursor.
 * @param out Out. Points into @p p. Not copied and not terminated.
 * @param slen Out. Its length.
 * @return MMGR_FALSE if the buffer is short.
 */
mmgr_bool mmgr_rd_str(const uint8_t *p, size_t len, size_t *off, const uint8_t **out, uint32_t *slen);
/**
 * @brief Copy an mpint into a fixed width field, left padded with zeros.
 * @param m mpint bytes.
 * @param mlen Its length.
 * @param out Out. Destination.
 * @param outlen Field width.
 * @return MMGR_FALSE if the value does not fit.
 */
mmgr_bool mmgr_mpint_to_fixed(const uint8_t *m, uint32_t mlen, uint8_t *out, size_t outlen);

/** @brief Module namespace. */
MMGR_NS ByteioNs byteio MMGR_UNUSED = {
    .put = mmgr_byteio_put,
    .put_be = mmgr_byteio_put_be,
    .raw = mmgr_byteio_bytes,
    .take_be = mmgr_byteio_take_be,
    .rd_u32 = mmgr_rd_u32,
    .rd_str = mmgr_rd_str,
    .mpint_fixed = mmgr_mpint_to_fixed,
};

MMGR_END_DECLS

#endif
