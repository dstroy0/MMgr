// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "memoria_externa/memoria_externa.h"

#define ROOMY_THRESHOLD 10000u

void setUp(void)
{
}

void tearDown(void)
{
}

void test_exter_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("memoria_externa.h compiled with no header before it");
}

void test_place_refuses_a_size_of_zero(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 0u, .free_dram = 4096u,
                                            .free_psram = 4096u, .psram_threshold = ROOMY_THRESHOLD),
                                  "a size of zero is refused before either memory is consulted");
}

void test_a_dma_request_reaches_internal_memory(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 64u, .dma_required = MMGR_TRUE,
                                            .free_dram = 4096u, .free_psram = 4096u,
                                            .psram_threshold = ROOMY_THRESHOLD),
                                  "a DMA request that fits internal memory belongs there");
}

void test_a_dma_request_fails_rather_than_falling_back_to_external(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 4096u, .dma_required = MMGR_TRUE,
                                            .free_dram = 64u, .free_psram = 1048576u,
                                            .psram_threshold = ROOMY_THRESHOLD),
                                  "external memory is not reachable by DMA, so a fallback would hand back a bad address");
}

void test_a_dma_request_ignores_the_threshold(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 4096u, .dma_required = MMGR_TRUE,
                                            .free_dram = 8192u, .free_psram = 1048576u, .psram_threshold = 1024u),
                                  "the DMA requirement is answered before the threshold is read");
}

void test_at_or_above_the_threshold_external_is_tried_first(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_PSRAM,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 1024u, .free_dram = 1048576u,
                                            .free_psram = 1048576u, .psram_threshold = 1024u),
                                  "a size equal to the threshold is at it, so external memory is preferred");
}

void test_below_the_threshold_internal_is_tried_first(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 1023u, .free_dram = 1048576u,
                                            .free_psram = 1048576u, .psram_threshold = 1024u),
                                  "one byte below the threshold is below it, so internal memory is preferred");
}

void test_a_large_request_falls_back_to_internal_when_external_is_full(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 2048u, .free_dram = 1048576u,
                                            .free_psram = 1024u, .psram_threshold = 1024u),
                                  "external memory is tried first above the threshold and internal is tried second");
}

void test_a_small_request_falls_back_to_external_when_internal_is_full(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_PSRAM,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 512u, .free_dram = 64u,
                                            .free_psram = 1048576u, .psram_threshold = ROOMY_THRESHOLD),
                                  "internal memory is tried first below the threshold and external is tried second");
}

void test_a_large_request_that_fits_neither_fails(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 4096u, .free_dram = 64u,
                                            .free_psram = 128u, .psram_threshold = 1024u),
                                  "both memories were tried above the threshold and neither took it");
}

void test_a_small_request_that_fits_neither_fails(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 512u, .free_dram = 64u,
                                            .free_psram = 128u, .psram_threshold = ROOMY_THRESHOLD),
                                  "both memories were tried below the threshold and neither took it");
}

void test_internal_placement_leaves_the_reserve_free(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_DRAM,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 800u, .free_dram = 1000u,
                                            .free_psram = 0u, .psram_threshold = ROOMY_THRESHOLD,
                                            .dram_reserve = 200u),
                                  "a request that leaves exactly the reserve free still fits");

    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 801u, .free_dram = 1000u,
                                            .free_psram = 0u, .psram_threshold = ROOMY_THRESHOLD,
                                            .dram_reserve = 200u),
                                  "one byte into the reserve is one byte too many");
}

void test_the_reserve_does_not_bound_external_placement(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_PSRAM,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 801u, .free_dram = 1000u,
                                            .free_psram = 1000u, .psram_threshold = ROOMY_THRESHOLD,
                                            .dram_reserve = 200u),
                                  "the external test is a size comparison alone, so the reserve never reaches it");
}

void test_a_size_past_free_internal_memory_does_not_wrap(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 2000u, .free_dram = 1000u,
                                            .free_psram = 0u, .psram_threshold = ROOMY_THRESHOLD),
                                  "a size above free internal memory would wrap the reserve subtraction if it ran");
}

void test_a_reserve_above_free_internal_memory_refuses_every_request(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(PLACE_FAIL,
                                  MMGR_CALL(exter.place, ExternaCfg, .size = 1u, .free_dram = 100u, .free_psram = 0u,
                                            .psram_threshold = ROOMY_THRESHOLD, .dram_reserve = 200u),
                                  "a reserve larger than what is free leaves nothing placeable");
}

void test_init_points_the_pair_at_the_first_buffer(void)
{
    PingPong pair;

    pair.fill_index = 1u;
    MMGR_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, MMGR_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair),
                                    "a reset pair fills buffer 0");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, MMGR_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair),
                                    "a reset pair drains buffer 1");
}

void test_the_two_indexes_never_agree(void)
{
    PingPong pair;

    MMGR_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);

    for (int flip = 0; flip < 4; flip++)
    {
        const uint8_t fill = MMGR_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair);
        const uint8_t drain = MMGR_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair);

        TEST_ASSERT_NOT_EQUAL_MESSAGE(fill, drain, "one buffer cannot be filled and drained at once");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, (uint8_t)(fill ^ drain), "the drain index is the fill index low bit flipped");
        MMGR_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair);
    }
}

void test_reading_an_index_does_not_move_it(void)
{
    PingPong pair;

    MMGR_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);
    (void)MMGR_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair);
    (void)MMGR_CALL(exter.pingpong_drain, ExternaCfg, .pingpong = &pair);
    (void)MMGR_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, MMGR_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair),
                                    "reading the pair three times left it where it started");
}

void test_swap_hands_back_the_index_it_moved_to(void)
{
    PingPong pair;

    MMGR_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);

    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, MMGR_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair),
                                    "the first swap moves the fill to buffer 1 and says so");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, MMGR_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair),
                                    "what swap returned is what the pair now holds");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, MMGR_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair),
                                    "the second swap moves it back to buffer 0 and says so");
}

void test_a_swap_of_a_swap_is_where_it_started(void)
{
    PingPong pair;

    MMGR_CALL(exter.pingpong_init, ExternaCfg, .pingpong = &pair);

    for (int round = 0; round < 8; round++)
    {
        MMGR_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair);
        MMGR_CALL(exter.pingpong_swap, ExternaCfg, .pingpong = &pair);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, MMGR_CALL(exter.pingpong_fill, ExternaCfg, .pingpong = &pair),
                                        "an even number of flips is no flip at all");
    }
}
