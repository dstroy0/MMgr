// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_spatium_accuracy.c
 * @brief Checks which bytes a derived span names, against the rules spatium.h states, over every
 *        combination of extent, cursor, flag and count.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note A span is four numbers, and every walk here is arithmetic on them. A walk that produced a
 *       span one byte short, or carried the cursor forward without rebasing it, hands back something
 *       that looks like a span and names the wrong bytes.
 * @note The reference is each rule from the header written out separately: where the derived span
 *       starts, how long it is, where its cursor lands, and what its flag carries. Nothing here calls
 *       one entry to check another.
 * @note Small extents are the whole point. Every rule is a comparison between the count and the
 *       extent or the cursor, and every ordering among those occurs within the first few integers, so
 *       the grid is walked exhaustively instead of sampled.
 * @note The spans the grid walks are built here as values and not through the constructors. The walks
 *       take a span inside their argument pack, so any span is a legal input, and building them
 *       directly is what reaches an extent of zero and a flag already set.
 * @note One case uses real storage and writes through a derived span, since the header states a
 *       derived span is a second view of the same bytes and not a copy of them. Numbers alone cannot
 *       show that.
 * @note Contract checks live in test_spatium. This file asks which bytes a span names.
 */
#include <stdint.h>
#include <stdio.h>

#include "spatium/spatium.h"

#include "unity.h"

/**
 * @brief Expands to 8u, the largest extent the exhaustive grid walks.
 *
 * @note Every rule compares the count against the extent or the cursor, and every ordering among
 *       three small numbers occurs well inside this. A larger grid repeats orderings it already
 *       covered.
 */
#define MMGR_ACCURACY_SPAT_MAX 8u

/**
 * @brief Expands to 16u, the bytes the storage every span here points at holds.
 *
 * @note Twice the largest extent. A span that ran past its own cap still lands inside the object, and
 *       the case then reports a wrong extent instead of faulting.
 */
#define MMGR_ACCURACY_SPAT_BUFFER 16u

/**
 * @brief Storage the spans in the grid point at.
 *
 * @note File scope, because the grid compares addresses derived from it and a case-local array would
 *       give each case a different base. Nothing in the grid reads or writes through it; the
 *       addresses alone are what the walks are checked on.
 */
static uint8_t s_storage[MMGR_ACCURACY_SPAT_BUFFER];

/**
 * @brief Checks a fill span against the four numbers it is expected to carry.
 *
 * @param[in] produced The span the module returned.
 * @param[in] buf      Address it should start at [BORROWS].
 * @param[in] cap      Extent it should cover.
 * @param[in] pos      Cursor it should carry.
 * @param[in] overflow Flag it should carry.
 * @param[in] label    Text naming the case, printed when a member disagrees.
 * @note All four are checked every time. A walk that got the extent right and the cursor wrong hands
 *       back a span that writes in the right place and reports the wrong amount written.
 */
static void accuracy_expect_span(mmgr_span produced, const uint8_t *buf, size_t cap, size_t pos, embed_bool overflow,
                                 const char *label)
{
    char message[192];

    (void)snprintf(message, sizeof message, "%s: the span starts at the wrong byte", label);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(buf, produced.buf, message);
    (void)snprintf(message, sizeof message, "%s: the span covers the wrong number of bytes", label);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(cap, produced.cap, message);
    (void)snprintf(message, sizeof message, "%s: the span carries the wrong cursor", label);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(pos, produced.pos, message);
    (void)snprintf(message, sizeof message, "%s: the span carries the wrong overflow flag", label);
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)overflow, (int)produced.overflow, message);
}

/**
 * @brief Checks a read span against the four numbers it is expected to carry.
 *
 * @param[in] produced The span the module returned.
 * @param[in] buf      Address it should start at [BORROWS].
 * @param[in] len      Extent it should cover.
 * @param[in] pos      Cursor it should carry.
 * @param[in] err      Flag it should carry.
 * @param[in] label    Text naming the case, printed when a member disagrees.
 * @note The read side of accuracy_expect_span. The two cannot share a body, because a read span names
 *       its extent len and a fill span names it cap.
 */
