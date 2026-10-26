#include "verba_scribo/verba_linea.h"

#include "unity.h"

#include <stdint.h>

static char s_out[300];
static char s_handed[512];
static size_t s_handed_size;
static char s_small[8];
static char s_large[64];

void setUp(void)
{
    for (size_t at = 0u; at < sizeof s_out; at++)
    {
        s_out[at] = 0x7F;
    }
    s_handed_size = 0u;
}

void tearDown(void)
{
}

static VerbaLinea line_of(size_t capacity)
{
    const VerbaLinea line = {s_out, capacity, 0u, NULL, NULL, 0u};
    return line;
}

// Takes every byte a line hands it, in order, and reports them taken
static int line_handed(void *context, const char *bytes, size_t size)
{
    (void)context;
    for (size_t at = 0u; at < size; at++)
    {
        s_handed[s_handed_size + at] = bytes[at];
    }
    s_handed_size += size;
    return 1;
}

static int line_refused(void *context, const char *bytes, size_t size)
{
    (void)context;
    (void)bytes;
    (void)size;
    return 0;
}

// Moves the line it is handed from s_small into s_large, what it holds kept, and refuses once there
static int line_grown(void *context, const char *bytes, size_t size)
{
    VerbaLinea *const line = (VerbaLinea *)context;

    if (line->out == s_large)
    {
        return 0;
    }
    for (size_t at = 0u; at < size; at++)
    {
        s_large[at] = bytes[at];
    }
    line->out = s_large;
    line->capacity = sizeof s_large;
    return MMGR_LINE_GROWN;
}

void test_put_n_writes_the_bytes_and_finish_terminates_them(void)
{
    VerbaLinea line = line_of(16u);

    mmgr_line_put_n(&line, "abcdef", 3u);
    TEST_ASSERT_EQUAL_size_t(3u, line.at);
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_line_finish(&line));
    TEST_ASSERT_EQUAL_STRING("abc", s_out);
}

void test_a_byte_is_held_back_for_the_terminator(void)
{
    VerbaLinea fits = line_of(8u);
    VerbaLinea over = line_of(8u);

    mmgr_line_put_n(&fits, "1234567", 7u);
    TEST_ASSERT_TRUE(mmgr_line_ok(&fits));
    mmgr_line_put_n(&over, "12345678", 8u);
    TEST_ASSERT_FALSE_MESSAGE(mmgr_line_ok(&over), "eight bytes do not fit a line of eight");
}

void test_a_write_past_the_room_fills_a_line_with_no_flush(void)
{
    VerbaLinea line = line_of(8u);

    mmgr_line_put(&line, "abc");
    mmgr_line_put(&line, "defghij");
    TEST_ASSERT_EQUAL_size_t(8u, line.at);
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_line_finish(&line), "a full line finishes at no length");
}

void test_a_full_line_takes_no_more(void)
{
    VerbaLinea line = line_of(4u);

    mmgr_line_put(&line, "toolong");
    mmgr_line_ch(&line, 'x');
    mmgr_line_decimal(&line, 7u, 1u);
    TEST_ASSERT_EQUAL_size_t(4u, line.at);
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
}

void test_put_measures_a_terminated_text(void)
{
    VerbaLinea line = line_of(32u);

    mmgr_line_put(&line, "hello");
    mmgr_line_put(&line, ", line");
    TEST_ASSERT_EQUAL_size_t(11u, mmgr_line_finish(&line));
    TEST_ASSERT_EQUAL_STRING("hello, line", s_out);
}

void test_put_most_writes_at_most_its_count(void)
{
    VerbaLinea line = line_of(32u);

    mmgr_line_put_most(&line, "abcdefgh", 3u);
    mmgr_line_put_most(&line, "xy", 10u);
    TEST_ASSERT_EQUAL_size_t(5u, mmgr_line_finish(&line));
    TEST_ASSERT_EQUAL_STRING("abcxy", s_out);
}

void test_put_columns_pads_with_spaces_after_the_text(void)
{
    VerbaLinea line = line_of(64u);

    mmgr_line_put_columns(&line, "ab", 5u);
    mmgr_line_put_columns(&line, "longer", 3u);
    mmgr_line_ch(&line, '|');
    TEST_ASSERT_EQUAL_STRING_LEN("ab   longer|", s_out, 12u);
    TEST_ASSERT_EQUAL_size_t(12u, line.at);
}

void test_put_columns_pads_past_one_run_of_spaces(void)
{
    VerbaLinea line = line_of(64u);

    mmgr_line_put_columns(&line, "a", 40u);
    mmgr_line_ch(&line, '|');
    TEST_ASSERT_EQUAL_size_t(41u, line.at);
    TEST_ASSERT_EQUAL_CHAR('a', s_out[0]);
    for (size_t at = 1u; at < 40u; at++)
    {
        TEST_ASSERT_EQUAL_CHAR(' ', s_out[at]);
    }
    TEST_ASSERT_EQUAL_CHAR('|', s_out[40]);
}

void test_ch_writes_one_character(void)
{
    VerbaLinea line = line_of(4u);

    mmgr_line_ch(&line, 'q');
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_line_finish(&line));
    TEST_ASSERT_EQUAL_STRING("q", s_out);
}

void test_decimal_pads_with_zeros_to_its_fewest_digits(void)
{
    VerbaLinea line = line_of(64u);

    mmgr_line_decimal(&line, 42u, 5u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, 0u, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, UINT64_MAX, 1u);
    mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING("00042 0 18446744073709551615", s_out);
}

void test_decimal_holds_its_fewest_digits_at_255(void)
{
    VerbaLinea line = line_of(300u);

    mmgr_line_decimal(&line, 7u, 300u);
    TEST_ASSERT_EQUAL_size_t(255u, line.at);
    TEST_ASSERT_EQUAL_CHAR('0', s_out[0]);
    TEST_ASSERT_EQUAL_CHAR('7', s_out[254]);
}

