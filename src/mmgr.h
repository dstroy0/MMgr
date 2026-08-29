/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file mmgr.h
 * @brief Umbrella header: pulls in the module headers a consumer builds against.
 *
 * @note mmgr_config.h comes first, since the feature switches below decide what else is included.
 * @warning mmgr_string_shim.h is not among these; including it changes the meaning of the <string.h> names.
 */
#ifndef MMGR_H
#define MMGR_H

#include "config/mmgr_config.h"

#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"
#include "carceribus/carceribus.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"
#include "endian/endian.h"
#include "fractio/fractio.h"
#include "memoria_operor/memoria_operor.h"
#include "numeros_scribo/numeros_scribo.h"
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "proximus_operor/proximus_operor.h"
#include "spatium/spatium.h"
#include "verba_scribo/verba_scribo.h"
#include "verbum_scrutor/verbum_scrutor.h"

/**
 * @brief The DMA module, reached only when MMGR_ENABLE_DMA is set.
 *
 * @note memoriam_praetereo.h guards its own contents on MMGR_ENABLE_DMA as well.
 */
#if MMGR_ENABLE_DMA
#include "memoriam_praetereo/memoriam_praetereo.h"
#endif

/**
 * @brief The external memory module, reached only when MMGR_ENABLE_EXTRAM is set.
 *
 * @note confinium_externum.h guards its own contents on MMGR_ENABLE_EXTRAM as well.
 */
#if MMGR_ENABLE_EXTRAM
#include "confinium_externum/confinium_externum.h"
#endif

#endif
