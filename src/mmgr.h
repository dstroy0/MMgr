/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @file mmgr.h
 * @brief Umbrella header: pulls in the module headers a consumer builds against.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note One include for a consumer, so a program reaches the library without knowing which module
 *       holds what. A module still includes only what it needs. This file is for the caller.
 * @note mmgr_config.h comes first, since the feature switches below decide what else is included.
 * @note Every module here is unconditional. The two below are not, because a part without the
 *       hardware must not carry the code that drives it.
 * @warning mmgr_string_shim.h is deliberately not among these. Including it changes the meaning of
 *          the <string.h> names for the whole translation unit, which is a decision a consumer
 *          makes rather than one an umbrella header makes for it.
 */
#ifndef MMGR_H
#define MMGR_H

#include "config/mmgr_config.h"

#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "endian/endian.h"
#include "fractio/fractio.h"
#include "locus_carcerum/locus_carcerum.h"
#include "memoria_anularis/memoria_anularis.h"
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
 * @note memoria_externa.h guards its own contents on MMGR_ENABLE_EXTRAM as well.
 */
#if MMGR_ENABLE_EXTRAM
#include "memoria_externa/memoria_externa.h"
#endif

#endif
