/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file mmgr_settings.h
 * @brief The settings header of a build that gives no command line flag, for the settings check.
 *
 * @note Only the settings check in test/CMakeLists.txt has this directory on its include path.
 * @note Both values differ from the defaults. The check thus sees that MMgr read them from here.
 */
#ifndef MMGR_TEST_SETTINGS_H
#define MMGR_TEST_SETTINGS_H

#define MMGR_RING_LOCULI 16u
#define MMGR_RING_WORDS 72u

#endif
