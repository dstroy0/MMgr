/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Bench shim: the one rawmemcpy symbol ProtoCore's ring.h reaches for.
 *
 * @note Only proto_raw_read is used by ring.h, so only it is provided; the body maps onto
 *       MMgr's proxim.read, which is the same span move under the other name.
 * @note This exists so ring.h compiles verbatim, without dragging in protocore_config.h and the
 *       platform layer behind it.
 */
#ifndef MMGR_BENCH_PROTO_RAWMEMCPY_H
#define MMGR_BENCH_PROTO_RAWMEMCPY_H

#include "config/mmgr_config.h"
#include "proximus_operor/proximus_operor.h"

/**
 * @brief Moves sz bytes from p to dst at any alignment.
 *
 * @param[out] dst Destination [BORROWS].
 * @param[in]  p   Source [BORROWS].
 * @param[in]  sz  Bytes to move.
 * @note Forwards to proxim.read, so the destination is aligned before the word run exactly as
 *       ProtoCore's own proto_raw_read does.
 */
static inline void proto_raw_read(void *dst, const void *p, size_t sz)
{
    MMGR_CALL(proxim.read, ProximusCfg, .dst = dst, .at = p, .size = sz);
}

#endif
