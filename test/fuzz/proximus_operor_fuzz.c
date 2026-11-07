/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file proximus_operor_fuzz.c
 * @brief A libFuzzer harness over the near loads and stores: every width at every offset, aligned where the entry
 *        asks it, each against memcpy in the target's own order, the bytes around each store checked untouched,
 *        and the aligning read against memcpy at every offset and length.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "proximus_operor/proximus_operor.h"

#include "fuzz_input.h"

#include <string.h>

#define FUZZ_BYTES 128u

static union {
    uint64_t aligned;
    uint8_t bytes[FUZZ_BYTES];
} s_area, s_source, s_expected;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        const uint8_t op = fuzz_byte(&input);
        const uint64_t val = fuzz_edge_word(&input);
        size_t at = fuzz_below(&input, FUZZ_BYTES - 8u);

        for (size_t step = 0u; step < FUZZ_BYTES; step += 1u)
        {
            s_area.bytes[step] = (uint8_t)(0x3Bu * step + 7u);
        }
        memcpy(s_expected.bytes, s_area.bytes, FUZZ_BYTES);

        switch (op % 13u)
        {
        case 0: {
            uint16_t expected;
            memcpy(&expected, s_area.bytes + at, sizeof expected);
            fuzz_check(EMBED_CALL(proxim.load16, ProximusCfg, .at = s_area.bytes + at) == expected, "load16 is memcpy");
            break;
        }
        case 1: {
            uint32_t expected;
            memcpy(&expected, s_area.bytes + at, sizeof expected);
            fuzz_check(EMBED_CALL(proxim.load32, ProximusCfg, .at = s_area.bytes + at) == expected, "load32 is memcpy");
            break;
        }
        case 2: {
            uint64_t expected;
            memcpy(&expected, s_area.bytes + at, sizeof expected);
            fuzz_check(EMBED_CALL(proxim.load64, ProximusCfg, .at = s_area.bytes + at) == expected, "load64 is memcpy");
            break;
        }
        case 3: {
            const uint16_t low = (uint16_t)val;
            EMBED_CALL(proxim.put16, ProximusCfg, .dst = s_area.bytes + at, .val = val);
            memcpy(s_expected.bytes + at, &low, sizeof low);
            break;
        }
        case 4: {
            const uint32_t low = (uint32_t)val;
            EMBED_CALL(proxim.put32, ProximusCfg, .dst = s_area.bytes + at, .val = val);
            memcpy(s_expected.bytes + at, &low, sizeof low);
            break;
        }
        case 5:
            EMBED_CALL(proxim.put64, ProximusCfg, .dst = s_area.bytes + at, .val = val);
            memcpy(s_expected.bytes + at, &val, sizeof val);
            break;
        case 6: {
            embed_word expected;
            memcpy(&expected, s_area.bytes + at, sizeof expected);
            fuzz_check(EMBED_CALL(proxim.load, ProximusCfg, .at = s_area.bytes + at) == expected, "load is memcpy");
            break;
        }
        case 7: {
            const embed_word low = (embed_word)val;
            EMBED_CALL(proxim.put, ProximusCfg, .dst = s_area.bytes + at, .val = val);
            memcpy(s_expected.bytes + at, &low, sizeof low);
            break;
        }
        case 8: {
            at -= at % sizeof(embed_word);
            embed_word expected;
            memcpy(&expected, s_area.bytes + at, sizeof expected);
            fuzz_check(EMBED_CALL(proxim.al_load, ProximusCfg, .at = s_area.bytes + at) == expected,
                       "al_load is memcpy at an aligned address");
            break;
        }
        case 9: {
            at -= at % sizeof(embed_word);
            const embed_word low = (embed_word)val;
            EMBED_CALL(proxim.al_put, ProximusCfg, .dst = s_area.bytes + at, .val = val);
            memcpy(s_expected.bytes + at, &low, sizeof low);
            break;
        }
        case 10: {
            at -= at % 8u;
            uint64_t expected;
            memcpy(&expected, s_area.bytes + at, sizeof expected);
            fuzz_check(EMBED_CALL(proxim.al_load64, ProximusCfg, .at = s_area.bytes + at) == expected,
                       "al_load64 is memcpy at an aligned address");
            break;
        }
        case 11:
            at -= at % 8u;
            EMBED_CALL(proxim.al_put64, ProximusCfg, .dst = s_area.bytes + at, .val = val);
            memcpy(s_expected.bytes + at, &val, sizeof val);
            break;
        default: {
            const size_t from = fuzz_below(&input, FUZZ_BYTES);
            const size_t bytes = fuzz_below(&input, FUZZ_BYTES - ((from > at) ? from : at) + 1u);
            for (size_t step = 0u; step < FUZZ_BYTES; step += 1u)
            {
                s_source.bytes[step] = (uint8_t)(val >> (8u * (step % 8u))) ^ (uint8_t)step;
            }
            EMBED_CALL(proxim.read, ProximusCfg, .dst = s_area.bytes + at, .at = s_source.bytes + from, .size = bytes);
            memcpy(s_expected.bytes + at, s_source.bytes + from, bytes);
            break;
        }
        }
        fuzz_check(memcmp(s_area.bytes, s_expected.bytes, FUZZ_BYTES) == 0, "a store writes its bytes and only those");
    }
    return 0;
}
