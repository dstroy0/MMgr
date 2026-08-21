// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "spatium/spatium.h"

/**
 * @file spatium.c
 * @brief Bounded views over caller memory. A span owns nothing.
 */

mmgr_spat mmgr_spat_from_backend(const mmgr_spat_cfg *cfg)
{
    MMGR_ASSERT(cfg->buf != NULL, "a span needs a buffer");
    MMGR_ASSERT(cfg->cap != 0, "a span needs a capacity");

    mmgr_spat s;
    s.buf = cfg->buf;
    s.cap = cfg->cap;
    s.pos = 0;
    return s;
}
