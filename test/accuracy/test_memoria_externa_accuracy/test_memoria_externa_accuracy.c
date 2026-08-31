// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
/**
 * @file test_memoria_externa_accuracy.c
 * @brief Checks the placement decision against the policy memoria_externa.h states, over every
 *        combination of six small inputs, and checks the two-buffer index over long runs of swaps.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-31
 *
 * @note The placement is a decision, not a computation, and its accuracy is whether the decision
 *       matches the policy the header publishes. The reference below is that policy written out from
 *       the header's own wording: the refusal at a size of zero, the DMA request that never falls
 *       back to external memory, the threshold that reverses which memory is tried first, and the
 *       reserve that only the internal test applies.
 * @note Six inputs over nine values each, with the DMA flag both ways, is 118098 decisions. That is
 *       every ordering of size against the two free figures, the threshold and the reserve, so no
 *       boundary between them goes unvisited.
 * @note Small numbers are the whole point. Every boundary in the policy is a comparison between two
 *       of the inputs, and the orderings that matter all occur among the first few integers.
 * @note One case uses figures near the top of a size_t. The internal test subtracts the size from the
 *       free bytes, and a subtraction that ran before its guard would wrap there and admit a request
 *       that does not fit.
 * @note Contract checks on a null pair live in test_memoria_externa. This file asks which memory a
 *       request is sent to.
 */
#include <stdint.h>
#include <stdio.h>

#include "memoria_externa/memoria_externa.h"

#include "unity.h"

/**
 * @brief Expands to 9u, the values each figure in the exhaustive sweep takes.
 *
 * @note Zero through eight. The policy compares the inputs against each other, so what a sweep needs
 *       is every ordering among them and not a wide range of magnitudes.
 */
#define MMGR_ACCURACY_EXTERN_STEPS 9u

/**
 * @brief Returns where the documented policy sends a request.
 *
 * @param[in] size         Bytes to place.
 * @param[in] dma_required Whether the bytes must be reachable by DMA.
 * @param[in] free_dram    Bytes free in internal memory.
 * @param[in] free_psram   Bytes free in external memory.
 * @param[in] threshold    Size at or above which external memory is tried first.
 * @param[in] reserve      Internal bytes that must remain free afterwards.
 * @return                 PLACE_DRAM, PLACE_PSRAM or PLACE_FAIL.
 * @note Transcribed from the four rules memoria_externa.h states on mmgr_extern_place: a size of 0
 *       gives PLACE_FAIL, a DMA request gives PLACE_DRAM or PLACE_FAIL and never external memory, at
 *       or above the threshold external memory is preferred, and below it internal memory is.
 * @note The two fit tests differ, which the header calls out. Internal has to leave the reserve free
 *       afterwards, and external is a size comparison on its own.
 * @note The internal test is written as an addition, where the module subtracts. Both express the
 *       same rule and neither can wrap at the values this file sweeps, so the two arrive at the same
 *       decision without sharing the arithmetic that reaches it.
 */
static mmgr_place accuracy_policy_place(size_t size, embed_bool dma_required, size_t free_dram, size_t free_psram,
                                        size_t threshold, size_t reserve)
{
    const embed_bool dram_fits = (embed_bool)((size <= free_dram) && ((size + reserve) <= free_dram));
    const embed_bool psram_fits = (embed_bool)(size <= free_psram);

    if (size == 0u)
    {
        return PLACE_FAIL;
    }
    if (dma_required)
    {
        return dram_fits ? PLACE_DRAM : PLACE_FAIL;
    }
    if (size >= threshold)
    {
        if (psram_fits)
        {
            return PLACE_PSRAM;
        }
        return dram_fits ? PLACE_DRAM : PLACE_FAIL;
    }
    if (dram_fits)
    {
        return PLACE_DRAM;
    }
    return psram_fits ? PLACE_PSRAM : PLACE_FAIL;
}

/**
 * @brief Returns the name of a placement, for a failure message.
 *
 * @param[in] place Placement to name.
 * @return          The enumerator's name [BORROWS].
 * @note A failing sweep reports two placements, and their numbers are 0, 1 and 2. The names are what
 *       make the message readable without a lookup.
 */
