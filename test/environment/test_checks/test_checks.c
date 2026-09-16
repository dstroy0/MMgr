#include "unity.h"

#include "mmgr.h"

void test_checks_widths_are_what_was_asked_for(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, MMGR_DEBUG_CHECKS, "MMGR_DEBUG_CHECKS did not reach the translation unit");

    TEST_ASSERT_TRUE_MESSAGE(EMBED_WORD_BITS == 16 || EMBED_WORD_BITS == 32 || EMBED_WORD_BITS == 64,
                             "EMBED_WORD_BITS is not a supported width");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_INDEX_BITS == 16 || EMBED_INDEX_BITS == 32,
                             "EMBED_INDEX_BITS is not a supported width");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_INDEX_BITS <= EMBED_WORD_BITS,
                             "an index wider than the register cannot be carried in one");
}

void test_checks_types_match_the_widths(void)
{
    TEST_ASSERT_EQUAL_size_t(EMBED_WORD_BITS / 8u, sizeof(embed_word));
    TEST_ASSERT_EQUAL_size_t(EMBED_INDEX_BITS / 8u, sizeof(embed_index));
    TEST_ASSERT_EQUAL_size_t(1u, sizeof(EmbedEnumProbe));
}
