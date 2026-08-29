/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file impensa_ancorae_acus.h
 * @brief Byte cost lookup: its argument, the call, and the ancorae dispatch table.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note Five source files define the call, each with its own table. A build links exactly one.
 */
#ifndef MMGR_IMPENSA_ANCORAE_ACUS_H
#define MMGR_IMPENSA_ANCORAE_ACUS_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief Argument for the cost lookup.
 */
typedef struct
{
    const uint8_t byte; /**< Byte value to look up. */
} AncoraeCfg;

/**
 * @brief Type of the ancorae dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the impensa member is at offset 0 and that the struct holds nothing else.
 */
typedef struct
{
    uint8_t (*impensa)(const AncoraeCfg *args); /**< Cost of one byte value. */
} ImpensaAncoraeAcusNs;
MMGR_NS_LAYOUT(ImpensaAncoraeAcusNs, impensa);

/**
 * @brief Returns the cost of args->byte under the table this build links.
 *
 * @param[in] args Byte to look up [BORROWS].
 * @return         The cost, 1 through 255.
 * @note Lower means the byte is rarer under the linked table. cellul_pick_rows keeps the lowest it
 *       finds.
 * @warning args is dereferenced without a null check, so it must point to a readable AncoraeCfg.
 * @warning The value depends on which of the five tables was linked, so it is not portable between builds.
 */
uint8_t mmgr_ancorae_impensa(const AncoraeCfg *args);

/**
 * @brief Dispatch table instance named ancorae, whose impensa member is mmgr_ancorae_impensa.
 */
MMGR_NS ImpensaAncoraeAcusNs ancorae MMGR_UNUSED = {
    .impensa = mmgr_ancorae_impensa,
};

MMGR_FINIS_DECLS

#endif
