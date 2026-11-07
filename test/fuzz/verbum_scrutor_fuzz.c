/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file verbum_scrutor_fuzz.c
 * @brief A libFuzzer harness over the word lanes: every lane, mask and word entry on any word, byte, family, mask
 *        and count, each against the same question asked one lane at a time in memory order, with a lane mask
 *        built as the high bit of each answering lane.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "verbum_scrutor/verbum_scrutor.h"

#include "fuzz_input.h"

#include <string.h>

#define LANES MMGR_SWAR_BYTES

static void lanes_of(embed_word tested, uint8_t *bytes)
{
    memcpy(bytes, &tested, LANES);
}

static embed_word word_of(const uint8_t *bytes)
{
    embed_word tested;
    memcpy(&tested, bytes, LANES);
    return tested;
}

/** @brief The at_lane mask with the high bit set in each at_lane @p marked names. */
static embed_word mask_of(const int *marked)
{
    uint8_t bytes[LANES];
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        bytes[at_lane] = marked[at_lane] ? 0x80u : 0u;
    }
    return word_of(bytes);
}

static int letter(uint8_t byte)
{
    return ((byte >= 'A') && (byte <= 'Z')) || ((byte >= 'a') && (byte <= 'z'));
}

static embed_word drawn_mask(FuzzInput *input)
{
    return (embed_word)fuzz_edge_word(input) & MMGR_VERBUM_SCRUTOR_HIGH;
}

static void lane_entries(FuzzInput *input)
{
    const embed_word tested = (embed_word)fuzz_edge_word(input);
    const uint8_t byte = fuzz_byte(input);
    const uint8_t fam = fuzz_byte(input);
    const int ci = (fuzz_byte(input) & 1u) != 0u;
    const embed_word val = (embed_word)fuzz_edge_word(input);
    uint8_t lanes[LANES];
    uint8_t others[LANES];
    uint8_t made[LANES];
    int marked[LANES];

    lanes_of(tested, lanes);
    lanes_of(val, others);

    // ge and le take a byte under 0x80, as their contracts state
    const uint8_t bounded = (uint8_t)(byte & 0x7Fu);
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = lanes[at_lane] >= bounded;
    }
    const embed_word ge = EMBED_CALL(lane.ge, ScrutLaneCfg, .word = tested, .byte = bounded);
    if (ge != mask_of(marked))
    {
        fprintf(stderr, "ge of 0x%016llx against 0x%02x gave 0x%016llx, the lanes give 0x%016llx\n",
                (unsigned long long)tested, bounded, (unsigned long long)ge, (unsigned long long)mask_of(marked));
    }
    fuzz_check(ge == mask_of(marked), "ge is lane >= byte");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = lanes[at_lane] <= bounded;
    }
    fuzz_check(EMBED_CALL(lane.le, ScrutLaneCfg, .word = tested, .byte = bounded) == mask_of(marked), "le is lane <= byte");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        made[at_lane] = (uint8_t)((uint8_t)(lanes[at_lane] - bounded) & 0x7Fu);
    }
    fuzz_check(EMBED_CALL(lane.sub7, ScrutLaneCfg, .word = tested, .byte = bounded) == word_of(made),
               "sub7 is each lane less the byte, modulo 128");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = lanes[at_lane] == 0u;
    }
    fuzz_check(EMBED_CALL(lane.has_zero, ScrutLaneCfg, .word = tested) == mask_of(marked), "has_zero marks zero lanes");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = (lanes[at_lane] == byte) || (ci && letter(lanes[at_lane]) && ((uint8_t)(lanes[at_lane] ^ 0x20u) == byte));
    }
    fuzz_check(EMBED_CALL(lane.eq, ScrutLaneCfg, .word = tested, .byte = byte, .ci = (embed_bool)ci) == mask_of(marked),
               "eq marks the lanes equal to the byte, a letter of either case with ci");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        const uint8_t diff = (uint8_t)(lanes[at_lane] ^ others[at_lane]);
        made[at_lane] = (ci && letter(lanes[at_lane])) ? (uint8_t)(diff & ~0x20u) : diff;
    }
    fuzz_check(EMBED_CALL(lane.xor_, ScrutLaneCfg, .word = tested, .val = val, .ci = (embed_bool)ci) == word_of(made),
               "xor_ is the lane difference, the case bit cleared on letters with ci");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = (lanes[at_lane] & fam) == (byte & fam);
    }
    fuzz_check(EMBED_CALL(lane.fam_eq, ScrutLaneCfg, .word = tested, .byte = byte, .fam = fam) == mask_of(marked),
               "fam_eq compares within the family bits");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = (lanes[at_lane] & MMGR_FAM_CS) == MMGR_FAM_CI;
    }
    fuzz_check(EMBED_CALL(lane.any_upper, ScrutLaneCfg, .word = tested) == mask_of(marked),
               "any_upper marks the 0x40 block, bit seven aside");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = (lanes[at_lane] & 0xF0u) == 0x30u;
    }
    fuzz_check(EMBED_CALL(lane.any_digit, ScrutLaneCfg, .word = tested) == mask_of(marked), "any_digit marks 0x30 to 0x3F");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = letter(lanes[at_lane]);
    }
    fuzz_check(EMBED_CALL(lane.alpha, ScrutLaneCfg, .word = tested) == mask_of(marked), "alpha marks the letters");

    const embed_word lane_mask = drawn_mask(input);
    uint8_t mask_lanes[LANES];
    lanes_of(lane_mask, mask_lanes);
    size_t count = 0u;
    size_t first = LANES;
    size_t last = LANES;
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        if (mask_lanes[at_lane] != 0u)
        {
            count += 1u;
            first = (first == LANES) ? at_lane : first;
            last = at_lane;
        }
    }
    fuzz_check(EMBED_CALL(lane.count, ScrutLaneCfg, .mask = lane_mask) == count, "count is the set lanes");
    fuzz_check(EMBED_CALL(lane.first, ScrutLaneCfg, .mask = lane_mask) == first, "first is the first set lane in memory");
    fuzz_check(EMBED_CALL(lane.last, ScrutLaneCfg, .mask = lane_mask) == last, "last is the last set lane in memory");
}

