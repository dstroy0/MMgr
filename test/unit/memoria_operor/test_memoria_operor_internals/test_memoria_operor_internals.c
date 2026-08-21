// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The lane mask arm that only a big endian target asks for.
//
// memor_lo_lanes keeps the low `to` lanes, and at or above the word size it keeps the whole word.
// Only memor_span_lanes calls it, and only its big endian arm ever asks for the whole word: there
// the count is flipped to MMGR_RAW_WORD - to, so a span starting at lane zero asks for all of
// them. On little endian the count is always the ragged tail, which is one to word size minus one,
// so the saturating answer is written and never run.
//
// There is no big endian entry in MMGR_ENVIRONMENTS to reach it through, and adding one would mean
// a target this machine cannot execute. Calling the entry directly is the same assertion without
// the hardware: what is being pinned is the mask, not the byte order that asks for it.
//
// The translation unit is compiled in rather than linked, which is what makes the file-local
// entries visible - the same arrangement test_transformo_internals uses, and for the same reason.
#include "memoria_operor/memoria_operor.c"

#include "unity.h"

#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

/** @brief The lane mask for a keep count, with the rest of the work left at zero. */
static mmgr_migro_word lo_lanes_of(size_t to)
{
    MemorCtx c;

    memset(&c, 0, sizeof c);
    c.to = to;
    return memor_lo_lanes(&c);
}

void test_lo_lanes_keeps_the_low_lanes(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(0u, lo_lanes_of(0u), "no lanes kept is an empty mask");
    TEST_ASSERT_EQUAL_HEX8(0xFFu, (unsigned char)lo_lanes_of(1u));
    TEST_ASSERT_EQUAL_MESSAGE((mmgr_migro_word)0xFFFFu, (mmgr_migro_word)(lo_lanes_of(2u) & 0xFFFFu),
                              "two lanes is two bytes of ones");
}

void test_lo_lanes_at_the_word_size_keeps_everything(void)
{
    // The shift that builds the mask is by the full width at this count, which is what the
    // saturating answer exists to avoid: one to the width of the type is not a value C defines.
    TEST_ASSERT_EQUAL_MESSAGE((mmgr_migro_word) ~(mmgr_migro_word)0, lo_lanes_of(MMGR_RAW_WORD),
                              "a whole word of lanes is a whole word of ones");
}

void test_lo_lanes_past_the_word_size_keeps_everything(void)
{
    // Above the width for the same reason, so a flipped count that overshoots is still a mask and
    // not a shift nobody defined.
    TEST_ASSERT_EQUAL((mmgr_migro_word) ~(mmgr_migro_word)0, lo_lanes_of(MMGR_RAW_WORD + 1u));
}
