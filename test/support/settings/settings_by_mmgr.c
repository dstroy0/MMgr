/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file settings_by_mmgr.c
 * @brief The settings check through mmgr.h: the ring header reaches mmgr.h before config/mmgr_config.h.
 *
 * @note This file compiles with no definition of either value, and with mmgr_settings.h on its include path.
 *       The compile stops where mmgr.h did not read that header.
 */
#include "memoria_anularis/memoria_anularis.h"

EMBED_STATIC_ASSERT(MMGR_RING_LOCULI == 16u, "mmgr.h did not read MMGR_RING_LOCULI from mmgr_settings.h");
EMBED_STATIC_ASSERT(MMGR_RING_WORDS == 72u, "mmgr.h did not read MMGR_RING_WORDS from mmgr_settings.h");

/** @brief One object. The translation unit is thus not empty. */
const size_t mmgr_settings_by_mmgr = sizeof(mmgr_ring);
