/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file kat_clean.c
 * @brief The known answer of harness.py nolibc that holds no symbol of a C library.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-23
 *
 * @note Every stage of the check must pass this file, and its image must link with no default library. A toolchain
 *       that fails it gives no verdict that counts.
 * @note The code is arithmetic on its arguments alone. It has no aggregate, no copy and no loop that a compiler can
 *       lower to a call.
 */
#include <stdint.h>

uint32_t kat_clean_mix(uint32_t left, uint32_t right);

uint32_t kat_clean_mix(uint32_t left, uint32_t right)
{
    const uint32_t turned = (left << 7u) | (left >> 25u);
    return (turned ^ right) + 0x9E3779B9u;
}