static void accuracy_expect_cspan(mmgr_cspan produced, const uint8_t *buf, size_t len, size_t pos, embed_bool err,
                                  const char *label)
{
    char message[192];

    (void)snprintf(message, sizeof message, "%s: the read span starts at the wrong byte", label);
    TEST_ASSERT_EQUAL_PTR_MESSAGE(buf, produced.buf, message);
    (void)snprintf(message, sizeof message, "%s: the read span covers the wrong number of bytes", label);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(len, produced.len, message);
    (void)snprintf(message, sizeof message, "%s: the read span carries the wrong cursor", label);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(pos, produced.pos, message);
    (void)snprintf(message, sizeof message, "%s: the read span carries the wrong error flag", label);
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)err, (int)produced.err, message);
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note The storage the grid points at is never read through, and the one case that writes through a
 *       span fills what it needs itself.
 */
void setUp(void)
{
}

/**
 * @brief Runs after each Unity test case.
 *
 * @note Required alongside setUp, since the generated runner calls both around every case.
 * @note Nothing here allocates, so there is nothing to release.
 */
void tearDown(void)
{
}

/**
 * @brief Checks the two constructors against the spans they are documented to build.
 *
 * @note The constructors are what every other case starts from. A constructor that set a member
 *       wrongly would be read as a walk defect everywhere else.
 * @note A fresh span starts at the buffer it was given, covers the extent it was given, rests at zero
 *       and carries no flag. All four are checked.
 * @note The read constructor is offered a null buffer and a zero extent, which it is documented to
 *       accept where the fill constructor asserts. What reports those unusable is cok, checked below.
 */
void test_the_constructors_build_the_spans_they_are_documented_to(void)
{
    accuracy_expect_span(EMBED_CALL(spat.from, SpatiumCfg, .buf = s_storage, .cap = MMGR_ACCURACY_SPAT_BUFFER),
                         s_storage, MMGR_ACCURACY_SPAT_BUFFER, 0u, EMBED_FALSE, "a fresh fill span");
    accuracy_expect_cspan(EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = s_storage, .cap = MMGR_ACCURACY_SPAT_BUFFER),
                          s_storage, MMGR_ACCURACY_SPAT_BUFFER, 0u, EMBED_FALSE, "a fresh read span");
    accuracy_expect_cspan(EMBED_CALL(spat.cfrom, SpatiumCfg, .cbuf = NULL, .cap = 0u), NULL, 0u, 0u, EMBED_FALSE,
                          "a read span over nothing");
}

/**
 * @brief Checks the span beginning a given number of bytes in, over the whole grid.
 *
 * @note The rules are the header's: the start moves forward by the count, the extent shrinks by it,
 *       the cursor is rebased and rests at zero once the skip has passed it, and the flag comes
 *       forward unchanged. Each is written out separately here.
 * @note A count of exactly the extent is documented to give an empty span that has not failed, and a
 *       count past it is documented to fail. Both boundaries are in the grid, and they are one apart.
 * @note Every cursor from zero to the extent is offered at every extent, with the flag both ways, so
 *       the rebasing rule is checked on both sides of the skip.
 */
