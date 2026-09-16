/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file suite_cases.c
 * @brief Pulls the praet cases into this project without copying them.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note PlatformIO compiles what is under src and refuses a build_src_filter that climbs out of it
 *       with ../, which it reports as "Source directory cannot be under variant directory". A file
 *       that includes the real one is how the sources stay where they live.
 * @note The part runs the same file the host does. A copy would pass here and drift there.
 */
#include "../../../integration/test_praet_correctness/test_praet_correctness.c"
