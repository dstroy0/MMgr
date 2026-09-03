/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file mmgr_praet.c
 * @brief Pulls the library's own DMA translation unit into this project.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note This is src, compiled for the part. The suite reaches the four mmgr_praet_ entries through
 *       it, and the engine under the suite is what answers the hooks underneath.
 * @note MMGR_PRAET_CHANNELS and MMGR_PRAET_BUF_SIZE arrive from platformio.ini. Nothing in the tree
 *       supplies them yet, which is the same reason a CMake build with DMA on needs them passed in.
 */
#include "../../../../src/memoriam_praetereo/memoriam_praetereo.c"
