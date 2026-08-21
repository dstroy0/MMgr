// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_SPATIUM_H
#define MMGR_SPATIUM_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @file spatium.h
 * @brief Bounded views over caller memory. A span owns nothing and never allocates.
 *
 * A span is where the memory is and how far along it we are. It does not carry whether a write
 * fitted, because a write that does not fit is not a thing that happens at run time - the caller
 * knows the buffer and knows the length, both at the point the call is written, so the two are
 * either compatible or the program is wrong. MMGR_ASSERT is where that is said. It costs nothing
 * in a shipping build and aborts in the checks build, which every suite already runs against.
 *
 * @c cap is kept for the assert to read. Nothing else looks at it.
 *
 * There is no read side. A read is a pointer, how far it may go, and where it is - which is
 * the argument list byteio's readers already take, and the same one cellularum_laboro passes
 * as (s, read_cap). A struct holding those three added a second spelling and no information.
 */

/** @brief Writable span. */
typedef struct
{
    uint8_t *buf;
    size_t cap;
    size_t pos;
} mmgr_spat;

/** @brief What a span is built from. */
typedef struct
{
    uint8_t *buf; /**< Buffer. */
    size_t cap;   /**< Its size. */
} mmgr_spat_cfg;

/**
 * @brief Wrap a buffer.
 * @param cfg The configuration.
 * @return The span, empty.
 */
mmgr_spat mmgr_spat_from_backend(const mmgr_spat_cfg *cfg);

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_spat (*from_impl)(const mmgr_spat_cfg *cfg);
} SpatiumNs;
MMGR_NS_LAYOUT(SpatiumNs, from_impl);

/** @brief Module namespace. */
MMGR_NS SpatiumNs spat MMGR_UNUSED = {.from_impl = mmgr_spat_from_backend};

/**
 * @brief Build the configuration where the call is written.
 *
 *     spat.from(mem, sizeof mem);   ->   spat.from_impl(&(const mmgr_spat_cfg){mem, sizeof mem});
 *
 * A function-like macro expands wherever its name is followed by an open parenthesis, and the
 * member access in front does not stop that. The replacement names from_impl, so nothing expands
 * twice, and the member declaration, the initializer and the layout assert are untouched because
 * in none of those is the name followed by an open parenthesis.
 */
#define from(...) from_impl(&(const mmgr_spat_cfg){__VA_ARGS__})

MMGR_FINIS_DECLS

#endif
