/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file endian_fuzz.c
 * @brief A libFuzzer harness over the byte orders. Each write runs against bytes laid out one shift at a time, and
 *        each read against the same. Every write reads back, and the reversal runs against the low bytes taken in reverse.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "endian/endian.h"

#include "fuzz_input.h"

#include <string.h>

static const mmgr_endian_width s_widths[3] = {MMGR_ENDIAN_16, MMGR_ENDIAN_32, MMGR_ENDIAN_64};

static uint64_t low_bytes(uint64_t val, unsigned int width)
{
    return (width == 8u) ? val : (val & (((uint64_t)1 << (8u * width)) - 1u));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        const mmgr_endian_width width = s_widths[fuzz_byte(&input) % 3u];
        const unsigned int bytes = (unsigned int)width;
        const uint64_t val = fuzz_edge_word(&input);
        const uint64_t low = low_bytes(val, bytes);
        uint8_t little[8];
        uint8_t big[8];
        uint8_t written[9];

        for (unsigned int at = 0u; at < bytes; at += 1u)
        {
            little[at] = (uint8_t)(val >> (8u * at));
            big[at] = (uint8_t)(val >> (8u * (bytes - 1u - at)));
        }

        memset(written, 0xA5, sizeof written);
        fuzz_check(EMBED_CALL(parva_extremitas.wr, EndianCfg, .dst = written, .val = val, .width = width) == bytes,
                   "the little-endian write counts its bytes");
        fuzz_check(memcmp(written, little, bytes) == 0, "the little-endian write lays out the low byte first");
        fuzz_check(written[bytes] == 0xA5u, "the little-endian write stays within its width");
        fuzz_check(EMBED_CALL(parva_extremitas.rd, EndianCfg, .src = written, .width = width) == low,
                   "the little-endian read gives back what was written");

        memset(written, 0xA5, sizeof written);
        fuzz_check(EMBED_CALL(magna_extremitas.wr, EndianCfg, .dst = written, .val = val, .width = width) == bytes,
                   "the big-endian write counts its bytes");
        fuzz_check(memcmp(written, big, bytes) == 0, "the big-endian write lays out the high byte first");
        fuzz_check(written[bytes] == 0xA5u, "the big-endian write stays within its width");
        fuzz_check(EMBED_CALL(magna_extremitas.rd, EndianCfg, .src = written, .width = width) == low,
                   "the big-endian read gives back what was written");

        fuzz_check(EMBED_CALL(magna_extremitas.rd, EndianCfg, .src = little, .width = width) ==
                       EMBED_CALL(parva_extremitas.rev, EndianCfg, .val = val, .width = width),
                   "the reversal is the little-endian bytes read big-endian");
        fuzz_check(EMBED_CALL(parva_extremitas.rev, EndianCfg,
                              .val = EMBED_CALL(parva_extremitas.rev, EndianCfg, .val = val, .width = width),
                              .width = width) == low,
                   "the reversal reversed is the low bytes");
    }
    return 0;
}
