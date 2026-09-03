/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file unity_config.h
 * @brief Tells Unity where its output goes on a part that has no stdout.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note PlatformIO's Unity package is built with UNITY_INCLUDE_CONFIG_H set, so unity_internals.h
 *       includes this file and the build fails without it. Measured, on the first build here.
 * @note This is where the output hook belongs. Passing it as a define on the command line reaches
 *       the suite but not Unity's own translation unit, and Unity is what calls it.
 */
#ifndef MMGR_TEST_UNITY_CONFIG_H
#define MMGR_TEST_UNITY_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Sends one character to the part's serial port.
 *
 * @param[in] letter Character to send, as Unity hands it over.
 * @note Defined in device_main.cpp, which is the only file here that knows what a serial port is.
 */
void device_putchar(int letter);

#ifdef __cplusplus
}
#endif

/**
 * @brief Where Unity writes every character it emits.
 *
 * @param letter_ Character Unity is emitting.
 */
#define UNITY_OUTPUT_CHAR(letter_) device_putchar(letter_)

#endif
