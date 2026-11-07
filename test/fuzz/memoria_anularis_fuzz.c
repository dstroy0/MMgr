/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file memoria_anularis_fuzz.c
 * @brief A libFuzzer harness over the ring: init with any sizes against the refusals it states, then any sequence
 *        of puts, reads, single byte reads, peeks and consumes against a byte queue that keeps one byte reserved,
 *        the segment view against a claim and a release counter, and the loculi against a free and a pinned mask.
 *        A ring laid with a segment size carries no byte stream, and the model checks that too.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "memoria_anularis/memoria_anularis.h"

#include "fuzz_input.h"

#include <string.h>

#define FUZZ_CAP_MOST 256u

static mmgr_ring s_ring;
static uint8_t s_buf[FUZZ_CAP_MOST];

/** @brief The model: the queued bytes, the segment counters and the loculus masks. */
typedef struct
{
    uint8_t queue[FUZZ_CAP_MOST];
    size_t queued;
    size_t capacity;
    int stream;
    size_t segment_count;
    size_t segment_bytes;
    size_t claimed;
    size_t released;
    int claim_open;
    embed_word pinned;
    const uint8_t *kept[MMGR_RING_LOCULI + 1u];
    size_t kept_bytes[MMGR_RING_LOCULI + 1u];
} RingModel;

static RingModel s_model;

static int power_of_two(size_t count)
{
    return (count != 0u) && ((count & (count - 1u)) == 0u);
}

static int laid(FuzzInput *input)
{
    const uint8_t kind = fuzz_byte(input);
    memset(&s_model, 0, sizeof s_model);
    if ((kind & 1u) == 0u)
    {
        const size_t capacity = fuzz_below(input, FUZZ_CAP_MOST + 1u);
        const size_t segment_count = fuzz_below(input, 70u);
        const embed_bool ready = EMBED_CALL(anularis.init, AnularisCfg, .ring = &s_ring, .buf = s_buf,
                                            .capacity = capacity, .segment_count = segment_count);
        const int expected = power_of_two(capacity) && power_of_two(segment_count) && (segment_count <= capacity);
        fuzz_check((ready != 0) == expected, "init refuses a size that is zero, not a power of two, or too many segments");
        s_model.capacity = capacity;
        s_model.stream = 1;
        s_model.segment_count = segment_count;
        s_model.segment_bytes = expected ? (capacity / segment_count) : 0u;
        return expected;
    }
    const size_t segment_count = (size_t)1u << (fuzz_byte(input) % 5u);
    const size_t segment_bytes = 1u + fuzz_below(input, FUZZ_CAP_MOST / segment_count);
    const size_t capacity = segment_count * segment_bytes + fuzz_below(input, FUZZ_CAP_MOST - segment_count * segment_bytes + 1u);
    const embed_bool ready = EMBED_CALL(anularis.init, AnularisCfg, .ring = &s_ring, .buf = s_buf, .capacity = capacity,
                                        .segment_count = segment_count, .segment_bytes = segment_bytes);
    fuzz_check(ready != 0, "init with a segment size takes any capacity that holds the segments");
    s_model.capacity = capacity;
    s_model.stream = 0;
    s_model.segment_count = segment_count;
    s_model.segment_bytes = segment_bytes;
    return 1;
}

static size_t model_vacant(void)
{
    return s_model.stream ? (s_model.capacity - 1u - s_model.queued) : 0u;
}

