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

/**
 * @brief What a span is made from.
 *
 * Public, and in the header, because the caller is what builds it: the values go in declaration
 * order and the ones left out are zero.
 *
 * Neither the span nor the module's context. mmgr_spat is what comes back and SpatCtx is what the
 * body works with; this is only what goes in. Three types where a flat argument list would have
 * been one is the price of the entry taking one parameter, and it is paid once, here.
 *
 * Both members are const. Nothing writes to a config once the caller has built it, and the
 * compound literal is gone before there is code to write to it.
 */
typedef struct
{
    uint8_t *const buf; /**< The memory the span will view. */
    const size_t cap;   /**< Its size. */
} SpatCfg;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    mmgr_spat (*init)(const SpatCfg *c);
} SpatiumNs;
MMGR_NS_LAYOUT(SpatiumNs, init);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
mmgr_spat mmgr_spat_init(const SpatCfg *c);
/** @} */

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS SpatiumNs spat MMGR_UNUSED = {
    .init = mmgr_spat_init,
};

MMGR_FINIS_DECLS

#endif