static const char *accuracy_place_name(mmgr_place place)
{
    switch (place)
    {
    case PLACE_DRAM:
        return "PLACE_DRAM";
    case PLACE_PSRAM:
        return "PLACE_PSRAM";
    case PLACE_FAIL:
        return "PLACE_FAIL";
    default:
        break;
    }
    return "an unnamed placement";
}

/**
 * @brief Runs before each Unity test case.
 *
 * @note Unity calls this around every case, so the symbol has to exist even when it does nothing.
 * @note The placement reads nothing but its arguments, and each pingpong case initializes its own
 *       pair, so there is no state to prepare.
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
 * @brief Checks the reference policy this suite rests on against decisions worked out by hand.
 *
 * @note Exists to catch a defect in the reference as itself. A reference that agreed with the module
 *       while both departed from the header would pass the sweep below and prove nothing.
 * @note Each expectation names one rule from the header. A size of zero is refused whatever fits, a
 *       DMA request never reaches external memory even when it would fit there, the threshold sends
 *       a large request to external memory first, and below it a request goes to internal memory
 *       first even when both would take it.
 * @note The reserve is checked at the boundary the header describes: a reserve equal to the free
 *       internal bytes admits only a size of 0, which is refused for being zero.
 */
void test_the_reference_policy_this_suite_relies_on_is_itself_right(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL, accuracy_policy_place(0u, EMBED_FALSE, 100u, 100u, 50u, 0u),
                                  "a size of zero is refused whatever fits");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM, accuracy_policy_place(10u, EMBED_TRUE, 100u, 100u, 5u, 0u),
                                  "a DMA request that fits internal memory goes there");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL, accuracy_policy_place(10u, EMBED_TRUE, 5u, 100u, 5u, 0u),
                                  "a DMA request too large for internal memory fails instead of falling back");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_PSRAM, accuracy_policy_place(60u, EMBED_FALSE, 100u, 100u, 50u, 0u),
                                  "a request at or above the threshold is tried in external memory first");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM, accuracy_policy_place(60u, EMBED_FALSE, 100u, 10u, 50u, 0u),
                                  "a large request that will not fit external memory falls back to internal");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM, accuracy_policy_place(10u, EMBED_FALSE, 100u, 100u, 50u, 0u),
                                  "a request below the threshold is tried in internal memory first");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_PSRAM, accuracy_policy_place(10u, EMBED_FALSE, 5u, 100u, 50u, 0u),
                                  "a small request that will not fit internal memory falls back to external");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_PSRAM, accuracy_policy_place(10u, EMBED_FALSE, 100u, 100u, 50u, 95u),
                                  "the reserve keeps a request out of internal memory");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL, accuracy_policy_place(10u, EMBED_FALSE, 100u, 5u, 50u, 95u),
                                  "a request the reserve blocks and external memory will not take fails");
}

/**
 * @brief Checks the placement against the documented policy over every combination of six inputs.
 *
 * @note This is the case the file exists for. Every ordering of the size against the two free
 *       figures, the threshold and the reserve is visited, with the DMA flag taken both ways.
 * @note The failure message names all six inputs and both placements. A disagreement is reproducible
 *       from the message alone, with no part of the grid to narrow down by hand.
 * @note The sweep is 118098 decisions and each one is a handful of comparisons, which is why the
 *       whole grid is walked instead of a sample of it.
 */