static void bytes_step(FuzzInput *input, uint8_t op)
{
    uint8_t moved[FUZZ_CAP_MOST + 8u];
    switch (op % 5u)
    {
    case 0:
    {
        const size_t bytes = fuzz_below(input, FUZZ_CAP_MOST);
        for (size_t at = 0u; at < bytes; at += 1u)
        {
            moved[at] = fuzz_byte(input);
        }
        const embed_bool written = EMBED_CALL(anularis.put, AnularisCfg, .ring = &s_ring, .src = moved, .bytes = bytes);
        const int fits = bytes <= model_vacant();
        fuzz_check((written != 0) == fits, "put writes the whole span or refuses it");
        if (fits)
        {
            memcpy(s_model.queue + s_model.queued, moved, bytes);
            s_model.queued += bytes;
        }
        break;
    }
    case 1:
    {
        const size_t bytes = fuzz_below(input, FUZZ_CAP_MOST);
        memset(moved, 0xA5, sizeof moved);
        const size_t taken = EMBED_CALL(anularis.read, AnularisCfg, .ring = &s_ring, .dst = moved, .bytes = bytes);
        const size_t expected = (bytes < s_model.queued) ? bytes : s_model.queued;
        fuzz_check(taken == expected, "read takes what is asked or what is there");
        fuzz_check(memcmp(moved, s_model.queue, expected) == 0, "read takes the bytes in the order they were put");
        fuzz_check(moved[expected] == 0xA5u, "read writes no more than it takes");
        memmove(s_model.queue, s_model.queue + expected, s_model.queued - expected);
        s_model.queued -= expected;
        break;
    }
    case 2:
    {
        uint8_t byte = 0x5Au;
        const embed_bool taken = EMBED_CALL(anularis.read_byte, AnularisCfg, .ring = &s_ring, .dst = &byte);
        fuzz_check((taken != 0) == (s_model.queued != 0u), "read_byte takes a byte when one is there");
        if (s_model.queued != 0u)
        {
            fuzz_check(byte == s_model.queue[0], "read_byte takes the oldest byte");
            memmove(s_model.queue, s_model.queue + 1u, s_model.queued - 1u);
            s_model.queued -= 1u;
        }
        else
        {
            fuzz_check(byte == 0x5Au, "an empty read_byte writes nothing");
        }
        break;
    }
    case 3:
    {
        const size_t offset = fuzz_below(input, s_model.queued + 1u);
        const size_t bytes = fuzz_below(input, s_model.queued - offset + 1u);
        memset(moved, 0xA5, sizeof moved);
        EMBED_CALL(anularis.peek, AnularisCfg, .ring = &s_ring, .dst = moved, .bytes = bytes, .offset = offset);
        fuzz_check(memcmp(moved, s_model.queue + offset, bytes) == 0, "peek copies the bytes ahead of the tail");
        break;
    }
    default:
    {
        const size_t bytes = fuzz_below(input, s_model.queued + 1u);
        EMBED_CALL(anularis.consume, AnularisCfg, .ring = &s_ring, .bytes = bytes);
        memmove(s_model.queue, s_model.queue + bytes, s_model.queued - bytes);
        s_model.queued -= bytes;
        break;
    }
    }
    fuzz_check(EMBED_CALL(anularis.available, AnularisCfg, .ring = &s_ring) == (s_model.stream ? s_model.queued : 0u),
               "available is the bytes queued");
    fuzz_check(EMBED_CALL(anularis.vacant, AnularisCfg, .ring = &s_ring) == model_vacant(),
               "vacant is the capacity less one less the bytes queued");
}

static void segments_step(FuzzInput *input, uint8_t op)
{
    size_t index = 9999u;
    const size_t inflight = s_model.claimed - s_model.released;
    switch (op % 4u)
    {
    case 0:
    {
        const embed_bool open = EMBED_CALL(anularis.seg_next, AnularisCfg, .ring = &s_ring, .out_index = &index);
        fuzz_check((open != 0) == (inflight < s_model.segment_count), "seg_next opens a segment while one is free");
        if (open)
        {
            fuzz_check(index == s_model.claimed % s_model.segment_count, "seg_next names the segment after the claimed");
            s_model.claim_open = 1;
        }
        else
        {
            fuzz_check(index == 9999u, "a refused seg_next writes no index");
        }
        break;
    }
    case 1:
        if (s_model.claim_open)
        {
            EMBED_CALL(anularis.seg_publish, AnularisCfg, .ring = &s_ring);
            s_model.claimed += 1u;
            s_model.claim_open = 0;
        }
        break;
    case 2:
    {
        const embed_bool front = EMBED_CALL(anularis.seg_front, AnularisCfg, .ring = &s_ring, .out_index = &index);
        fuzz_check((front != 0) == (inflight != 0u), "seg_front names a segment while one is in flight");
        if (front)
        {
            fuzz_check(index == s_model.released % s_model.segment_count, "seg_front names the oldest segment");
        }
        break;
    }
    default:
        if (inflight != 0u)
        {
            EMBED_CALL(anularis.seg_release, AnularisCfg, .ring = &s_ring);
            s_model.released += 1u;
        }
        break;
    }
    fuzz_check(EMBED_CALL(anularis.seg_inflight, AnularisCfg, .ring = &s_ring) == s_model.claimed - s_model.released,
               "seg_inflight is the claimed less the released");
    const size_t at = fuzz_below(input, s_model.segment_count);
    fuzz_check(EMBED_CALL(anularis.seg_at, AnularisCfg, .ring = &s_ring, .index = at) == s_buf + at * s_model.segment_bytes,
               "a segment sits at its index times its size");
}

