/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file impensa_ancorae_acus_fuzz.c
 * @brief A libFuzzer harness over the needle cost tables, built once for each of the five with that table's source.
 *        Every byte's cost is from 1 through 255, the same on every lookup. The NUL and the second byte the table
 *        names, FUZZ_ANCORAE_BARRED, cost 255, and neither is ever the sieve offset.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "impensa_ancorae_acus/impensa_ancorae_acus.h"

#include "fuzz_input.h"

#ifndef FUZZ_ANCORAE_BARRED
#error "FUZZ_ANCORAE_BARRED names the second byte the linked table bars"
#endif

static uint8_t s_first[256];
static int s_seen[256];

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        const uint8_t byte = fuzz_byte(&input);
        const uint8_t cost = EMBED_CALL(ancorae.impensa, AncoraeCfg, .byte = byte);
        fuzz_check(cost >= 1u, "a cost is at least 1");
        if ((byte == 0u) || (byte == (uint8_t)FUZZ_ANCORAE_BARRED))
        {
            fuzz_check(cost == 255u, "the NUL and the barred byte cost 255");
        }
        if (s_seen[byte])
        {
            fuzz_check(s_first[byte] == cost, "a byte costs the same on every lookup");
        }
        s_first[byte] = cost;
        s_seen[byte] = 1;
    }
    return 0;
}