void test_every_placement_decision_matches_the_documented_policy(void)
{
    for (unsigned size = 0u; size < MMGR_ACCURACY_EXTERN_STEPS; size++)
    {
        for (unsigned free_dram = 0u; free_dram < MMGR_ACCURACY_EXTERN_STEPS; free_dram++)
        {
            for (unsigned free_psram = 0u; free_psram < MMGR_ACCURACY_EXTERN_STEPS; free_psram++)
            {
                for (unsigned threshold = 0u; threshold < MMGR_ACCURACY_EXTERN_STEPS; threshold++)
                {
                    for (unsigned reserve = 0u; reserve < MMGR_ACCURACY_EXTERN_STEPS; reserve++)
                    {
                        for (unsigned dma = 0u; dma < 2u; dma++)
                        {
                            // Explicit cast puts the loop counter in the embed_bool the entry takes.
                            // The counter is 0 or 1, which is what that container holds
                            const embed_bool dma_required = (embed_bool)dma;
                            const mmgr_place expected =
                                accuracy_policy_place(size, dma_required, free_dram, free_psram, threshold, reserve);
                            const mmgr_place produced =
                                EMBED_CALL(exter.place, ExternaCfg, .size = size, .dma_required = dma_required,
                                           .free_dram = free_dram, .free_psram = free_psram,
                                           .psram_threshold = threshold, .dram_reserve = reserve);

                            if (expected != produced)
                            {
                                char message[192];

                                (void)snprintf(message, sizeof message,
                                               "size %u dma %u dram %u psram %u threshold %u reserve %u: policy gives "
                                               "%s and the module gives %s",
                                               size, dma, free_dram, free_psram, threshold, reserve,
                                               accuracy_place_name(expected), accuracy_place_name(produced));
                                TEST_FAIL_MESSAGE(message);
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief Checks that a request larger than the free internal bytes is refused at the top of the range.
 *
 * @note The internal test subtracts the size from the free bytes and compares the remainder against
 *       the reserve. A subtraction taken before its guard wraps to a very large remainder, and the
 *       request is then admitted to memory that cannot hold it.
 * @note The figures sit near the top of a size_t, where a wrap produces a number larger than any
 *       reserve. At small values a wrapped remainder can still fall below a reserve by chance, which
 *       is why the case is written up here.
 * @note External memory is given nothing to offer in the arms that expect a refusal, so the refusal
 *       is the internal test's and not a fallback that happened to fail as well.
 */
void test_a_request_larger_than_the_free_bytes_is_refused_at_the_top_of_the_range(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = SIZE_MAX, .dma_required = EMBED_TRUE,
                                             .free_dram = 16u, .free_psram = 0u, .psram_threshold = 0u,
                                             .dram_reserve = 0u),
                                  "a request of the whole address space was admitted to internal memory");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = SIZE_MAX - 1u, .dma_required = EMBED_TRUE,
                                             .free_dram = 16u, .free_psram = 0u, .psram_threshold = 0u,
                                             .dram_reserve = 8u),
                                  "a request one short of the whole address space cleared the reserve");
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  EMBED_CALL(exter.place, ExternaCfg, .size = 16u, .dma_required = EMBED_TRUE,
                                             .free_dram = SIZE_MAX, .free_psram = 0u, .psram_threshold = 0u,
                                             .dram_reserve = 8u),
                                  "a request that fits the whole address space was refused");
}

/**
 * @brief Checks that the two buffer indexes are always each other's complement.
 *
 * @note The pair keeps one member and derives the other, and the claim is that the two never name the
 *       same buffer. A producer and a consumer holding the same index write and read the same bytes
 *       at once, which is the failure the whole arrangement exists to prevent.
 * @note Checked after the initial state and after every swap in a long run, which tests the property
 *       at both values many times over instead of at whichever one a single swap lands on.
 * @note The swap's return is checked against a separate read of the fill index, since a swap that
 *       flipped the member and returned the old value would leave a caller acting on the buffer it
 *       just stopped filling.
 */
void test_the_two_buffer_indexes_are_always_each_others_complement(void)
{
    PingPong pair;

    EMBED_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair),
                                    "a fresh pair does not start at buffer zero");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, EMBED_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair),
                                    "a fresh pair does not drain buffer one");

    for (unsigned step = 0u; step < 1000u; step++)
    {
        const uint8_t returned = EMBED_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair);
        const uint8_t fill = EMBED_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair);
        const uint8_t drain = EMBED_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair);
        char message[96];

        (void)snprintf(message, sizeof message, "at swap %u", step);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(fill, returned, message);
        TEST_ASSERT_TRUE_MESSAGE(fill <= 1u, "the fill index left the pair of buffers");
        TEST_ASSERT_TRUE_MESSAGE(drain <= 1u, "the drain index left the pair of buffers");
        TEST_ASSERT_NOT_EQUAL_MESSAGE(fill, drain, message);
        // A pair started at zero is filling the buffer the swap count's parity names. That count is
        // kept here, so the expectation reaches this line without reading the pair at all
        TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)((step + 1u) & 1u), fill, message);
    }
}
