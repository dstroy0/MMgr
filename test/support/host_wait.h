/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file host_wait.h
 * @brief The host's address wait and wake behind embed_word_wait and embed_word_wake, and a millisecond
 *        clock, for a suite that waits on another thread; the platform's headers live in host_wait.c alone.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-05
 */
#ifndef HOST_WAIT_H
#define HOST_WAIT_H

/**
 * @brief The host's clock in milliseconds, since a start of its choosing.
 *
 * @return The milliseconds.
 */
unsigned long long host_clock_ms(void);

#endif
