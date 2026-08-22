// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_OCTETUS_INTROITUS_EXITUS_H
#define MMGR_OCTETUS_INTROITUS_EXITUS_H

#include "cellularum_laboro/cellularum_laboro.h"
#include "endian/endian.h"
#include "memoria_operor/memoria_operor.h"
#include "spatium/spatium.h"

MMGR_INCIPE_DECLS

/**
 * @file octetus_introitus_exitus.h
 * @brief Read and write byte fields, big endian on the wire.
 *
 * One shape: a fixed width scalar, or a run of bytes, moved in a chosen order. Nothing here parses
 * and nothing here can fail - what did both went to cellularum_laboro.
 *
 * The table is the whole surface. There are no free functions to call.
 */

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    void (*put)(mmgr_spat *w, uint8_t b);
    void (*put_be)(mmgr_spat *w, uint64_t val, int32_t nbytes);
    void (*raw)(mmgr_spat *w, const void *src, size_t n);
    void (*take_be)(const uint8_t *p, size_t len, size_t *off, uint64_t *out, size_t nbytes);
    void (*rd_u32)(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
} OctetusIntroitusExitusNs;
MMGR_NS_LAYOUT(OctetusIntroitusExitusNs, put, put_be, raw, take_be, rd_u32);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
void mmgr_octet_put(mmgr_spat *w, uint8_t b);
void mmgr_octet_put_be(mmgr_spat *w, uint64_t val, int32_t nbytes);
void mmgr_octet_bytes(mmgr_spat *w, const void *src, size_t n);
void mmgr_octet_take_be(const uint8_t *p, size_t len, size_t *off, uint64_t *out, size_t nbytes);
void mmgr_rd_u32(const uint8_t *p, size_t len, size_t *off, uint32_t *out);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS OctetusIntroitusExitusNs byteio MMGR_UNUSED = {
    .put = mmgr_octet_put,
    .put_be = mmgr_octet_put_be,
    .raw = mmgr_octet_bytes,
    .take_be = mmgr_octet_take_be,
    .rd_u32 = mmgr_rd_u32,
};

MMGR_FINIS_DECLS

#endif
