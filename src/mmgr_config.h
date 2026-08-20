// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_CONFIG_H
#define MMGR_CONFIG_H

#include <stdint.h>

#ifndef MMGR_WORD_BITS
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#define MMGR_WORD_BITS 64
#elif UINTPTR_MAX == 0xFFFFFFFFu
#define MMGR_WORD_BITS 32
#elif UINTPTR_MAX == 0xFFFFu
#define MMGR_WORD_BITS 16
#else
#error "cannot derive MMGR_WORD_BITS from UINTPTR_MAX on this target - pass -DMMGR_WORD_BITS=16|32|64"
#endif
#endif

#ifndef MMGR_INDEX_BITS
#define MMGR_INDEX_BITS 32
#endif

#include "mmgr_types.h"

#ifndef MMGR_SWAR_BITS
#define MMGR_SWAR_BITS MMGR_WORD_BITS
#endif

MMGR_STATIC_ASSERT(MMGR_SWAR_BITS == 16 || MMGR_SWAR_BITS == 32 || MMGR_SWAR_BITS == 64,
                   "MMGR_SWAR_BITS must be 16, 32 or 64");
MMGR_STATIC_ASSERT(MMGR_SWAR_BITS <= MMGR_WORD_BITS,
                   "MMGR_SWAR_BITS exceeds MMGR_WORD_BITS - a lane wider than the register it is scanned in "
                   "does not degrade, it reads the wrong bytes");

#ifndef MMGR_HW_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define MMGR_HW_BIG_ENDIAN 1
#else
#define MMGR_HW_BIG_ENDIAN 0
#endif
#endif

#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
#endif

#ifndef MMGR_DEBUG_CHECKS
#define MMGR_DEBUG_CHECKS 0
#endif

#ifndef MMGR_WORKER_COUNT
#define MMGR_WORKER_COUNT 1
#endif
#ifndef MMGR_GHOST_WORKER_SLOT
#define MMGR_GHOST_WORKER_SLOT (MMGR_WORKER_COUNT)
#endif

#if (MMGR_WORKER_COUNT != 1) || MMGR_DEBUG_CHECKS
#define MMGR_NEEDS_CONTEXT_ID 1
#else
#define MMGR_NEEDS_CONTEXT_ID 0
#endif

#if MMGR_NEEDS_CONTEXT_ID
uintptr_t mmgr_platform_context_id(void);
#endif

#ifndef MMGR_PLAINTEXT_ARENA_SIZE
#define MMGR_PLAINTEXT_ARENA_SIZE 4096u
#endif
#ifndef MMGR_SECURE_ARENA_SIZE
#define MMGR_SECURE_ARENA_SIZE 4096u
#endif

#ifndef MMGR_ENABLE_DMA
#define MMGR_ENABLE_DMA 0
#endif
#ifndef MMGR_ENABLE_PSRAM_POOL
#define MMGR_ENABLE_PSRAM_POOL 0
#endif

#if MMGR_ENABLE_DMA
#ifndef MMGR_DMA_CHANNELS
#define MMGR_DMA_CHANNELS 2
#endif
#ifndef MMGR_DMA_BUF_SIZE
#define MMGR_DMA_BUF_SIZE 256
#endif
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define MMGR_TU_PRAGMA(directive) _Pragma(#directive)
#else
#define MMGR_TU_PRAGMA(directive)
#endif
#define MMGR_OPTIMIZE_O2 MMGR_TU_PRAGMA(GCC optimize("O2"))

#endif
