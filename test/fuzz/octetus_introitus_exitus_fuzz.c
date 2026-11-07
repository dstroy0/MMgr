/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file octetus_introitus_exitus_fuzz.c
 * @brief A libFuzzer harness over the octet stream: appends of a byte, a big endian value and a run on a span of
 *        any capacity against a model that stores nothing past the cap and latches the overflow, then the bytes
 *        written read back through take_be and rd_str against the same model, and mpint_fixed against a field
 *        filled by hand.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "octetus_introitus_exitus/octetus_introitus_exitus.h"

#include "fuzz_input.h"

#include <string.h>

#define FUZZ_CAP_MOST 96u

/** @brief The model of a fill span: the bytes it holds, its cap, its position and its overflow. */
typedef struct
{
    uint8_t buf[FUZZ_CAP_MOST];
    size_t cap;
    size_t pos;
    int overflow;
} SpanModel;

static uint8_t *model_claim(SpanModel *model, size_t bytes)
{
    const size_t at = model->pos;
    model->pos += bytes;
    if (model->overflow || (at > model->cap) || (bytes > model->cap - at))
    {
        model->overflow = 1;
        return NULL;
    }
    return model->buf + at;
}

static void appends(FuzzInput *input, mmgr_span *span, SpanModel *model)
{
    const unsigned int count = fuzz_byte(input) % 24u;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        const uint8_t op = fuzz_byte(input);
        switch (op % 3u)
        {
        case 0:
        {
            const uint8_t byte = fuzz_byte(input);
            EMBED_CALL(byteio.put, OctetusCfg, .write_span = span, .byte = byte);
            uint8_t *const at = model_claim(model, 1u);
            if (at != NULL)
            {
                *at = byte;
            }
            break;
        }
        case 1:
        {
            const size_t bytes = 1u + (size_t)(fuzz_byte(input) % 8u);
            const uint64_t value = fuzz_edge_word(input);
            EMBED_CALL(byteio.put_be, OctetusCfg, .write_span = span, .value = value, .bytes = bytes);
            uint8_t *const at = model_claim(model, bytes);
            if (at != NULL)
            {
                for (size_t step = 0u; step < bytes; step += 1u)
                {
                    at[step] = (uint8_t)(value >> (8u * (bytes - 1u - step)));
                }
            }
            break;
        }
        default:
        {
            uint8_t run[32];
            const size_t bytes = fuzz_below(input, sizeof run + 1u);
            for (size_t step = 0u; step < bytes; step += 1u)
            {
                run[step] = fuzz_byte(input);
            }
            EMBED_CALL(byteio.raw, OctetusCfg, .write_span = span, .src = run, .bytes = bytes);
            uint8_t *const at = model_claim(model, bytes);
            if (at != NULL)
            {
                memcpy(at, run, bytes);
            }
            break;
        }
        }
        fuzz_check(span->pos == model->pos, "an append moves the position as the model does");
        fuzz_check((span->overflow != 0) == (model->overflow != 0), "an append overflows where the model does");
        fuzz_check(memcmp(span->buf, model->buf, model->cap) == 0, "the span holds the model's bytes");
    }
}

