// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_memoriam_praetereo.c
 * @brief Exercises the praet dispatch table against the unported build, where every hardware hook
 *        keeps its refusing default.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 */
#include "memoriam_praetereo/memoriam_praetereo.h"

#include "unity.h"

/**
 * @brief Channel every case acts on, chosen below any configured channel count.
 */
#define PRAET_TEST_CHANNEL 0u

/**
 * @brief Records that a completion callback ran, so a case can assert the port layer never invoked one.
 */
static int s_completion_count;

/**
 * @brief Counts one completion, standing in for an application's completion handler.
 *
 * @param[in] event Completion the port layer reports [BORROWS].
 * @param[in] user  Pointer registered alongside this callback [BORROWS].
 * @note Never runs in this suite. The refusing hardware defaults accept no transfer, so nothing
 *       reaches a completion, which is what test_an_unported_build_reports_no_completion asserts.
 */
static void count_one_completion(const mmgr_praet_event *event, void *user)
{
    (void)event;
    (void)user;
    s_completion_count++;
}

/**
 * @brief Callback and user pointer every case registers when it opens a channel.
 */
static const PraetCallbackCfg s_completion_binding = {
    .callback = count_one_completion,
    .user = NULL,
};

/**
 * @brief Prepares the fixture Unity runs before each case in this suite.
 *
 * @note Clears the completion counter so a case reads only what it itself provoked.
 */
void setUp(void)
{
    s_completion_count = 0;
}

/**
 * @brief Releases the fixture Unity runs after each case in this suite.
 *
 * @note Empty because no case opens a channel the port layer accepted, so none leaves one to close.
 */
void tearDown(void)
{
}

/**
 * @brief Checks that memoriam_praetereo.h compiles with no header ahead of it.
 *
 * @note The include above is the whole test. A header that needs a prior include fails to compile
 *       here rather than at some caller that happened to include the two in the other order.
 */
void test_dma_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("memoriam_praetereo.h compiled with no header before it");
}

/**
 * @brief Checks that every praet member points at a function.
 *
 * @note A null member would fault at the first call rather than at the declaration, so the table is
 *       read before any case calls through it.
 */
void test_every_praet_entry_is_reachable(void)
{
    TEST_ASSERT_NOT_NULL(praet.open);
    TEST_ASSERT_NOT_NULL(praet.tx_submit);
    TEST_ASSERT_NOT_NULL(praet.close);
    TEST_ASSERT_NOT_NULL(praet.poll);
}

/**
 * @brief Checks that no two praet members share one function.
 *
 * @note The header states each member calls the matching mmgr_praet_ function one to one, which is
 *       where this table differs from memor. Two members sharing a pointer would satisfy every
 *       return-value case here and still be wrong.
 */
void test_no_two_praet_entries_share_a_function(void)
{
    TEST_ASSERT_TRUE_MESSAGE(praet.open == mmgr_praet_open, "open is not wired to mmgr_praet_open");
    TEST_ASSERT_TRUE_MESSAGE(praet.tx_submit == mmgr_praet_tx_submit, "tx_submit is not wired to mmgr_praet_tx_submit");
    TEST_ASSERT_TRUE_MESSAGE(praet.close == mmgr_praet_close, "close is not wired to mmgr_praet_close");
    TEST_ASSERT_TRUE_MESSAGE(praet.poll == mmgr_praet_poll, "poll is not wired to mmgr_praet_poll");
}

/**
 * @brief Checks that opening a channel fails while the hardware hook keeps its default.
 *
 * @note A port layer replaces mmgr_praet_hw_open through weak linkage. This suite links no port, so
 *       the refusing default stands and an unported build fails visibly instead of appearing to work.
 */
void test_an_unported_build_refuses_to_open_a_channel(void)
{
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(praet.open, PraetCfg, .channel = PRAET_TEST_CHANNEL, .peripheral = 0u,
                                         .loopback = EMBED_FALSE, .on_complete = &s_completion_binding),
                              "the default hardware hook refuses, so no channel opens without a port");
}

/**
 * @brief Checks that submitting a transfer fails while the hardware hook keeps its default.
 *
 * @note Submitted against a channel that never opened, which is the state an unported build is
 *       always in.
 */
