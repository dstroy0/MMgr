/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file impensa_ancorae_acus_inet.c
 * @brief Byte cost table that scores only the characters an address consists of.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-29
 *
 * @note One of five files that define mmgr_ancorae_impensa_table. A build links exactly one of them, beside
 *       impensa_ancorae_acus.c.
 */
#include "impensa_ancorae_acus/impensa_ancorae_acus.h"

/**
 * @brief Cost of each byte value, indexed by the byte itself.
 *
 * @note Lower means rarer, and cellul_pick_rows retains the lowest cost it finds.
 * @note 255 marks the NUL and the colon, and so neither is ever chosen as a sieve offset.
 * @note Also above 1: the digits, 'a' to 'f', 'A' to 'F', and 37 '%', 46 '.', 47 '/', 91 '[', 93 ']'.
 * @note Each cost is 1 through 255.
 */
const uint8_t mmgr_ancorae_impensa_table[256] = {
    255, 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 1, 1,   1, 1, 1, 1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 1, 137, 1, 1, 1, 1,   1,   1,   1,   1,   242, 178,
    240, 235, 231, 228, 228, 228, 228, 228, 228, 228, 255, 1, 1, 1,   1, 1, 1, 178, 178, 178, 178, 178, 178, 1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 1, 1,   1, 1, 1, 1,   1,   160, 1,   160, 1,   1,
    1,   206, 206, 206, 206, 206, 206, 1,   1,   1,   1,   1, 1, 1,   1, 1, 1, 1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 1, 1,   1, 1, 1, 1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 1, 1,   1, 1, 1, 1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 1, 1,   1, 1, 1, 1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 1, 1,   1, 1, 1, 1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 1, 1,   1, 1, 1, 1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 1, 1,   1, 1};
