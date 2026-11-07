/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file memoria_externa_fuzz.c
 * @brief A libFuzzer harness over external memory placement: every request size, DMA requirement, pair of free
 *        figures, threshold and reserve against the placement the header states, and the ping-pong pair through
 *        any sequence of swaps against one bit. Built with external memory on and the module compiled in.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "memoria_externa/memoria_externa.h"

#include "fuzz_input.h"

static size_t drawn_size(FuzzInput *input)
{
    const uint8_t kind = fuzz_byte(input);
    switch (kind & 3u)
    {
    case 0:
        return (size_t)(kind >> 2);
    case 1:
        return SIZE_MAX - (size_t)(kind >> 2);
    default:
        return (size_t)fuzz_word(input);
    }
}

static mmgr_place stated(size_t size, int dma, size_t free_dram, size_t free_psram, size_t threshold, size_t reserve)
{
    const int dram_fits = (size <= free_dram) && ((free_dram - size) >= reserve);
    const int psram_fits = size <= free_psram;
    if (size == 0u)
    {
        return PLACE_FAIL;
    }
    if (dma)
    {
        return dram_fits ? PLACE_DRAM : PLACE_FAIL;
    }
    if (size >= threshold)
    {
        return psram_fits ? PLACE_PSRAM : (dram_fits ? PLACE_DRAM : PLACE_FAIL);
    }
    return dram_fits ? PLACE_DRAM : (psram_fits ? PLACE_PSRAM : PLACE_FAIL);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    PingPong pair;
    uint8_t fill = 0u;

    EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);
    while (fuzz_left(&input))
    {
        const uint8_t op = fuzz_byte(&input);
        if ((op & 1u) == 0u)
        {
            const size_t request = drawn_size(&input);
            const int dma = (op & 2u) != 0u;
            const size_t free_dram = drawn_size(&input);
            const size_t free_psram = drawn_size(&input);
            const size_t threshold = drawn_size(&input);
            const size_t reserve = drawn_size(&input);
            const mmgr_place placed = EMBED_CALL(
                exter.place, ExternaCfg, .size = request, .dma_required = (embed_bool)dma, .free_dram = free_dram,
                .free_psram = free_psram, .psram_threshold = threshold, .dram_reserve = reserve);
            fuzz_check(placed == stated(request, dma, free_dram, free_psram, threshold, reserve),
                       "a request is placed where the header states");
        }
        else if ((op & 2u) == 0u)
        {
            fill = (uint8_t)(fill ^ 1u);
            fuzz_check(EMBED_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair) == fill,
                       "a swap answers the index now filled");
        }
        else
        {
            EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);
            fill = 0u;
        }
        fuzz_check(EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair) == fill,
                   "the fill index is the model's");
        fuzz_check(EMBED_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair) == (uint8_t)(fill ^ 1u),
                   "the drain index is the other one");
    }
    return 0;
}
