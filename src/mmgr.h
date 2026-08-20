// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_H
#define MMGR_H

#include "mmgr_config.h"

#include "bitio/bitio.h"
#include "byteio/byteio.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "clarus_custodiae/clarus_custodiae.h"
#include "confinium/confinium.h"
#include "confinium_exclusivum_infinitas/confinium_exclusivum_infinitas.h"
#include "endian/endian.h"
#include "fractio/fractio.h"
#include "memoria_operor/memoria_operor.h"
#include "numeros_scribo/numeros_scribo.h"
#include "occultum_custodiae/occultum_custodiae.h"
#include "proximus_operor/proximus_operor.h"
#include "spatium/spatium.h"
#include "verba_scribo/verba_scribo.h"
#include "verbum_scrutor/verbum_scrutor.h"

#if MMGR_ENABLE_DMA
#include "dma/dma.h"
#endif

#if MMGR_ENABLE_PSRAM_POOL
#include "confinium_externum/confinium_externum.h"
#endif

#endif
