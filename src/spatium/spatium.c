// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "spatium/spatium.h"

/**
 * @file spatium.c
 * @brief Bounded views over caller memory. A span owns nothing.
 *
 * The configuration a caller wrote is already the argument list, so there is no context here to
 * build one out of. mmgr_spat_cfg is what the macro in the header assembles and what these read.
 */

/**
 * @brief Check the configuration is one a span can be made from.
 * @param cfg The configuration.
 *
 * A null buffer or a zero capacity is a caller that has not decided what it is writing into, which
 * is a program that should not have been built rather than a state to hand back. MMGR_ASSERT says
 * so: nothing in a shipping build, an abort in the checks build.
 */
MMGR_INLINE void spat_check(const mmgr_spat_cfg *cfg)
{
    MMGR_ASSERT(cfg->buf != NULL, "a span needs a buffer");
    MMGR_ASSERT(cfg->cap != 0, "a span needs a capacity");
}

/**
 * @brief Build the span.
 * @param cfg The configuration.
 * @return The span, empty.
 */
MMGR_INLINE mmgr_spat spat_from(const mmgr_spat_cfg *cfg)
{
    spat_check(cfg);

    mmgr_spat s;
    s.buf = cfg->buf;
    s.cap = cfg->cap;
    s.pos = 0;
    return s;
}

mmgr_spat mmgr_spat_from_backend(const mmgr_spat_cfg *cfg)
{
    return spat_from(cfg);
}
