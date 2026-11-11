/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file spatium_fuzz.c
 * @brief A libFuzzer harness over the spans. Every entry runs on spans of any storage, cap, position and overflow,
 *        each answer against its contract computed field by field.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#include "spatium/spatium.h"

#include "fuzz_input.h"

#define FUZZ_BYTES 64u

static uint8_t s_store[FUZZ_BYTES];

static mmgr_span drawn_span(FuzzInput *input)
{
    const uint8_t kind = fuzz_byte(input);
    mmgr_span span;
    span.buf = ((kind & 7u) == 0u) ? NULL : s_store;
    span.cap = ((kind & 0x18u) == 0u) ? 0u : fuzz_below(input, FUZZ_BYTES + 1u);
    span.pos = fuzz_below(input, FUZZ_BYTES * 2u);
    span.overflow = (embed_bool)((kind & 0x20u) != 0u);
    return span;
}

static int same_span(mmgr_span left, mmgr_span right)
{
    return (left.buf == right.buf) && (left.cap == right.cap) && (left.pos == right.pos) &&
           ((left.overflow != 0) == (right.overflow != 0));
}

static int same_cspan(mmgr_cspan left, mmgr_cspan right)
{
    return (left.buf == right.buf) && (left.len == right.len) && (left.pos == right.pos) &&
           ((left.err != 0) == (right.err != 0));
}

static const mmgr_span s_failed = {NULL, 0u, 0u, EMBED_TRUE};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzInput input = fuzz_input(data, size);
    while (fuzz_left(&input))
    {
        const mmgr_span span = drawn_span(&input);
        const size_t count = fuzz_below(&input, FUZZ_BYTES * 2u);
        const int storage = (span.buf != NULL) && (span.cap != 0u);

        if (span.buf != NULL && span.cap != 0u)
        {
            const mmgr_span built = EMBED_CALL(spat.from, SpatiumCfg, .buf = span.buf, .cap = span.cap);
            fuzz_check(same_span(built, (mmgr_span){span.buf, span.cap, 0u, EMBED_FALSE}),
                       "from builds an empty span over its buffer");
        }
        const mmgr_cspan cbuilt = EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = span.buf, .cap = span.cap);
        fuzz_check(same_cspan(cbuilt, (mmgr_cspan){span.buf, span.cap, 0u, EMBED_FALSE}),
                   "cfrom builds a read span as it is handed");

        fuzz_check((EMBED_CALL(spat.has_storage, SpatiumCfg, .span = span) != 0) == storage,
                   "has_storage is a buffer and a cap");
        fuzz_check((EMBED_CALL(spat.ok, SpatiumCfg, .span = span) != 0) == (storage && !span.overflow),
                   "ok is storage and no overflow");

        const mmgr_cspan cspan = {span.buf, span.cap, span.pos, span.overflow};
        fuzz_check((EMBED_CALL(spat.cok, SpatiumCfg, .cspan = cspan) != 0) == (storage && !span.overflow),
                   "cok is storage and no error");

        mmgr_span rewound = span;
        EMBED_CALL(spat.reset, SpatiumCfg, .at = &rewound);
        fuzz_check(same_span(rewound, (mmgr_span){span.buf, span.cap, 0u, EMBED_FALSE}),
                   "reset rewinds the position and clears the overflow");

        const mmgr_span rest = EMBED_CALL(spat.after, SpatiumCfg, .span = span, .count = count);
        if (count > span.cap)
        {
            fuzz_check(same_span(rest, s_failed), "after past the cap is a failed span");
        }
        else
        {
            const mmgr_span expected = {(span.buf != NULL) ? (span.buf + count) : NULL, span.cap - count,
                                        (span.pos > count) ? (span.pos - count) : 0u, span.overflow};
            fuzz_check(same_span(rest, expected), "after covers what is left past count");
        }

        const mmgr_span head = EMBED_CALL(spat.first, SpatiumCfg, .span = span, .count = count);
        if (count > span.cap)
        {
            fuzz_check(same_span(head, s_failed), "first past the cap is a failed span");
        }
        else
        {
            const mmgr_span expected = {span.buf, count, (span.pos < count) ? span.pos : count, span.overflow};
            fuzz_check(same_span(head, expected), "first covers the opening count bytes");
        }

        const mmgr_cspan view = EMBED_CALL(spat.read, SpatiumCfg, .span = span, .count = count);
        const mmgr_cspan view_expected = {span.buf, (count < span.pos) ? count : span.pos, 0u,
                                          (embed_bool)(span.overflow || (count > span.pos))};
        fuzz_check(same_cspan(view, view_expected), "read covers the first count written and marks a short read");

        const mmgr_cspan produced = EMBED_CALL(spat.produced, SpatiumCfg, .span = span);
        const mmgr_cspan produced_expected = {span.buf, span.pos, 0u, span.overflow};
        fuzz_check(same_cspan(produced, produced_expected), "produced covers everything written");
    }
    return 0;
}