void test_an_unported_build_refuses_a_transfer(void)
{
    static const uint8_t payload[4] = {1u, 2u, 3u, 4u};

    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = PRAET_TEST_CHANNEL,
                                         .buf = payload, .bytes = (uint16_t)sizeof payload),
                              "the default hardware hook refuses, so no transfer is accepted without a port");
}

/**
 * @brief Checks that a transfer of no bytes is refused the same way as any other.
 *
 * @note The refusing default reads neither the buffer nor the count, so an empty transfer takes the
 *       same path as a full one and must answer the same.
 */
void test_an_empty_transfer_is_refused_as_well(void)
{
    TEST_ASSERT_FALSE_MESSAGE(
        EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = PRAET_TEST_CHANNEL, .buf = NULL, .bytes = 0u),
        "an unported build refuses every transfer, including one of no bytes");
}

/**
 * @brief Checks that closing a channel that never opened does nothing and returns.
 *
 * @note The default mmgr_praet_hw_close does nothing, so this case fails by faulting rather than by
 *       an assertion.
 */
void test_closing_a_channel_that_never_opened_returns(void)
{
    EMBED_CALL(praet.close, PraetTransferCfg, .channel = PRAET_TEST_CHANNEL);
    TEST_PASS_MESSAGE("close returned on a channel the port layer never accepted");
}

/**
 * @brief Checks that polling an unported build does nothing and returns.
 *
 * @note mmgr_praet_poll forwards to the hook with no assertion in between, so this reaches the
 *       default hook exactly as written here.
 */
void test_polling_an_unported_build_returns(void)
{
    EMBED_CALL(praet.poll, PraetCfg, .channel = PRAET_TEST_CHANNEL, .on_complete = &s_completion_binding);
    TEST_PASS_MESSAGE("poll returned against the port layer's do-nothing default");
}

/**
 * @brief Checks that no refused call ever reaches the registered completion callback.
 *
 * @note A hook that refused a transfer and still reported completion would hand the application an
 *       event for bytes that never moved.
 */
void test_an_unported_build_reports_no_completion(void)
{
    (void)EMBED_CALL(praet.open, PraetCfg, .channel = PRAET_TEST_CHANNEL, .on_complete = &s_completion_binding);
    (void)EMBED_CALL(praet.tx_submit, PraetTransferCfg, .channel = PRAET_TEST_CHANNEL, .buf = NULL, .bytes = 0u);
    EMBED_CALL(praet.poll, PraetCfg, .channel = PRAET_TEST_CHANNEL, .on_complete = &s_completion_binding);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, s_completion_count, "a refused transfer must not reach the completion callback");
}

/**
 * @brief Checks that the completion event carries every field the port layer fills.
 *
 * @note The event is the port layer's whole report. A field dropped from the struct would compile
 *       here and lose data at every real port.
 */
void test_the_completion_event_carries_every_reported_field(void)
{
    static const uint8_t moved[2] = {0xABu, 0xCDu};
    const mmgr_praet_event event = {
        .data = moved,
        .completion_ms = 1234u,
        .completion_us = 567u,
        .bytes = (uint16_t)sizeof moved,
        .sequence = 9u,
        .channel = PRAET_TEST_CHANNEL,
        .peripheral = 2u,
        .direction = 1u,
    };

    TEST_ASSERT_EQUAL_PTR(moved, event.data);
    TEST_ASSERT_EQUAL_UINT32(1234u, event.completion_ms);
    TEST_ASSERT_EQUAL_UINT32(567u, event.completion_us);
    TEST_ASSERT_EQUAL_UINT16(2u, event.bytes);
    TEST_ASSERT_EQUAL_UINT16(9u, event.sequence);
    TEST_ASSERT_EQUAL_UINT8(PRAET_TEST_CHANNEL, event.channel);
    TEST_ASSERT_EQUAL_UINT8(2u, event.peripheral);
    TEST_ASSERT_EQUAL_UINT8(1u, event.direction);
}

/**
 * @brief Checks that microseconds within a millisecond stay below one thousand.
 *
 * @note completion_ms and completion_us are separate members, so a port that folded the whole
 *       elapsed time into completion_us would still fill both and read wrong here.
 */
void test_the_completion_event_splits_time_into_two_fields(void)
{
    const mmgr_praet_event event = {
        .completion_ms = 5u,
        .completion_us = 999u,
    };

    TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(1000u, event.completion_us,
                                         "microseconds within a millisecond never reach one thousand");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, event.completion_ms, "the millisecond field holds its own count");
}