void test_the_span_beginning_a_count_in_names_the_right_bytes(void)
{
    for (size_t cap = 0u; cap <= MMGR_ACCURACY_SPAT_MAX; cap++)
    {
        for (size_t pos = 0u; pos <= cap; pos++)
        {
            for (unsigned flag = 0u; flag < 2u; flag++)
            {
                for (size_t count = 0u; count <= (cap + 2u); count++)
                {
                    // Explicit cast puts the loop counter in the embed_bool the member holds
                    const embed_bool overflow = (embed_bool)flag;
                    const mmgr_span parent = {s_storage, cap, pos, overflow};
                    const mmgr_span produced = EMBED_CALL(spat.after, SpatiumCfg, .span = parent, .count = count);
                    char label[160];

                    (void)snprintf(label, sizeof label, "after %u of a span of %u at %u with overflow %u",
                                   (unsigned)count, (unsigned)cap, (unsigned)pos, flag);

                    if (count > cap)
                    {
                        accuracy_expect_span(produced, NULL, 0u, 0u, EMBED_TRUE, label);
                    }
                    else
                    {
                        accuracy_expect_span(produced, s_storage + count, cap - count,
                                             (pos > count) ? (pos - count) : 0u, overflow, label);
                    }
                }
            }
        }
    }
}

/**
 * @brief Checks the span covering only the first bytes, over the whole grid.
 *
 * @note The rules are the header's: the start does not move, the extent becomes the count, the cursor
 *       is held down to the shorter extent, and the flag comes forward unchanged.
 * @note Holding the cursor down is the rule a narrowing gets wrong most quietly. A span whose cursor
 *       sat past the new extent would report more written than it covers.
 * @note A count past the extent is documented to fail, and that boundary sits one past a count of
 *       exactly the extent, which succeeds.
 */
void test_the_span_covering_the_first_count_names_the_right_bytes(void)
{
    for (size_t cap = 0u; cap <= MMGR_ACCURACY_SPAT_MAX; cap++)
    {
        for (size_t pos = 0u; pos <= cap; pos++)
        {
            for (unsigned flag = 0u; flag < 2u; flag++)
            {
                for (size_t count = 0u; count <= (cap + 2u); count++)
                {
                    // Explicit cast puts the loop counter in the embed_bool the member holds
                    const embed_bool overflow = (embed_bool)flag;
                    const mmgr_span parent = {s_storage, cap, pos, overflow};
                    const mmgr_span produced = EMBED_CALL(spat.first, SpatiumCfg, .span = parent, .count = count);
                    char label[160];

                    (void)snprintf(label, sizeof label, "first %u of a span of %u at %u with overflow %u",
                                   (unsigned)count, (unsigned)cap, (unsigned)pos, flag);

                    if (count > cap)
                    {
                        accuracy_expect_span(produced, NULL, 0u, 0u, EMBED_TRUE, label);
                    }
                    else
                    {
                        accuracy_expect_span(produced, s_storage, count, (pos < count) ? pos : count, overflow, label);
                    }
                }
            }
        }
    }
}

/**
 * @brief Checks the read span over what was written, over the whole grid.
 *
 * @note The rules are the header's: the view starts where the fill span does, covers the smaller of
 *       the count and the cursor, rests at zero, and is marked in error when the fill span overflowed
 *       or when more was asked for than was written.
 * @note The error rule has two independent causes, and the grid offers both separately and together.
 *       A view that reported only one of them looks correct on half the grid.
 * @note A count equal to the cursor is the boundary between asking for what is there and asking for
 *       more, and it sits one below a count that fails.
 */
void test_the_read_span_over_what_was_written_covers_the_right_bytes(void)
{
    for (size_t cap = 0u; cap <= MMGR_ACCURACY_SPAT_MAX; cap++)
    {
        for (size_t pos = 0u; pos <= cap; pos++)
        {
            for (unsigned flag = 0u; flag < 2u; flag++)
            {
                for (size_t count = 0u; count <= (cap + 2u); count++)
                {
                    // Explicit cast puts the loop counter in the embed_bool the member holds
                    const embed_bool overflow = (embed_bool)flag;
                    const mmgr_span parent = {s_storage, cap, pos, overflow};
                    const mmgr_cspan produced = EMBED_CALL(spat.read, SpatiumCfg, .span = parent, .count = count);
                    // Explicit cast narrows the combined test into the embed_bool the member holds
                    const embed_bool expected_err = (embed_bool)(overflow || (count > pos));
                    char label[160];

                    (void)snprintf(label, sizeof label, "read %u of a span of %u at %u with overflow %u",
                                   (unsigned)count, (unsigned)cap, (unsigned)pos, flag);
                    accuracy_expect_cspan(produced, s_storage, (count < pos) ? count : pos, 0u, expected_err, label);
                }
            }
        }
    }
}

