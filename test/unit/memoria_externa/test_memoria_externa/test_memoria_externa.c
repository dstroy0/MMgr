// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_memoria_externa.c
 * @brief Exercises exter.place across the DMA requirement, the size threshold, the free-space
 *        fallbacks and the internal reserve, and the ping-pong pair across init, read and swap.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note Built only where MMGR_ENABLE_EXTRAM is on, which the CAPABILITY EXTRAM line in this
 *       directory's CMakeLists sets. A default configure leaves this suite out of the build.
 * @note Every placement case supplies free_dram and free_psram itself, so the policy is driven at
 *       sizes no board has to have.
 */
#include "memoria_externa/memoria_externa.h"

#include "unity.h"

/**
 * @brief Expands to 10000, a threshold no size in this suite reaches.
 *
 * @note Cases pass this where they want the below-threshold path, in which internal memory is tried
 *       first. The largest size any case requests is 4096, so nothing here meets it.
 */
#define ROOMY_THRESHOLD 10000u

/**
 * @brief Runs before each Unity test case.
 *
 * @note Empty because each case builds the state it needs. A placement case passes its free-space
 *       figures inline, and a ping-pong case declares its own pair with automatic storage.
 */
void setUp(void)
{
}

/**
 * @brief Runs after each Unity test case.
 *
 * @note Empty because exter.place answers with a placement decision rather than an allocation, so a
 *       case that called it holds nothing to hand back.
 */
void tearDown(void)
{
}

/**
 * @brief Checks that memoria_externa.h compiles with no header ahead of it.
 *
 * @note The include order above is the whole test. memoria_externa.h is listed before unity.h, so
 *       anything the header needs and does not include itself fails to compile here.
 */
void test_exter_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("memoria_externa.h compiled with no header before it");
}

/**
 * @brief Checks that a size of zero is refused whatever free space is offered.
 *
 * @note Both memories are given 4096 bytes free, so the refusal cannot come from a shortage. A
 *       policy testing size against free space before testing it against zero would place it.
 */
void test_place_refuses_a_size_of_zero(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 0u, .free_dram = 4096u,
                                             .free_psram = 4096u, .psram_threshold = ROOMY_THRESHOLD),
                                  "a size of zero is refused before either memory is consulted");
}

/**
 * @brief Checks that a DMA request fitting internal memory is placed there.
 *
 * @note Both memories are given the same free space, so the answer turns on the DMA requirement
 *       alone rather than on which one has more room.
 */
void test_a_dma_request_reaches_internal_memory(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 64u, .dma_required = EMBED_TRUE,
                                             .free_dram = 4096u, .free_psram = 4096u,
                                             .psram_threshold = ROOMY_THRESHOLD),
                                  "a DMA request that fits internal memory belongs there");
}

/**
 * @brief Checks that a DMA request too large for internal memory fails instead of moving outward.
 *
 * @note External memory is given a megabyte free against 64 bytes internal, so a policy that fell
 *       back would have somewhere to fall. The DMA engine cannot reach external memory, so the
 *       address such a fallback returned would be one the transfer could not use.
 */
void test_a_dma_request_fails_rather_than_falling_back_to_external(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        PLACE_FAIL,
        EMBED_CALL(exter.place, ExternaCfg, .size = 4096u, .dma_required = EMBED_TRUE, .free_dram = 64u,
                   .free_psram = 1048576u, .psram_threshold = ROOMY_THRESHOLD),
        "external memory is not reachable by DMA, so a fallback would hand back a bad address");
}

/**
 * @brief Checks that the DMA requirement is answered before the size threshold is consulted.
 *
 * @note The size is four times the threshold, which on its own would send the request outward, and
 *       internal memory has room for it. A policy reading the threshold first would place it
 *       externally and leave the transfer pointed at memory the engine cannot reach.
 */
void test_a_dma_request_ignores_the_threshold(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 4096u, .dma_required = EMBED_TRUE,
                                             .free_dram = 8192u, .free_psram = 1048576u, .psram_threshold = 1024u),
                                  "the DMA requirement is answered before the threshold is read");
}

/**
 * @brief Checks that a size equal to the threshold counts as at it, so external memory is tried
 *        first.
 *
 * @note Both memories are given a megabyte free, so either could take the request and the answer
 *       comes from the policy rather than from available room.
 */
