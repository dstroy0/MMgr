// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ENDIAN_H
#define MMGR_ENDIAN_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file endian.h
 * @brief Read and write fixed width integers in an explicit byte order.
 *
 * Explicit on both sides, so nothing here depends on what the host does. MMGR_HW_BIG_ENDIAN only
 * ever selects which end of a SWAR mask holds the first byte; it does not reach these.
 *
 * The table is the whole surface. There are no free functions to call.
 */

/**
 * @brief How wide a fixed width read or write is.
 *
 * The value is the byte count, so the context takes it as one and nothing converts. Packed, because
 * it sits in a config the caller builds at every call site and an int there is three bytes of
 * nothing.
 */
typedef enum MMGR_ENUM_PACKED
{
    MMGR_ENDIAN_16 = 2,
    MMGR_ENDIAN_32 = 4,
    MMGR_ENDIAN_64 = 8,
} mmgr_endian_width;

/**
 * @brief What a fixed width read or write is given.
 *
 * Public, and in the header, because the caller is what builds it: the values go in declaration
 * order and the ones left out are zero, so a write is @c {p, 0, v, MMGR_ENDIAN_32} and a read is
 * @c {0, p, 0, MMGR_ENDIAN_32}.
 *
 * Not the module's context. That is EndianCtx and it stays in the .c, because what the bodies work
 * with is nobody else's business and a header that shows it has handed that out for good.
 *
 * Every member is const. Nothing writes to a config once the caller has built it, and the compound
 * literal is gone before there is code to write to it.
 */
typedef struct
{
    uint8_t *const w;             /**< Destination, when writing. */
    const uint8_t *const r;       /**< Source, when reading. */
    const uint64_t v;             /**< The value, when writing. */
    const mmgr_endian_width n;    /**< How wide. */
} EndianCfg;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    size_t (*wr)(const EndianCfg *c);
    uint64_t (*rd)(const EndianCfg *c);
} EndianNs;
MMGR_NS_LAYOUT(EndianNs, wr, rd);

/** @name The entries the tables point at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The tables are
 *         still the whole surface: call through them.
 *  @{ */
size_t mmgr_wr_le(const EndianCfg *c);
uint64_t mmgr_rd_le(const EndianCfg *c);
size_t mmgr_wr_be(const EndianCfg *c);
uint64_t mmgr_rd_be(const EndianCfg *c);
/** @} */

/**
 * @brief Module namespaces, one per byte order.
 *
 * Two tables rather than one set of entries carrying the order in their names. The order is the
 * namespace and the width is the caller's, and those are separate choices - spelling both into
 * twelve entry names made them look like one. A second static const table costs nothing, and both
 * point at bodies over the same internal context.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS EndianNs parva_extremitas MMGR_UNUSED = {
    .wr = mmgr_wr_le,
    .rd = mmgr_rd_le,
};

MMGR_NS EndianNs magna_extremitas MMGR_UNUSED = {
    .wr = mmgr_wr_be,
    .rd = mmgr_rd_be,
};

MMGR_FINIS_DECLS

#endif
