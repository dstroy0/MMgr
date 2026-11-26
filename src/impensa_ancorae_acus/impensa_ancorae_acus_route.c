/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file impensa_ancorae_acus_route.c
 * @brief Byte cost table that scores characters, digits and path punctuation.
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
 * @note 255 marks the NUL and the slash, and so neither is ever chosen as a sieve offset.
 * @note The braces at 123 and 125 carry a cost, where max punctuation sits at 1.
 * @note Each cost is 1 through 255.
 */
EMBED_PASS_OBJECT(const uint8_t, mmgr_ancorae_impensa_table, [256],
                  {255, 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                   118, 118, 146, 1,   1,   204, 176, 255, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 181, 1,
                   128, 1,   128, 136, 1,   136, 94,  111, 116, 145, 102, 100, 122, 132, 43,  76,  118, 105, 132, 133,
                   100, 23,  129, 130, 137, 107, 83,  97,  40,  96,  26,  1,   1,   1,   1,   194, 1,   218, 175, 193,
                   198, 227, 183, 182, 204, 214, 125, 158, 200, 187, 214, 215, 182, 105, 210, 212, 219, 189, 165, 179,
                   121, 177, 107, 187, 1,   187, 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
                   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1});