void test_at_or_above_the_threshold_external_is_tried_first(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_PSRAM,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 1024u, .free_dram = 1048576u,
                                             .free_psram = 1048576u, .psram_threshold = 1024u),
                                  "a size equal to the threshold is at it, so external memory is preferred");
}

/**
 * @brief Checks that one byte under the threshold counts as below it, so internal memory is tried
 *        first.
 *
 * @note Sits one byte from test_at_or_above_the_threshold_external_is_tried_first. The two pin which
 *       side of the comparison the boundary falls on, and a policy written with the wrong
 *       strictness passes one of them and fails the other.
 */
void test_below_the_threshold_internal_is_tried_first(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 1023u, .free_dram = 1048576u,
                                             .free_psram = 1048576u, .psram_threshold = 1024u),
                                  "one byte below the threshold is below it, so internal memory is preferred");
}

/**
 * @brief Checks that a request above the threshold falls back to internal memory when external is
 *        too full to take it.
 *
 * @note External memory holds exactly half what the request needs, so the first choice refuses and
 *       the second is reached. Without the fallback the request would fail with a megabyte free.
 */
void test_a_large_request_falls_back_to_internal_when_external_is_full(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 2048u, .free_dram = 1048576u,
                                             .free_psram = 1024u, .psram_threshold = 1024u),
                                  "external memory is tried first above the threshold and internal is tried second");
}

/**
 * @brief Checks that a request below the threshold falls back to external memory when internal is
 *        too full to take it.
 *
 * @note The mirror of the fallback above, entered from the other side of the threshold, so both
 *       orderings are covered rather than only the one.
 */
void test_a_small_request_falls_back_to_external_when_internal_is_full(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_PSRAM,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 512u, .free_dram = 64u,
                                             .free_psram = 1048576u, .psram_threshold = ROOMY_THRESHOLD),
                                  "internal memory is tried first below the threshold and external is tried second");
}

/**
 * @brief Checks that a request above the threshold fails once both memories have refused it.
 *
 * @note Neither memory has room, so both branches of the above-threshold path are walked and the
 *       failure comes from the end of that walk rather than from an early refusal.
 */
void test_a_large_request_that_fits_neither_fails(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 4096u, .free_dram = 64u,
                                             .free_psram = 128u, .psram_threshold = 1024u),
                                  "both memories were tried above the threshold and neither took it");
}

/**
 * @brief Checks that a request below the threshold fails once both memories have refused it.
 *
 * @note Walks the below-threshold path to its end, which is the ordering the case above never
 *       reaches. Both paths have to fail closed, not only the one tried first.
 */
void test_a_small_request_that_fits_neither_fails(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 512u, .free_dram = 64u,
                                             .free_psram = 128u, .psram_threshold = ROOMY_THRESHOLD),
                                  "both memories were tried below the threshold and neither took it");
}

/**
 * @brief Checks that the internal reserve is held back, at the exact byte where a request begins to
 *        eat into it.
 *
 * @note The two sizes sit one byte apart at 800 and 801 against 1000 free with 200 reserved, so the
 *       first leaves the reserve exactly whole and the second takes one byte of it.
 * @note External memory is given nothing free in both, so neither answer can come from a fallback.
 */
void test_internal_placement_leaves_the_reserve_free(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 800u, .free_dram = 1000u,
                                             .free_psram = 0u, .psram_threshold = ROOMY_THRESHOLD,
                                             .dram_reserve = 200u),
                                  "a request that leaves exactly the reserve free still fits");

    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 801u, .free_dram = 1000u,
                                             .free_psram = 0u, .psram_threshold = ROOMY_THRESHOLD,
                                             .dram_reserve = 200u),
                                  "one byte into the reserve is one byte too many");
}

/**
 * @brief Checks that the internal reserve does not bound a placement that lands externally.
 *
 * @note The same 801 bytes the case above refused internally, now with external memory holding 1000
 *       free. The external test is a size comparison alone, so the reserve never enters it.
 */
void test_the_reserve_does_not_bound_external_placement(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_PSRAM,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 801u, .free_dram = 1000u,
                                             .free_psram = 1000u, .psram_threshold = ROOMY_THRESHOLD,
                                             .dram_reserve = 200u),
                                  "the external test is a size comparison alone, so the reserve never reaches it");
}

