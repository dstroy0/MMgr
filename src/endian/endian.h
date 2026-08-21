// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_ENDIAN_H
#define MMGR_ENDIAN_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

/**
 * @file endian.h
 * @brief Read and write fixed width integers in an explicit byte order.
 *
 * Explicit on both sides, so nothing here depends on what the host does. MMGR_HW_BIG_ENDIAN only
 * ever selects which end of a SWAR mask holds the first byte; it does not reach these.
 */

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    size_t (*wr16le)(uint8_t *p, uint16_t v);
    size_t (*wr32le)(uint8_t *p, uint32_t v);
    size_t (*wr64le)(uint8_t *p, uint64_t v);
    uint16_t (*rd16le)(const uint8_t *p);
    uint32_t (*rd32le)(const uint8_t *p);
    uint64_t (*rd64le)(const uint8_t *p);
    size_t (*wr16be)(uint8_t *p, uint16_t v);
    size_t (*wr32be)(uint8_t *p, uint32_t v);
    size_t (*wr64be)(uint8_t *p, uint64_t v);
    uint16_t (*rd16be)(const uint8_t *p);
    uint32_t (*rd32be)(const uint8_t *p);
    uint64_t (*rd64be)(const uint8_t *p);
} EndianNs;
MMGR_NS_LAYOUT(EndianNs, wr16le, wr32le, wr64le, rd16le, rd32le, rd64le, wr16be, wr32be, wr64be, rd16be, rd32be,
               rd64be);

/**
 * @brief Write a uint16 in little endian order.
 * @param p Destination. At least 2 bytes.
 * @param v Value.
 * @return Bytes written, always 2.
 */
size_t mmgr_wr16le(uint8_t *p, uint16_t v);
/**
 * @brief Write a uint32 in little endian order.
 * @param p Destination. At least 4 bytes.
 * @param v Value.
 * @return Bytes written, always 4.
 */
size_t mmgr_wr32le(uint8_t *p, uint32_t v);
/**
 * @brief Write a uint64 in little endian order.
 * @param p Destination. At least 8 bytes.
 * @param v Value.
 * @return Bytes written, always 8.
 */
size_t mmgr_wr64le(uint8_t *p, uint64_t v);
/**
 * @brief Read a uint16 in little endian order.
 * @param p Source. At least 2 bytes.
 * @return The value.
 */
uint16_t mmgr_rd16le(const uint8_t *p);
/**
 * @brief Read a uint32 in little endian order.
 * @param p Source. At least 4 bytes.
 * @return The value.
 */
uint32_t mmgr_rd32le(const uint8_t *p);
/**
 * @brief Read a uint64 in little endian order.
 * @param p Source. At least 8 bytes.
 * @return The value.
 */
uint64_t mmgr_rd64le(const uint8_t *p);
/**
 * @brief Write a uint16 in big endian order.
 * @param p Destination. At least 2 bytes.
 * @param v Value.
 * @return Bytes written, always 2.
 */
size_t mmgr_wr16be(uint8_t *p, uint16_t v);
/**
 * @brief Write a uint32 in big endian order.
 * @param p Destination. At least 4 bytes.
 * @param v Value.
 * @return Bytes written, always 4.
 */
size_t mmgr_wr32be(uint8_t *p, uint32_t v);
/**
 * @brief Write a uint64 in big endian order.
 * @param p Destination. At least 8 bytes.
 * @param v Value.
 * @return Bytes written, always 8.
 */
size_t mmgr_wr64be(uint8_t *p, uint64_t v);
/**
 * @brief Read a uint16 in big endian order.
 * @param p Source. At least 2 bytes.
 * @return The value.
 */
uint16_t mmgr_rd16be(const uint8_t *p);
/**
 * @brief Read a uint32 in big endian order.
 * @param p Source. At least 4 bytes.
 * @return The value.
 */
uint32_t mmgr_rd32be(const uint8_t *p);
/**
 * @brief Read a uint64 in big endian order.
 * @param p Source. At least 8 bytes.
 * @return The value.
 */
uint64_t mmgr_rd64be(const uint8_t *p);

/** @brief Module namespace. */
MMGR_NS EndianNs endian MMGR_UNUSED = {.wr16le = mmgr_wr16le,
                                       .wr32le = mmgr_wr32le,
                                       .wr64le = mmgr_wr64le,
                                       .rd16le = mmgr_rd16le,
                                       .rd32le = mmgr_rd32le,
                                       .rd64le = mmgr_rd64le,
                                       .wr16be = mmgr_wr16be,
                                       .wr32be = mmgr_wr32be,
                                       .wr64be = mmgr_wr64be,
                                       .rd16be = mmgr_rd16be,
                                       .rd32be = mmgr_rd32be,
                                       .rd64be = mmgr_rd64be};

MMGR_END_DECLS

#endif