static void reads(FuzzInput *input, const uint8_t *bytes, size_t len)
{
    mmgr_cspan span = {bytes, len, 0u, EMBED_FALSE};
    size_t pos = 0u;
    int err = 0;
    const unsigned int count = fuzz_byte(input) % 16u;

    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if ((fuzz_byte(input) & 1u) == 0u)
        {
            const size_t width = 1u + (size_t)(fuzz_byte(input) % 8u);
            uint64_t out = 0x5A5A5A5A5A5A5A5Au;
            const embed_bool taken = EMBED_CALL(byteio.take_be, OctetusCfg, .read_span = &span, .out = &out, .bytes = width);
            const int fits = !err && (pos <= len) && (width <= len - pos);
            fuzz_check((taken != 0) == fits, "take_be reads where the bytes are there");
            if (fits)
            {
                uint64_t expected = 0u;
                for (size_t step = 0u; step < width; step += 1u)
                {
                    expected = (expected << 8) | bytes[pos + step];
                }
                fuzz_check(out == expected, "take_be reads the value most significant byte first");
                pos += width;
            }
            else
            {
                fuzz_check(out == 0x5A5A5A5A5A5A5A5Au, "a short take_be leaves its out untouched");
                err = 1;
            }
        }
        else
        {
            const uint8_t *blob = NULL;
            size_t blob_bytes = 0u;
            const embed_bool found = EMBED_CALL(byteio.rd_str, OctetusCfg, .read_span = &span, .blob = &blob,
                                                .blob_bytes = &blob_bytes);
            int fits = !err && (pos <= len) && (4u <= len - pos);
            size_t run = 0u;
            if (fits)
            {
                run = ((size_t)bytes[pos] << 24) | ((size_t)bytes[pos + 1u] << 16) | ((size_t)bytes[pos + 2u] << 8) |
                      (size_t)bytes[pos + 3u];
                fits = run <= len - pos - 4u;
            }
            fuzz_check((found != 0) == fits, "rd_str finds a run that lies within the span");
            if (fits)
            {
                fuzz_check((blob == bytes + pos + 4u) && (blob_bytes == run), "rd_str points at the run behind its length");
                pos += 4u + run;
            }
            else
            {
                fuzz_check((blob == NULL) && (blob_bytes == 0u), "a short rd_str writes nothing out");
                err = 1;
            }
        }
        fuzz_check(span.pos == pos, "a read moves the cursor as the model does");
        fuzz_check((span.err != 0) == (err != 0), "a read latches its error where the model does");
    }
}

static void fixed(FuzzInput *input)
{
    uint8_t integer[24];
    uint8_t field[16];
    uint8_t expected[16];
    const size_t bytes = fuzz_below(input, sizeof integer + 1u);
    const size_t cap = 1u + fuzz_below(input, sizeof field);

    for (size_t step = 0u; step < bytes; step += 1u)
    {
        integer[step] = ((fuzz_byte(input) & 3u) == 0u) ? 0u : fuzz_byte(input);
    }
    size_t lead = 0u;
    while ((lead < bytes) && (integer[lead] == 0u))
    {
        lead += 1u;
    }
    const size_t value = bytes - lead;
    memset(field, 0xA5, sizeof field);
    mmgr_span span = {field, cap, 0u, EMBED_FALSE};
    const embed_bool fits = EMBED_CALL(byteio.mpint_fixed, OctetusCfg, .write_span = &span, .src = integer, .bytes = bytes);

    fuzz_check((fits != 0) == (value <= cap), "mpint_fixed fits an integer no wider than its field");
    if (value <= cap)
    {
        memset(expected, 0, cap - value);
        memcpy(expected + (cap - value), integer + lead, value);
        fuzz_check(memcmp(field, expected, cap) == 0, "mpint_fixed right-aligns the integer behind zeros");
        fuzz_check(span.pos == cap, "mpint_fixed leaves the cursor at the cap");
    }
    else
    {
        fuzz_check(span.overflow != 0, "an integer too wide latches the overflow");
        for (size_t step = 0u; step < cap; step += 1u)
        {
            fuzz_check(field[step] == 0xA5u, "an integer too wide writes nothing");
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        uint8_t buf[FUZZ_CAP_MOST];
        SpanModel model;
        memset(&model, 0, sizeof model);
        memset(buf, 0, sizeof buf);
        model.cap = 1u + fuzz_below(&input, FUZZ_CAP_MOST);
        mmgr_span span = {buf, model.cap, 0u, EMBED_FALSE};

        appends(&input, &span, &model);
        reads(&input, model.buf, (model.pos < model.cap) ? model.pos : model.cap);
        fixed(&input);
    }
    return 0;
}