static void loculi_step(FuzzInput *input, uint8_t op)
{
    const size_t index = fuzz_below(input, MMGR_RING_LOCULI + 2u);
    const embed_word all = (MMGR_RING_LOCULI == EMBED_WORD_BITS) ? ~(embed_word)0
                                                                  : (((embed_word)1 << MMGR_RING_LOCULI) - 1u);
    switch (op % 4u)
    {
    case 0:
    {
        const uint8_t *const region = s_buf + fuzz_below(input, FUZZ_CAP_MOST);
        const size_t bytes = fuzz_below(input, 64u);
        const embed_bool taken =
            EMBED_CALL(anularis.loculus_hold, AnularisCfg, .ring = &s_ring, .index = index, .src = region, .bytes = bytes);
        const int expected = (index < MMGR_RING_LOCULI) && (((s_model.pinned >> index) & 1u) == 0u);
        fuzz_check((taken != 0) == expected, "a hold takes a loculus in range that is not pinned");
        if (expected)
        {
            s_model.pinned |= (embed_word)1 << index;
            s_model.kept[index] = region;
            s_model.kept_bytes[index] = bytes;
        }
        break;
    }
    case 1:
        EMBED_CALL(anularis.loculus_drop, AnularisCfg, .ring = &s_ring, .index = index);
        if (index < MMGR_RING_LOCULI)
        {
            s_model.pinned &= ~((embed_word)1 << index);
        }
        break;
    case 2:
        EMBED_CALL(anularis.loculus_mark, AnularisCfg, .ring = &s_ring, .index = index);
        break;
    default:
    {
        const embed_word mask = (embed_word)fuzz_edge_word(input);
        embed_iword lowest = -1;
        for (unsigned int bit = 0u; bit < EMBED_WORD_BITS; bit += 1u)
        {
            if (((mask >> bit) & 1u) != 0u)
            {
                lowest = (embed_iword)bit;
                break;
            }
        }
        fuzz_check(EMBED_CALL(anularis.loculus_next, AnularisCfg, .mask = mask) == lowest,
                   "loculus_next is the lowest set bit");
        break;
    }
    }
    fuzz_check(EMBED_CALL(anularis.loculus_ready, AnularisCfg, .ring = &s_ring) == (all & ~s_model.pinned),
               "the ready loculi are the free ones not pinned");
    const mmgr_ring_span *const kept = EMBED_CALL(anularis.loculus_keepout, AnularisCfg, .ring = &s_ring, .index = index);
    if (index >= MMGR_RING_LOCULI)
    {
        fuzz_check(kept == NULL, "a keepout out of range is NULL");
    }
    else if (s_model.kept[index] != NULL)
    {
        fuzz_check((kept != NULL) && (kept->buf == s_model.kept[index]) && (kept->bytes == s_model.kept_bytes[index]),
                   "a keepout names the last region its loculus recorded");
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    if (!laid(&input))
    {
        return 0;
    }
    while (fuzz_left(&input))
    {
        const uint8_t op = fuzz_byte(&input);
        switch (op % 3u)
        {
        case 0:
            bytes_step(&input, (uint8_t)(op / 3u));
            break;
        case 1:
            segments_step(&input, (uint8_t)(op / 3u));
            break;
        default:
            loculi_step(&input, (uint8_t)(op / 3u));
            break;
        }
    }
    return 0;
}