static void mask_entries(FuzzInput *input)
{
    const embed_word lane_mask = drawn_mask(input);
    const size_t bytes = fuzz_below(input, LANES * 3u);
    const size_t wi = fuzz_below(input, 6u);
    uint8_t lanes[LANES];
    uint8_t made[LANES];
    int marked[LANES];
    lanes_of(lane_mask, lanes);

    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        made[at_lane] = (lanes[at_lane] != 0u) ? 0xFFu : 0u;
    }
    fuzz_check(EMBED_CALL(mask.spread, ScrutMaskCfg, .mask = lane_mask) == word_of(made), "spread widens set lanes to bytes");

    size_t first = LANES;
    size_t last = LANES;
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        if (lanes[at_lane] != 0u)
        {
            first = (first == LANES) ? at_lane : first;
            last = at_lane;
        }
    }
    memcpy(made, lanes, LANES);
    if (first != LANES)
    {
        made[first] = 0u;
    }
    fuzz_check(EMBED_CALL(mask.drop_first, ScrutMaskCfg, .mask = lane_mask) == word_of(made), "drop_first clears the first lane");
    memcpy(made, lanes, LANES);
    if (last != LANES)
    {
        made[last] = 0u;
    }
    fuzz_check(EMBED_CALL(mask.drop_last, ScrutMaskCfg, .mask = lane_mask) == word_of(made), "drop_last clears the last lane");

    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        made[at_lane] = (at_lane < bytes) ? 0xFFu : 0u;
        marked[at_lane] = at_lane < bytes;
    }
    fuzz_check(EMBED_CALL(mask.bytes_below, ScrutMaskCfg, .bytes = bytes) == word_of(made), "bytes_below covers the first bytes");
    fuzz_check(EMBED_CALL(mask.lanes_below, ScrutMaskCfg, .bytes = bytes) == mask_of(marked), "lanes_below covers the first lanes");

    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = at_lane < first;
    }
    fuzz_check(EMBED_CALL(mask.before, ScrutMaskCfg, .mask = lane_mask) == mask_of(marked), "before is the lanes ahead of the first");

    const size_t covered = wi * LANES;
    const size_t wanted = (bytes > covered) ? (bytes - covered) : 0u;
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = at_lane < wanted;
    }
    fuzz_check(EMBED_CALL(mask.tail, ScrutMaskCfg, .bytes = bytes, .wi = wi) == mask_of(marked),
               "tail covers the bytes still wanted in the word");

    const size_t run = fuzz_below(input, LANES + 3u);
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        int starts = (run <= 1u) ? (lanes[at_lane] != 0u) : (at_lane + run <= LANES);
        for (size_t step = 0u; (run > 1u) && starts && (step < run); step += 1u)
        {
            starts = lanes[at_lane + step] != 0u;
        }
        marked[at_lane] = starts;
    }
    fuzz_check(EMBED_CALL(mask.run, ScrutMaskCfg, .mask = lane_mask, .bytes = run) == mask_of(marked),
               "run marks the lanes that begin a run of set lanes");
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        marked[at_lane] = (run >= 2u) && (run <= LANES) && (at_lane + run > LANES);
    }
    fuzz_check(EMBED_CALL(mask.run_edge, ScrutMaskCfg, .bytes = run) == mask_of(marked),
               "run_edge marks the lanes too near the end for the run");
}

static void word_entries(FuzzInput *input)
{
    static union
    {
        embed_word aligned;
        uint8_t bytes[LANES * 3u];
    } s_area;
    for (size_t at = 0u; at < sizeof s_area.bytes; at += 1u)
    {
        s_area.bytes[at] = fuzz_byte(input);
    }
    const size_t at = fuzz_below(input, LANES * 2u + 1u);
    embed_word expected;
    memcpy(&expected, s_area.bytes + at, sizeof expected);
    fuzz_check(EMBED_CALL(word.load, ScrutWordCfg, .at = s_area.bytes + at) == expected, "load is memcpy");
    const size_t aligned = at - (at % LANES);
    memcpy(&expected, s_area.bytes + aligned, sizeof expected);
    fuzz_check(EMBED_CALL(word.load_al, ScrutWordCfg, .at = s_area.bytes + aligned) == expected, "load_al is memcpy");

    uint8_t lanes[LANES];
    lanes_of(expected, lanes);
    for (size_t at_lane = 0u; at_lane < LANES; at_lane += 1u)
    {
        lanes[at_lane] = ((lanes[at_lane] >= 'A') && (lanes[at_lane] <= 'Z')) ? (uint8_t)(lanes[at_lane] + 32u) : lanes[at_lane];
    }
    fuzz_check(EMBED_CALL(word.fold_lower, ScrutWordCfg, .word = expected) == word_of(lanes),
               "fold_lower lowers the capitals and nothing else");

    const size_t bytes = fuzz_below(input, 1000u);
    fuzz_check(EMBED_CALL(word.count, ScrutWordCfg, .bytes = bytes) == (bytes + LANES - 1u) / LANES,
               "count is the words a scan of the bytes reads");
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        switch (fuzz_byte(&input) % 3u)
        {
        case 0:
            lane_entries(&input);
            break;
        case 1:
            mask_entries(&input);
            break;
        default:
            word_entries(&input);
            break;
        }
    }
    return 0;
}
