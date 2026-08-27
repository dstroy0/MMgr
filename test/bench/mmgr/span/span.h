/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Bench shim: the span type and boolean names ProtoCore's ring.h reaches for.
 *
 * @note Only protocore_cspan and the three boolean names are used by ring.h, so only they are here.
 * @note protocore_cspan is copied field for field from ProtoCore's span.h, since slot_hold writes
 *       every one of them and a narrower stand-in would change what the hold costs.
 */
#ifndef MMGR_BENCH_PROTO_SPAN_H
#define MMGR_BENCH_PROTO_SPAN_H

#include "config/mmgr_config.h"

/**
 * @brief ProtoCore's boolean, mapped onto MMgr's.
 */
typedef mmgr_bool proto_bool;

/** @brief True, under ProtoCore's name. */
#define PROTO_TRUE MMGR_TRUE

/** @brief False, under ProtoCore's name. */
#define PROTO_FALSE MMGR_FALSE

/**
 * @brief A read-only byte region, as ProtoCore's span.h declares it.
 *
 * @note buf is const there, so a hold records a region it cannot write through [BORROWS].
 */
typedef struct
{
    const uint8_t *buf; /**< First byte, or NULL when there is nothing to read [BORROWS]. */
    size_t len;         /**< Readable bytes at buf. */
    size_t pos;         /**< Read cursor. */
    proto_bool err;     /**< Set once a read ran past len. */
} protocore_cspan;

#endif