/**
 * @brief Checks that the read span over everything written covers exactly the cursor's bytes.
 *
 * @note Documented as the same step against the span's own cursor, which is the one count that cannot
 *       ask for more than was written. Its error flag is the fill span's overflow alone.
 * @note Checked directly against the rule and not against the other entry, which keeps the two from
 *       being measured against each other.
 */
void test_the_read_span_over_everything_written_covers_the_cursor(void)
{
    for (size_t cap = 0u; cap <= MMGR_ACCURACY_SPAT_MAX; cap++)
    {
        for (size_t pos = 0u; pos <= cap; pos++)
        {
            for (unsigned flag = 0u; flag < 2u; flag++)
            {
                // Explicit cast puts the loop counter in the embed_bool the member holds
                const embed_bool overflow = (embed_bool)flag;
                const mmgr_span parent = {s_storage, cap, pos, overflow};
                char label[160];

                (void)snprintf(label, sizeof label, "everything written of a span of %u at %u with overflow %u",
                               (unsigned)cap, (unsigned)pos, flag);
                accuracy_expect_cspan(EMBED_CALL(spat.produced, SpatiumCfg, .span = parent), s_storage, pos, 0u,
                                      overflow, label);
            }
        }
    }
}

/**
 * @brief Checks the three predicates against the conditions they are documented to test.
 *
 * @note Each is a combination of two or three conditions, and a predicate that dropped one of them
 *       agrees with the rule on most of the grid. Every combination is offered.
 * @note The two usability predicates differ in the member they read, which is what keeps a read span
 *       from being handed where a fill span belongs, so both are swept separately.
 * @note A null buffer and a zero extent are each offered on their own as well as together, since a
 *       predicate testing only one of them passes whenever both hold.
 */
void test_the_predicates_test_what_they_are_documented_to(void)
{
    for (unsigned has_buffer = 0u; has_buffer < 2u; has_buffer++)
    {
        for (size_t cap = 0u; cap <= 2u; cap++)
        {
            for (unsigned flag = 0u; flag < 2u; flag++)
            {
                uint8_t *const buf = (has_buffer != 0u) ? s_storage : NULL;
                // Explicit cast puts the loop counter in the embed_bool the member holds
                const embed_bool raised = (embed_bool)flag;
                const mmgr_span fill = {buf, cap, 0u, raised};
                const mmgr_cspan reader = {buf, cap, 0u, raised};
                const embed_bool has_storage = (embed_bool)((buf != NULL) && (cap != 0u));
                char label[160];

                (void)snprintf(label, sizeof label, "a buffer of %u bytes at %s with the flag %u", (unsigned)cap,
                               (has_buffer != 0u) ? "an address" : "no address", flag);

                TEST_ASSERT_EQUAL_INT_MESSAGE((int)has_storage,
                                              (int)EMBED_CALL(spat.has_storage, SpatiumCfg, .span = fill), label);
                TEST_ASSERT_EQUAL_INT_MESSAGE((int)(embed_bool)(has_storage && !raised),
                                              (int)EMBED_CALL(spat.ok, SpatiumCfg, .span = fill), label);
                TEST_ASSERT_EQUAL_INT_MESSAGE((int)(embed_bool)(has_storage && !raised),
                                              (int)EMBED_CALL(spat.cok, SpatiumCfg, .cspan = reader), label);
            }
        }
    }
}

