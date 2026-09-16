#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

#include "unity.h"

static int ref_num(int c)
{
    return c >= '0' && c <= '9';
}
static int ref_upper(int c)
{
    return c >= 'A' && c <= 'Z';
}
static int ref_lower(int c)
{
    return c >= 'a' && c <= 'z';
}
static int ref_alpha(int c)
{
    return ref_upper(c) || ref_lower(c);
}
static int ref_alnum(int c)
{
    return ref_alpha(c) || ref_num(c);
}
static int ref_hex(int c)
{
    return ref_num(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
static int ref_punct(int c)
{
    return c >= 0x21 && c <= 0x7E && !ref_alnum(c);
}
static int ref_space(int c)
{
    return c == ' ' || (c >= 9 && c <= 13);
}
static int ref_print(int c)
{
    return c >= 0x20 && c <= 0x7E;
}
static int ref_ctrl(int c)
{
    return c < 0x20 || c == 0x7F;
}

static void all_256(MmgrAsciiClass k, int (*ref)(int), const char *what)
{
    for (int c = 0; c < 256; c++)
    {
        const int want = (c < 128) ? ref(c) : 0;
        const int got = EMBED_CALL(ascii.in, AsciiCfg, .kind = k, .byte = (uint8_t)c);
        if (got != want)
        {
            TEST_ASSERT_EQUAL_INT_MESSAGE(want, got, what);
        }
    }
}

void test_ascii_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("ascii_persona_bitorum.h compiled with no header before it");
}

void test_ascii_persona_bitorum_is_128_bits(void)
{
    TEST_ASSERT_EQUAL_size_t(16u, sizeof(MmgrAsciiMask));
}

void test_ascii_class_numbering_is_pinned(void)
{
    TEST_ASSERT_EQUAL_INT(0, MMGR_ASCII_NUM);
    TEST_ASSERT_EQUAL_INT(1, MMGR_ASCII_ALPHA);
    TEST_ASSERT_EQUAL_INT(2, MMGR_ASCII_ALNUM);
    TEST_ASSERT_EQUAL_INT(3, MMGR_ASCII_UPPER);
    TEST_ASSERT_EQUAL_INT(4, MMGR_ASCII_LOWER);
    TEST_ASSERT_EQUAL_INT(5, MMGR_ASCII_HEX);
    TEST_ASSERT_EQUAL_INT(6, MMGR_ASCII_PUNCT);
    TEST_ASSERT_EQUAL_INT(7, MMGR_ASCII_SPACE);
    TEST_ASSERT_EQUAL_INT(8, MMGR_ASCII_CTRL);
    TEST_ASSERT_EQUAL_INT(9, MMGR_ASCII_PRINT);
    TEST_ASSERT_EQUAL_INT(10, MMGR_ASCII_CLASSES);
}

void test_ascii_num(void)
{
    all_256(MMGR_ASCII_NUM, ref_num, "num");
}
void test_ascii_alpha(void)
{
    all_256(MMGR_ASCII_ALPHA, ref_alpha, "alpha");
}
void test_ascii_alnum(void)
{
    all_256(MMGR_ASCII_ALNUM, ref_alnum, "alnum");
}
void test_ascii_upper(void)
{
    all_256(MMGR_ASCII_UPPER, ref_upper, "upper");
}
void test_ascii_lower(void)
{
    all_256(MMGR_ASCII_LOWER, ref_lower, "lower");
}
void test_ascii_hex(void)
{
    all_256(MMGR_ASCII_HEX, ref_hex, "hex");
}
void test_ascii_punct(void)
{
    all_256(MMGR_ASCII_PUNCT, ref_punct, "punct");
}
void test_ascii_space(void)
{
    all_256(MMGR_ASCII_SPACE, ref_space, "space");
}
void test_ascii_print(void)
{
    all_256(MMGR_ASCII_PRINT, ref_print, "print");
}
void test_ascii_ctrl(void)
{
    all_256(MMGR_ASCII_CTRL, ref_ctrl, "ctrl");
}

void test_ascii_high_bytes_are_in_no_class(void)
{
    for (int c = 128; c < 256; c++)
    {
        TEST_ASSERT_FALSE(EMBED_CALL(ascii.in, AsciiCfg, .kind = MMGR_ASCII_PRINT, .byte = (uint8_t)c));
        TEST_ASSERT_FALSE(EMBED_CALL(ascii.in, AsciiCfg, .kind = MMGR_ASCII_CTRL, .byte = (uint8_t)c));
        TEST_ASSERT_FALSE(EMBED_CALL(ascii.in, AsciiCfg, .kind = MMGR_ASCII_ALNUM, .byte = (uint8_t)c));
    }
}

void test_ascii_classes_partition_the_printables(void)
{
    for (int c = 0x20; c <= 0x7E; c++)
    {
        const int n = EMBED_CALL(ascii.in, AsciiCfg, .kind = MMGR_ASCII_ALNUM, .byte = (uint8_t)c) +
                      EMBED_CALL(ascii.in, AsciiCfg, .kind = MMGR_ASCII_PUNCT, .byte = (uint8_t)c) +
                      EMBED_CALL(ascii.in, AsciiCfg, .kind = MMGR_ASCII_SPACE, .byte = (uint8_t)c);
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, n, "a printable byte belongs to exactly one of alnum, punct, space");
    }
}
