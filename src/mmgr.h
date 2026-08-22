// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_H
#define MMGR_H

/**
 * @file mmgr.h
 * @brief Everything, in one include. Also the Arduino compatibility name.
 *
 * A translation unit that wants one module should include that module. This exists so a sketch can
 * say `#include <mmgr.h>` and be done.
 */

#include "config/mmgr_config.h"

#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "custodia_soluta/custodia_soluta.h"
#include "carceribus/carceribus.h"
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"
#include "endian/endian.h"
#include "fractio/fractio.h"
#include "memoria_operor/memoria_operor.h"
#include "numeros_scribo/numeros_scribo.h"
#include "custodia_secura/custodia_secura.h"
#include "proximus_operor/proximus_operor.h"
#include "spatium/spatium.h"
#include "verba_scribo/verba_scribo.h"
#include "verbum_scrutor/verbum_scrutor.h"

#if MMGR_ENABLE_DMA
#include "dma/dma.h"
#endif

#if MMGR_ENABLE_EXTRAM
#include "confinium_externum/confinium_externum.h"
#endif

#endif