/**
 * @brief Checks that a derived span is a second view of the same bytes and not a copy of them.
 *
 * @note The header states a write through either view is seen by both, and numbers alone cannot show
 *       that. This case writes through the derived span and reads the parent's own storage back.
 * @note Both narrowings are covered. The span beginning a count in starts further along the same
 *       buffer, and the span covering the first bytes starts on the same byte with a shorter extent,
 *       so the two are checked at different addresses.
 * @note The bytes outside the derived span are checked to be untouched, which is what shows the view
 *       is bounded and not merely offset.
 */
void test_a_derived_span_is_a_second_view_of_the_same_bytes(void)
{
    uint8_t storage[MMGR_ACCURACY_SPAT_BUFFER];

    for (size_t count = 0u; count < MMGR_ACCURACY_SPAT_BUFFER; count++)
    {
        char label[128];

        for (size_t index = 0u; index < sizeof storage; index++)
        {
            storage[index] = 0u;
        }

        const mmgr_span whole = EMBED_CALL(spat.from, SpatiumCfg, .buf = storage, .cap = sizeof storage);
        const mmgr_span rest = EMBED_CALL(spat.after, SpatiumCfg, .span = whole, .count = count);

        (void)snprintf(label, sizeof label, "a span beginning %u bytes in", (unsigned)count);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(storage + count, rest.buf, label);

        if (rest.cap != 0u)
        {
            rest.buf[0] = 0xC3u;
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xC3u, storage[count], "a write through the derived span was not seen");
            for (size_t index = 0u; index < sizeof storage; index++)
            {
                if (index != count)
                {
                    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, storage[index], "the write reached a byte outside the view");
                }
            }
        }
    }

    for (size_t index = 0u; index < sizeof storage; index++)
    {
        storage[index] = 0u;
    }

    const mmgr_span whole = EMBED_CALL(spat.from, SpatiumCfg, .buf = storage, .cap = sizeof storage);
    const mmgr_span head = EMBED_CALL(spat.first, SpatiumCfg, .span = whole, .count = 4u);

    TEST_ASSERT_EQUAL_PTR_MESSAGE(storage, head.buf, "a narrowed span does not start on the same byte");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, head.cap, "a narrowed span does not cover the bytes it was given");
    head.buf[3] = 0x7Fu;
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x7Fu, storage[3], "a write through the narrowed span was not seen");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, storage[4], "the write reached past the narrowed span");
}

/**
 * @brief Checks that a rewind returns the cursor and clears the flag, leaving the bytes alone.
 *
 * @note The one entry that changes a span, and the only one that clears the flag, which is otherwise
 *       sticky for the span's whole life.
 * @note The bytes are checked to survive, since the header states a reset span hands out storage that
 *       still holds whatever the last fill wrote. A rewind that cleared them would be a different
 *       operation with the same name.
 * @note The extent and the start are checked to be unchanged as well, so the rewind is exactly the
 *       two members it names and nothing else.
 */
void test_a_rewind_returns_the_cursor_and_leaves_the_bytes(void)
{
    uint8_t storage[MMGR_ACCURACY_SPAT_BUFFER];

    for (size_t index = 0u; index < sizeof storage; index++)
    {
        // Explicit cast narrows the mixed index to the byte the storage holds
        storage[index] = (uint8_t)(index + 1u);
    }

    mmgr_span span = EMBED_CALL(spat.from, SpatiumCfg, .buf = storage, .cap = sizeof storage);

    span.pos = 9u;
    span.overflow = EMBED_TRUE;

    EMBED_CALL(spat.reset, SpatiumCfg, .at = &span);

    accuracy_expect_span(span, storage, sizeof storage, 0u, EMBED_FALSE, "a rewound span");
    for (size_t index = 0u; index < sizeof storage; index++)
    {
        // Explicit cast narrows the mixed index to the byte the comparison takes
        TEST_ASSERT_EQUAL_HEX8_MESSAGE((uint8_t)(index + 1u), storage[index], "a rewind cleared the bytes");
    }
}
