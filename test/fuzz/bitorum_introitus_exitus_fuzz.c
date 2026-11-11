/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file bitorum_introitus_exitus_fuzz.c
 * @brief A libFuzzer harness over the bit writer. Puts and aligns of any width run on a buffer of any capacity,
 *        against a model. The model appends one bit at a time low bit first. It refuses a put whose whole bytes
 *        pass the capacity and stops every write after it.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "bitorum_introitus_exitus/bitorum_introitus_exitus.h"

#include "fuzz_input.h"

#include <string.h>

#define FUZZ_CAP_MOST 64u

/** @brief The model of a writer: the bytes it wrote, its residue bits and whether it overflowed. */
typedef struct
{
    uint8_t out[FUZZ_CAP_MOST];
    size_t cap;
    size_t written;
    uint8_t residue;
    unsigned int pending;
    int overflow;
} BitModel;

static void model_put(BitModel *model, uint64_t val, unsigned int bit_count)
{
    if (model->overflow)
    {
        return;
    }
    if ((model->pending + bit_count) / 8u > model->cap - model->written)
    {
        model->overflow = 1;
        model->residue = 0u;
        model->pending = 0u;
        return;
    }
    for (unsigned int bit = 0u; bit < bit_count; bit += 1u)
    {
        model->residue = (uint8_t)(model->residue | (uint8_t)(((val >> bit) & 1u) << model->pending));
        model->pending += 1u;
        if (model->pending == 8u)
        {
            model->out[model->written] = model->residue;
            model->written += 1u;
            model->residue = 0u;
            model->pending = 0u;
        }
    }
}

static void model_align(BitModel *model)
{
    if (model->overflow || (model->pending == 0u))
    {
        return;
    }
    if (model->written >= model->cap)
    {
        model->overflow = 1;
        return;
    }
    model->out[model->written] = model->residue;
    model->written += 1u;
    model->residue = 0u;
    model->pending = 0u;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    BitModel model;
    uint8_t out[FUZZ_CAP_MOST + 1u];

    memset(&model, 0, sizeof model);
    memset(out, 0xA5, sizeof out);
    model.cap = 1u + fuzz_below(&input, FUZZ_CAP_MOST);

    mmgr_bitor writer = EMBED_CALL(bitio.init, BitorumCfg, .out = out, .cap = model.cap);
    while (fuzz_left(&input))
    {
        const uint8_t op = fuzz_byte(&input);
        if ((op & 7u) == 0u)
        {
            EMBED_CALL(bitio.align, BitorumCfg, .writer = &writer);
            model_align(&model);
        }
        else
        {
            const unsigned int bit_count = (unsigned int)(op >> 1) % 65u;
            const uint64_t val = fuzz_edge_word(&input);
            EMBED_CALL(bitio.put, BitorumCfg, .writer = &writer, .val = val, .bit_count = bit_count);
            model_put(&model, val, bit_count);
        }
        fuzz_check(writer.bytes_written == model.written, "the writer counts the bytes the model wrote");
        fuzz_check(writer.bit_count == model.pending, "the writer's pending bits are the model's");
        fuzz_check(writer.residue == model.residue, "the writer's residue is the model's");
        fuzz_check((writer.overflow != 0) == (model.overflow != 0), "the writer overflows where the model does");
        fuzz_check(memcmp(out, model.out, model.written) == 0, "the bytes written are the model's");
        fuzz_check(out[model.cap] == 0xA5u, "nothing is written past the capacity");
    }
    return 0;
}