/**
 * @brief Checks that a size past free internal memory is refused rather than wrapping the
 *        subtraction that bounds it.
 *
 * @note Asks 2000 bytes of 1000 free. A policy subtracting the size from free space in unsigned
 *       arithmetic would wrap to an enormous remainder and place the request.
 */
void test_a_size_past_free_internal_memory_does_not_wrap(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 2000u, .free_dram = 1000u,
                                             .free_psram = 0u, .psram_threshold = ROOMY_THRESHOLD),
                                  "a size above free internal memory would wrap the reserve subtraction if it ran");
}

/**
 * @brief Checks that a reserve larger than free internal memory refuses even a one-byte request.
 *
 * @note Reserves 200 bytes of 100 free. A policy subtracting the reserve from free space in
 *       unsigned arithmetic would wrap and report far more room than the part has.
 */
void test_a_reserve_above_free_internal_memory_refuses_every_request(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 1u, .free_dram = 100u, .free_psram = 0u,
                                             .psram_threshold = ROOMY_THRESHOLD, .dram_reserve = 200u),
                                  "a reserve larger than what is free leaves nothing placeable");
}

/**
 * @brief Checks that init points the pair at buffer 0 to fill and buffer 1 to drain.
 *
 * @note fill_index is set to 1 before init runs, so an init that left the member alone would read
 *       back 1 and fail here. A pair with automatic storage holds an indeterminate index otherwise,
 *       which would let that defect pass whenever the stack happened to hold zero.
 */
void test_init_points_the_pair_at_the_first_buffer(void)
{
    PingPong pair;

    pair.fill_index = 1u;
    EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair),
                                    "a reset pair fills buffer 0");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, EMBED_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair),
                                    "a reset pair drains buffer 1");
}

/**
 * @brief Checks across four swaps that the fill and drain indexes never name one buffer.
 *
 * @note Filling and draining the same buffer at once would read bytes still being written.
 * @note The exclusive-or assertion is the stronger of the two here. A difference of exactly one bit
 *       both proves the indexes differ and pins them to the pair 0 and 1.
 */
void test_the_two_indexes_never_agree(void)
{
    PingPong pair;

    EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);

    for (int flip = 0; flip < 4; flip++)
    {
        const uint8_t fill = EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair);
        const uint8_t drain = EMBED_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair);

        TEST_ASSERT_NOT_EQUAL_MESSAGE(fill, drain, "one buffer cannot be filled and drained at once");
        // Explicit cast narrows the promoted exclusive-or back to the width the assertion compares.
        // Both operands are uint8_t and the result is 0 or 1, so the narrowing drops nothing
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, (uint8_t)(fill ^ drain),
                                        "the drain index is the fill index low bit flipped");
        EMBED_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair);
    }
}

/**
 * @brief Checks that reading an index leaves the pair where it was.
 *
 * @note Reads the pair three times and then expects the index init placed. An accessor advancing
 *       the pair as a side effect of being read would move it under a caller that only wanted to
 *       know which buffer to write.
 */
void test_reading_an_index_does_not_move_it(void)
{
    PingPong pair;

    EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);
    (void)EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair);
    (void)EMBED_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair);
    (void)EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair),
                                    "reading the pair three times left it where it started");
}

/**
 * @brief Checks that swap returns the fill index it just moved to.
 *
 * @note What swap returned is read back against the pair itself, so a swap that moved the pair and
 *       returned the old index fails on the second assertion rather than passing unnoticed.
 */
void test_swap_hands_back_the_index_it_moved_to(void)
{
    PingPong pair;

    EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, EMBED_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair),
                                    "the first swap moves the fill to buffer 1 and says so");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair),
                                    "what swap returned is what the pair now holds");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, EMBED_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair),
                                    "the second swap moves it back to buffer 0 and says so");
}

/**
 * @brief Checks that two swaps in a row leave the pair where it began.
 *
 * @note Repeats eight times, so a pair drifting by one buffer per round is caught. A single pair of
 *       swaps would pass even where the state walked away over many.
 */
void test_a_swap_of_a_swap_is_where_it_started(void)
{
    PingPong pair;

    EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);

    for (int round = 0; round < 8; round++)
    {
        EMBED_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair);
        EMBED_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair),
                                        "an even number of flips is no flip at all");
    }
}
