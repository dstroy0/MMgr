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
 * There is no read side. A read is a pointer, how far it may go, and where it is - which is the
 * argument list byteio's readers already take, and the same one cellularum_laboro passes as
 * (s, read_cap). A struct holding those three added a second spelling and no information.
 *
 * The table is the whole surface. There are no free functions to call.
 */

/** @brief Writable span. */
typedef struct
{
    uint8_t *buf; /**< The memory. */
    size_t cap;   /**< Its size. Read by the assert, and by nothing else. */
    size_t pos;   /**< How far along it we are. */
} mmgr_spat;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_spat (*from)(uint8_t *buf, size_t cap);
} SpatiumNs;
MMGR_NS_LAYOUT(SpatiumNs, from);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
mmgr_spat mmgr_spat_from(uint8_t *buf, size_t cap);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS SpatiumNs spat MMGR_UNUSED = {
    .from = mmgr_spat_from,
};

MMGR_FINIS_DECLS

#endif
