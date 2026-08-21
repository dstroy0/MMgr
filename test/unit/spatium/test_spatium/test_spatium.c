// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "spatium/spatium.h"

void test_spat_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("spatium.h compiled with no header before it");
}

void test_spat_namespace_is_wired(void)
{
    const SpatiumNs *ns = &spat;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(SpatiumNs), sizeof(*ns), "the namespace instance is not its own type");
}

/* ---------------------------------------------------------------------------------------------
 * construction
 * ------------------------------------------------------------------------------------------- */

void test_from_takes_a_buffer(void)
{
    uint8_t buf[8];
    const mmgr_spat s = spat.from(buf, sizeof buf);

    TEST_ASSERT_EQUAL_PTR(buf, s.buf);
    TEST_ASSERT_EQUAL_size_t(8u, s.cap);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
    TEST_ASSERT_FALSE(s.overflow);
}

void test_from_a_null_pointer_is_a_sizing_pass(void)
{
    const mmgr_spat s = spat.from(NULL, 64u);

    TEST_ASSERT_NULL(s.buf);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, s.cap, "no storage means no room, whatever cap was asked for");
    TEST_ASSERT_FALSE(spat.has_storage(s));
}

void test_from_a_zero_capacity_has_no_storage(void)
{
    uint8_t buf[8];
    const mmgr_spat s = spat.from(buf, 0u);

    TEST_ASSERT_NULL_MESSAGE(s.buf, "a zero capacity buffer is the same as no buffer");
    TEST_ASSERT_EQUAL_size_t(0u, s.cap);
}

void test_cfrom_takes_a_read_only_buffer(void)
{
    static const uint8_t buf[4] = {1u, 2u, 3u, 4u};
    const mmgr_fspat s = spat.cfrom(buf, sizeof buf);

    TEST_ASSERT_EQUAL_PTR(buf, s.buf);
    TEST_ASSERT_EQUAL_size_t(4u, s.len);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
    TEST_ASSERT_TRUE(spat.cok(s));
}

void test_cfrom_guards_the_same_two_ways(void)
{
    static const uint8_t buf[4] = {0};
    TEST_ASSERT_NULL(spat.cfrom(NULL, 4u).buf);
    TEST_ASSERT_EQUAL_size_t(0u, spat.cfrom(buf, 0u).len);
}

/* ---------------------------------------------------------------------------------------------
 * state
 * ------------------------------------------------------------------------------------------- */

void test_ok_is_storage_and_no_overflow(void)
{
    uint8_t buf[8];
    mmgr_spat s = spat.from(buf, sizeof buf);
    TEST_ASSERT_TRUE(spat.ok(s));

    s.overflow = MMGR_TRUE;
    TEST_ASSERT_FALSE_MESSAGE(spat.ok(s), "an overflowed span is not ok even though it has storage");
    TEST_ASSERT_TRUE_MESSAGE(spat.has_storage(s), "overflow does not take the storage away");

    TEST_ASSERT_FALSE(spat.ok(spat.from(NULL, 0u)));
}

void test_cok_is_storage_and_no_error(void)
{
    static const uint8_t buf[4] = {0};
    mmgr_fspat s = spat.cfrom(buf, sizeof buf);
    TEST_ASSERT_TRUE(spat.cok(s));

    s.err = MMGR_TRUE;
    TEST_ASSERT_FALSE(spat.cok(s));
    TEST_ASSERT_FALSE(spat.cok(spat.cfrom(NULL, 0u)));
}

void test_len_is_what_was_written(void)
{
    uint8_t buf[8];
    mmgr_spat s = spat.from(buf, sizeof buf);
    TEST_ASSERT_EQUAL_size_t(0u, spat.len(s));

    s.pos = 5u;
    TEST_ASSERT_EQUAL_size_t(5u, spat.len(s));
}

void test_room_is_what_is_left(void)
{
    uint8_t buf[8];
    mmgr_spat s = spat.from(buf, sizeof buf);
    TEST_ASSERT_EQUAL_size_t(8u, spat.room(s));

    s.pos = 3u;
    TEST_ASSERT_EQUAL_size_t(5u, spat.room(s));

    s.pos = 8u;
    TEST_ASSERT_EQUAL_size_t(0u, spat.room(s));
}

