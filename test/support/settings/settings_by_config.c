/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file settings_by_config.c
 * @brief The settings check through config/mmgr_config.h, the header that a module reaches before mmgr.h.
 *
 * @note This file compiles with no definition of either value, and with mmgr_settings.h on its include path.
 *       The compile stops where config/mmgr_config.h did not read that header.
 */
#include "config/mmgr_config.h"

#if !defined(MMGR_RING_LOCULI) || !defined(MMGR_RING_WORDS)
#error "config/mmgr_config.h did not read mmgr_settings.h"
#endif

EMBED_STATIC_ASSERT(MMGR_RING_LOCULI == 16u, "config/mmgr_config.h read another MMGR_RING_LOCULI");

/** @brief One object. The translation unit is thus not empty. */
const unsigned mmgr_settings_by_config = MMGR_RING_WORDS;