void test_decimal_columns_aligns_right_with_spaces(void)
{
    VerbaLinea line = line_of(64u);

    mmgr_line_decimal_columns(&line, 42u, 5u);
    mmgr_line_ch(&line, '|');
    mmgr_line_decimal_columns(&line, 123456u, 3u);
    mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING("   42|123456", s_out);
}

void test_decimal_columns_holds_its_columns_at_255(void)
{
    VerbaLinea line = line_of(300u);

    mmgr_line_decimal_columns(&line, 9u, 1000u);
    TEST_ASSERT_EQUAL_size_t(255u, line.at);
    TEST_ASSERT_EQUAL_CHAR(' ', s_out[0]);
    TEST_ASSERT_EQUAL_CHAR('9', s_out[254]);
}

void test_decimal_signed_writes_the_sign_and_the_magnitude(void)
{
    VerbaLinea line = line_of(64u);

    mmgr_line_decimal_signed(&line, -42);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal_signed(&line, 17);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal_signed(&line, INT64_MIN);
    mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING("-42 17 -9223372036854775808", s_out);
}

void test_hex_writes_lower_case_padded_to_its_fewest_digits(void)
{
    VerbaLinea line = line_of(64u);

    mmgr_line_hex(&line, 255u, 4u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_hex(&line, 0xDEADBEEFu, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_hex(&line, 0u, 1u);
    mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING("00ff deadbeef 0", s_out);
}

void test_a_number_that_does_not_fit_fills_the_line(void)
{
    VerbaLinea line = line_of(4u);

    mmgr_line_decimal(&line, 12345u, 1u);
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
}

void test_flush_on_a_line_with_no_flush_refuses(void)
{
    VerbaLinea line = line_of(16u);

    mmgr_line_put(&line, "abc");
    TEST_ASSERT_FALSE(mmgr_line_flush(&line));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u, line.at, "a refused flush leaves a line with no flush as it was");
}

void test_flush_hands_what_the_line_holds_and_empties_it(void)
{
    VerbaLinea line = {s_out, 16u, 0u, line_handed, NULL, 0u};

    mmgr_line_put(&line, "abc");
    TEST_ASSERT_TRUE(mmgr_line_flush(&line));
    TEST_ASSERT_EQUAL_size_t(0u, line.at);
    TEST_ASSERT_EQUAL_size_t(3u, line.flushed);
    TEST_ASSERT_EQUAL_size_t(3u, s_handed_size);
    TEST_ASSERT_EQUAL_STRING_LEN("abc", s_handed, 3u);
}

void test_flush_on_a_full_line_refuses(void)
{
    VerbaLinea line = {s_out, 4u, 4u, line_handed, NULL, 0u};

    TEST_ASSERT_FALSE(mmgr_line_flush(&line));
    TEST_ASSERT_EQUAL_size_t(0u, s_handed_size);
}

void test_a_line_with_a_flush_hands_a_long_text_on_as_it_fills(void)
{
    VerbaLinea line = {s_out, 8u, 0u, line_handed, NULL, 0u};
    const char *const text = "a text far longer than the eight bytes the line holds";

    mmgr_line_put(&line, text);
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
    TEST_ASSERT_TRUE(mmgr_line_flush(&line));
    TEST_ASSERT_EQUAL_size_t(53u, s_handed_size);
    TEST_ASSERT_EQUAL_size_t(53u, line.flushed);
    TEST_ASSERT_EQUAL_STRING_LEN(text, s_handed, 53u);
}

void test_numbers_and_padding_cross_a_flush(void)
{
    VerbaLinea line = {s_out, 4u, 0u, line_handed, NULL, 0u};

    mmgr_line_decimal(&line, 1234567u, 1u);
    mmgr_line_put_columns(&line, "x", 6u);
    mmgr_line_hex(&line, 0xABCDEFu, 8u);
    TEST_ASSERT_TRUE(mmgr_line_flush(&line));
    TEST_ASSERT_EQUAL_size_t(21u, s_handed_size);
    TEST_ASSERT_EQUAL_STRING_LEN("1234567x     00abcdef", s_handed, 21u);
}

void test_a_line_of_one_byte_with_a_flush_fills_rather_than_handing_nothing_on(void)
{
    VerbaLinea line = {s_out, 1u, 0u, line_handed, NULL, 0u};

    mmgr_line_put(&line, "a");
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
    TEST_ASSERT_EQUAL_size_t(0u, s_handed_size);
}

void test_a_refusing_flush_leaves_the_line_full(void)
{
    VerbaLinea line = {s_out, 4u, 0u, line_refused, NULL, 0u};

    mmgr_line_put(&line, "more than four");
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_line_finish(&line));
}

void test_a_grown_line_keeps_what_it_holds_and_goes_on(void)
{
    VerbaLinea line = {s_small, sizeof s_small, 0u, line_grown, NULL, 0u};

    line.context = &line;
    mmgr_line_put(&line, "0123456789abcdef");
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
    TEST_ASSERT_EQUAL_PTR(s_large, line.out);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, line.flushed, "a grown line handed nothing on");
    TEST_ASSERT_EQUAL_size_t(16u, mmgr_line_finish(&line));
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef", s_large);
}

void test_a_grown_line_that_cannot_grow_again_is_full(void)
{
    VerbaLinea line = {s_small, sizeof s_small, 0u, line_grown, NULL, 0u};
    char text[80];

    for (size_t at = 0u; at < (sizeof text - 1u); at++)
    {
        text[at] = 'z';
    }
    text[sizeof text - 1u] = '\0';
    line.context = &line;
    mmgr_line_put(&line, text);
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
}