void test_room_of_a_position_past_the_end_is_zero(void)
{
    uint8_t buf[8];
    mmgr_spat s = spat.from(buf, sizeof buf);
    s.pos = 99u;

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, spat.room(s), "room never goes negative and never wraps");
}

void test_reset_clears_the_position_and_the_latch(void)
{
    uint8_t buf[8];
    mmgr_spat s = spat.from(buf, sizeof buf);
    s.pos = 4u;
    s.overflow = MMGR_TRUE;

    spat.reset(&s);
    TEST_ASSERT_EQUAL_size_t(0u, s.pos);
    TEST_ASSERT_FALSE(s.overflow);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(buf, s.buf, "reset keeps the storage");
}

/* ---------------------------------------------------------------------------------------------
 * slicing
 * ------------------------------------------------------------------------------------------- */

void test_after_moves_the_start(void)
{
    uint8_t buf[8];
    const mmgr_spat s = spat.after(spat.from(buf, sizeof buf), 3u);

    TEST_ASSERT_EQUAL_PTR(buf + 3, s.buf);
    TEST_ASSERT_EQUAL_size_t(5u, s.cap);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, s.pos, "a slice starts empty, it does not inherit a position");
}

void test_after_the_end_has_no_storage(void)
{
    uint8_t buf[8];
    TEST_ASSERT_NULL(spat.after(spat.from(buf, sizeof buf), 8u).buf);
    TEST_ASSERT_NULL(spat.after(spat.from(buf, sizeof buf), 99u).buf);
    TEST_ASSERT_NULL_MESSAGE(spat.after(spat.from(NULL, 0u), 0u).buf, "there is nothing to slice");
}

void test_first_takes_a_prefix(void)
{
    uint8_t buf[8];
    const mmgr_spat s = spat.first(spat.from(buf, sizeof buf), 3u);

    TEST_ASSERT_EQUAL_PTR(buf, s.buf);
    TEST_ASSERT_EQUAL_size_t(3u, s.cap);
}

void test_first_clamps_to_the_capacity(void)
{
    uint8_t buf[8];
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, spat.first(spat.from(buf, sizeof buf), 99u).cap,
                                     "a prefix longer than the span is the whole span");
    TEST_ASSERT_NULL(spat.first(spat.from(NULL, 0u), 4u).buf);
}

void test_first_of_zero_has_no_storage(void)
{
    uint8_t buf[8];
    TEST_ASSERT_EQUAL_size_t(0u, spat.first(spat.from(buf, sizeof buf), 0u).cap);
}

void test_produced_is_the_written_part(void)
{
    uint8_t buf[8];
    mmgr_spat s = spat.from(buf, sizeof buf);
    s.pos = 5u;

    const mmgr_fspat f = spat.produced(s);
    TEST_ASSERT_EQUAL_PTR(buf, f.buf);
    TEST_ASSERT_EQUAL_size_t(5u, f.len);
}

void test_produced_of_a_bad_span_is_empty(void)
{
    uint8_t buf[8];
    mmgr_spat s = spat.from(buf, sizeof buf);
    s.pos = 5u;
    s.overflow = MMGR_TRUE;

    TEST_ASSERT_NULL_MESSAGE(spat.produced(s).buf, "an overflowed span produced nothing that can be trusted");
    TEST_ASSERT_NULL(spat.produced(spat.from(NULL, 0u)).buf);
}

void test_read_takes_a_prefix_of_the_storage(void)
{
    uint8_t buf[8];
    const mmgr_fspat f = spat.read(spat.from(buf, sizeof buf), 3u);

    TEST_ASSERT_EQUAL_PTR(buf, f.buf);
    TEST_ASSERT_EQUAL_size_t(3u, f.len);
}

void test_read_clamps_and_guards(void)
{
    uint8_t buf[8];
    TEST_ASSERT_EQUAL_size_t(8u, spat.read(spat.from(buf, sizeof buf), 99u).len);
    TEST_ASSERT_NULL(spat.read(spat.from(NULL, 0u), 4u).buf);
}
