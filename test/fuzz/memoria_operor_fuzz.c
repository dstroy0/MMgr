/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file memoria_operor_fuzz.c
 * @brief A libFuzzer harness over the memory operations. Copies, both moves over regions that overlap, the compare,
 *        the search and the fill run at every offset and length, each against a byte loop. The bytes around the
 *        region stay untouched.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "memoria_operor/memoria_operor.h"

#include "fuzz_input.h"

#include <string.h>

#define FUZZ_BYTES 192u
#define FUZZ_TAIL 16u

static uint8_t s_area[FUZZ_BYTES + FUZZ_TAIL];
static uint8_t s_other[FUZZ_BYTES + FUZZ_TAIL];
static uint8_t s_expected[FUZZ_BYTES + FUZZ_TAIL];

static void filled(FuzzInput *input, uint8_t *area)
{
    const uint8_t seed = fuzz_byte(input);
    const uint8_t step = (uint8_t)(fuzz_byte(input) | 1u);
    for (size_t at = 0u; at < FUZZ_BYTES + FUZZ_TAIL; at += 1u)
    {
        area[at] = (uint8_t)(seed + step * (uint8_t)at);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        const uint8_t op = fuzz_byte(&input);
        const size_t from = fuzz_below(&input, FUZZ_BYTES);
        const size_t to = fuzz_below(&input, FUZZ_BYTES);
        const size_t most = FUZZ_BYTES - ((from > to) ? from : to);
        const size_t bytes = fuzz_below(&input, most + 1u);
        const uint8_t val = fuzz_byte(&input);

        filled(&input, s_area);
        memcpy(s_expected, s_area, sizeof s_area);
        switch (op % 6u)
        {
        case 0:
            filled(&input, s_other);
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_area + to, .src = s_other + from, .bytes = bytes);
            memcpy(s_expected + to, s_other + from, bytes);
            fuzz_check(memcmp(s_area, s_expected, sizeof s_area) == 0, "cpy copies the bytes and only those");
            break;
        case 1: {
            const size_t low = (from < to) ? from : to;
            const size_t high = (from < to) ? to : from;
            EMBED_CALL(memor.move_down, MemoriaCfg, .dst = s_area + low, .src = s_area + high, .bytes = bytes);
            memmove(s_expected + low, s_expected + high, bytes);
            fuzz_check(memcmp(s_area, s_expected, sizeof s_area) == 0, "move_down moves to a lower destination");
            break;
        }
        case 2: {
            const size_t low = (from < to) ? from : to;
            const size_t high = (from < to) ? to : from;
            EMBED_CALL(memor.move_up, MemoriaCfg, .dst = s_area + high, .src = s_area + low, .bytes = bytes);
            memmove(s_expected + high, s_expected + low, bytes);
            fuzz_check(memcmp(s_area, s_expected, sizeof s_area) == 0, "move_up moves to a higher destination");
            break;
        }
        case 3: {
            memcpy(s_other, s_area, sizeof s_area);
            const size_t differ = fuzz_below(&input, FUZZ_BYTES);
            if ((fuzz_byte(&input) & 1u) != 0u)
            {
                s_other[differ] = (uint8_t)(s_other[differ] + 1u + (fuzz_byte(&input) % 255u));
            }
            const embed_iword compared =
                EMBED_CALL(memor.cmp, MemoriaCfg, .src = s_area + from, .other = s_other + from, .bytes = bytes);
            int expected = 0;
            for (size_t at = 0u; at < bytes; at += 1u)
            {
                if (s_area[from + at] != s_other[from + at])
                {
                    expected = (int)s_area[from + at] - (int)s_other[from + at];
                    break;
                }
            }
            fuzz_check((int)compared == expected, "cmp answers the difference of the first unequal pair");
            break;
        }
        case 4: {
            const void *const found =
                EMBED_CALL(memor.chr, MemoriaCfg, .src = s_area + from, .bytes = bytes, .val = val);
            const void *const expected = memchr(s_area + from, val, bytes);
            fuzz_check(found == expected, "chr finds the first byte equal to val");
            break;
        }
        default:
            EMBED_CALL(memor.set, MemoriaCfg, .dst = s_area + to, .bytes = bytes, .val = val);
            memset(s_expected + to, val, bytes);
            fuzz_check(memcmp(s_area, s_expected, sizeof s_area) == 0, "set fills the bytes and only those");
            break;
        }
    }
    return 0;
}
