/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file fuzz_replay.c
 * @brief The main a fuzz harness links with where there is no libFuzzer: on gcc, on MSVC, on any toolchain.
 *        It hands the harness every file named after the count and the seed. Then it hands the count of inputs
 *        drawn from the seed, the same inputs on every toolchain. A harness's oracle judges each toolchain's
 *        build of the module on the same bytes. A disagreement stops in the harness, which names it.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#define REPLAY_BYTES_MOST 4096u

static uint8_t s_input[REPLAY_BYTES_MOST];

/** @brief The next word of a xorshift64 stream, the same on every toolchain. */
static uint64_t replay_next(uint64_t *state)
{
    uint64_t word = *state;
    word ^= word << 13;
    word ^= word >> 7;
    word ^= word << 17;
    *state = word;
    return word;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <runs> <seed> [input files...]\n", argv[0]);
        return 2;
    }
    const unsigned long runs = strtoul(argv[1], NULL, 10);
    uint64_t state = (uint64_t)strtoull(argv[2], NULL, 10) | 1u;
    unsigned long files = 0u;

    for (int at = 3; at < argc; at += 1)
    {
        FILE *const file = fopen(argv[at], "rb");
        if (file == NULL)
        {
            fprintf(stderr, "replay: %s cannot be read\n", argv[at]);
            return 2;
        }
        const size_t size = fread(s_input, 1u, sizeof s_input, file);
        fclose(file);
        LLVMFuzzerTestOneInput(s_input, size);
        files += 1u;
    }
    for (unsigned long run = 0u; run < runs; run += 1u)
    {
        const size_t size = (size_t)(replay_next(&state) % 1025u);
        for (size_t at = 0u; at < size; at += 8u)
        {
            const uint64_t word = replay_next(&state);
            for (size_t byte = 0u; (byte < 8u) && (at + byte < size); byte += 1u)
            {
                s_input[at + byte] = (uint8_t)(word >> (8u * byte));
            }
        }
        LLVMFuzzerTestOneInput(s_input, size);
    }
    printf("replay: %lu files and %lu drawn inputs passed\n", files, runs);
    return 0;
}
