/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file clz_fuzz.c
 * @brief A libFuzzer harness over the zero counts: each word's leading and trailing counts against a bit walk,
 *        with 0 at 63 as the header states.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "clz/clz.h"

#include "fuzz_input.h"

static int walked_lead(uint64_t val)
{
    int count = 0;
    for (int bit = 63; (bit > 0) && (((val >> bit) & 1u) == 0u); bit -= 1)
    {
        count += 1;
    }
    return count;
}

static int walked_trail(uint64_t val)
{
    int count = 0;
    for (int bit = 0; (bit < 63) && (((val >> bit) & 1u) == 0u); bit += 1)
    {
        count += 1;
    }
    return count;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        const uint64_t val = fuzz_edge_word(&input);
        fuzz_check(EMBED_CALL(clz.lead, ClzCfg, .val = val) == walked_lead(val), "clz.lead disagrees with the walk");
        fuzz_check(EMBED_CALL(clz.trail, ClzCfg, .val = val) == walked_trail(val), "clz.trail disagrees with the walk");
    }
    return 0;
}
