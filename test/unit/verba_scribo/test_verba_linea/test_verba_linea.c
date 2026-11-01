#include "verba_scribo/verba_linea.h"

#include "guard_page.h"
#include "unity.h"

#include <stdint.h>

static char s_out[300];
static char s_handed[512];
static size_t s_handed_size;

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

void test_a_put_stops_where_the_flush_refuses(void)
{
    VerbaLinea line = {s_out, 4u, 0u, line_refused, NULL, 0u};

    TEST_ASSERT_EQUAL_size_t(3u, mmgr_line_put_n(&line, "abcdefgh", 8u));
}

void test_a_put_on_a_line_with_no_room_left_takes_nothing(void)
{
    VerbaLinea line = line_of(4u);

    mmgr_line_put(&line, "abcdefgh");
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_line_put(&line, "x"));

    VerbaLinea none = line_of(0u);

    TEST_ASSERT_EQUAL_size_t(0u, mmgr_line_put(&none, "x"));
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

void test_a_write_answers_the_bytes_the_line_took(void)
{
    VerbaLinea line = line_of(8u);

    TEST_ASSERT_EQUAL_size_t(2u, mmgr_line_put_n(&line, "ab", 2u));
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_line_put(&line, "c"));
    TEST_ASSERT_EQUAL_size_t(1u, mmgr_line_put_most(&line, "de", 1u));
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_line_put(&line, ""));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u, mmgr_line_put_n(&line, "wxyz", 4u), "the three bytes left of the room");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_line_put(&line, "more"), "a full line takes none");
    TEST_ASSERT_EQUAL_STRING("abcdwxy", s_out);
}

void test_a_caller_offsets_by_the_answer_and_rereads_a_chunk_into_the_same_room(void)
{
    const char *const text = "0123456789abcdef";
    const size_t length = 16u;
    VerbaLinea line = line_of(8u);
    size_t done = 0u;

    while (done < length)
    {
        done += mmgr_line_put_n(&line, text + done, length - done);
        for (size_t at = 0u; s_out[at] != '\0'; at++)
        {
            s_handed[s_handed_size] = s_out[at];
            s_handed_size += 1u;
        }
        line.at = 0u;
    }
    TEST_ASSERT_EQUAL_size_t(length, s_handed_size);
    TEST_ASSERT_EQUAL_STRING_LEN(text, s_handed, length);
}

void test_a_refused_flush_answers_the_bytes_the_line_holds(void)
{
    VerbaLinea line = {s_out, 4u, 0u, line_refused, NULL, 0u};

    TEST_ASSERT_EQUAL_size_t(3u, mmgr_line_put(&line, "more than four"));
    TEST_ASSERT_EQUAL_STRING("mor", s_out);
}

void test_a_flush_takes_the_whole_of_a_text_and_answers_all_of_it(void)
{
    VerbaLinea line = {s_out, 8u, 0u, line_handed, NULL, 0u};

    TEST_ASSERT_EQUAL_size_t(53u, mmgr_line_put(&line, "a text far longer than the eight bytes the line holds"));
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
}

void test_every_write_leaves_the_line_terminated(void)
{
    VerbaLinea line = line_of(32u);

    mmgr_line_put(&line, "ab");
    TEST_ASSERT_EQUAL_STRING("ab", s_out);
    mmgr_line_put_n(&line, "cdef", 1u);
    TEST_ASSERT_EQUAL_STRING("abc", s_out);
    mmgr_line_ch(&line, 'd');
    TEST_ASSERT_EQUAL_STRING("abcd", s_out);
    mmgr_line_decimal(&line, 5u, 1u);
    TEST_ASSERT_EQUAL_STRING("abcd5", s_out);
    mmgr_line_hex(&line, 15u, 1u);
    TEST_ASSERT_EQUAL_STRING("abcd5f", s_out);
    mmgr_line_decimal_signed(&line, -1);
    TEST_ASSERT_EQUAL_STRING("abcd5f-1", s_out);
    mmgr_line_put_columns(&line, "x", 2u);
    TEST_ASSERT_EQUAL_STRING("abcd5f-1x ", s_out);
    mmgr_line_decimal_columns(&line, 7u, 2u);
    TEST_ASSERT_EQUAL_STRING("abcd5f-1x  7", s_out);
    mmgr_line_put_most(&line, "yz", 1u);
    TEST_ASSERT_EQUAL_STRING("abcd5f-1x  7y", s_out);
}

void test_a_write_of_nothing_terminates_the_line(void)
{
    VerbaLinea line = line_of(8u);

    mmgr_line_put_n(&line, "x", 0u);
    TEST_ASSERT_EQUAL_STRING("", s_out);
    s_out[0] = 0x7F;
    mmgr_line_put(&line, "");
    TEST_ASSERT_EQUAL_STRING("", s_out);
}

void test_a_text_that_fills_the_room_exactly_is_terminated_inside_the_line(void)
{
    VerbaLinea line = line_of(8u);

    mmgr_line_put(&line, "1234567");
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
    TEST_ASSERT_EQUAL_STRING("1234567", s_out);
    TEST_ASSERT_EQUAL_CHAR(0x7F, s_out[8]);
}

void test_a_text_past_the_room_keeps_what_fits_terminated(void)
{
    VerbaLinea line = line_of(8u);

    mmgr_line_put(&line, "abc");
    mmgr_line_put(&line, "defghij");
    TEST_ASSERT_EQUAL_STRING("abcdefg", s_out);
    TEST_ASSERT_EQUAL_CHAR(0x7F, s_out[8]);
    TEST_ASSERT_FALSE_MESSAGE(mmgr_line_ok(&line), "a truncated line reads as full");
    TEST_ASSERT_EQUAL_size_t(8u, line.at);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_line_finish(&line), "a truncated line finishes at no length");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("abcdefg", s_out, "and its finish leaves it as it was");
}

void test_bytes_past_the_room_keep_what_fits_terminated(void)
{
    VerbaLinea line = line_of(8u);

    mmgr_line_put_n(&line, "abcdefghij", 10u);
    TEST_ASSERT_EQUAL_STRING("abcdefg", s_out);
    TEST_ASSERT_EQUAL_CHAR(0x7F, s_out[8]);
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
}

void test_a_number_past_the_room_keeps_what_fits_terminated(void)
{
    VerbaLinea line = line_of(4u);

    mmgr_line_decimal(&line, 12345u, 1u);
    TEST_ASSERT_EQUAL_STRING("123", s_out);
    TEST_ASSERT_EQUAL_CHAR(0x7F, s_out[4]);
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
}

void test_a_full_line_stays_as_it_filled(void)
{
    VerbaLinea line = line_of(4u);

    mmgr_line_put(&line, "toolong");
    mmgr_line_ch(&line, 'x');
    mmgr_line_decimal(&line, 7u, 1u);
    mmgr_line_put_n(&line, "yz", 2u);
    TEST_ASSERT_EQUAL_STRING("too", s_out);
    TEST_ASSERT_EQUAL_size_t(4u, line.at);
}

void test_a_line_of_one_byte_holds_the_terminator_alone(void)
{
    VerbaLinea line = line_of(1u);

    mmgr_line_put(&line, "a");
    TEST_ASSERT_EQUAL_CHAR('\0', s_out[0]);
    TEST_ASSERT_EQUAL_CHAR(0x7F, s_out[1]);
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
}

void test_a_line_of_no_bytes_stores_nothing(void)
{
    VerbaLinea line = {NULL, 0u, 0u, NULL, NULL, 0u};

    mmgr_line_put(&line, "a");
    mmgr_line_put_n(&line, "b", 1u);
    mmgr_line_ch(&line, 'c');
    mmgr_line_decimal(&line, 7u, 1u);
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
    TEST_ASSERT_EQUAL_size_t(0u, line.at);
}

void test_a_line_with_a_flush_stays_terminated_as_it_hands_on(void)
{
    VerbaLinea line = {s_out, 8u, 0u, line_handed, NULL, 0u};

    mmgr_line_put(&line, "a text far longer than the eight bytes the line holds");
    TEST_ASSERT_TRUE(mmgr_line_ok(&line));
    TEST_ASSERT_EQUAL_CHAR('\0', s_out[line.at]);
    TEST_ASSERT_TRUE(mmgr_line_flush(&line));
    TEST_ASSERT_EQUAL_size_t(0u, line.at);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", s_out, "a line flushed empty reads as empty");
}

void test_a_refusing_flush_leaves_the_line_terminated(void)
{
    VerbaLinea line = {s_out, 4u, 0u, line_refused, NULL, 0u};

    mmgr_line_put(&line, "more than four");
    TEST_ASSERT_EQUAL_STRING("mor", s_out);
    TEST_ASSERT_EQUAL_CHAR(0x7F, s_out[4]);
    TEST_ASSERT_FALSE(mmgr_line_ok(&line));
}

// Whether capacity bytes of s_out hold a terminator and the byte past them is as setUp left it
static int line_terminated_within(size_t capacity)
{
    int terminated = 0;

    for (size_t at = 0u; (at < capacity) && !terminated; at++)
    {
        terminated = (s_out[at] == '\0');
    }
    return terminated && (s_out[capacity] == 0x7F);
}

// The entry of the place named written over the line, twice, with more than any line below holds
static void line_entry_written(VerbaLinea *line, unsigned int place)
{
    static const char s_long[] = "a text longer than the most bytes any line below holds";

    for (unsigned int round = 0u; round < 2u; round++)
    {
        switch (place)
        {
        case 0u:
            mmgr_line_put(line, s_long);
            break;
        case 1u:
            mmgr_line_put_n(line, s_long, sizeof s_long - 1u);
            break;
        case 2u:
            mmgr_line_put_most(line, s_long, sizeof s_long);
            break;
        case 3u:
            mmgr_line_put_columns(line, s_long, 80u);
            break;
        case 4u:
            mmgr_line_decimal(line, UINT64_MAX, 40u);
            break;
        case 5u:
            mmgr_line_decimal_columns(line, UINT64_MAX, 40u);
            break;
        case 6u:
            mmgr_line_decimal_signed(line, INT64_MIN);
            break;
        case 7u:
            mmgr_line_hex(line, UINT64_MAX, 40u);
            break;
        default:
            for (unsigned int count = 0u; count < 40u; count++)
            {
                mmgr_line_ch(line, 'x');
            }
            break;
        }
    }
}

void test_no_entry_leaves_a_line_of_any_capacity_unterminated(void)
{
    for (size_t capacity = 1u; capacity <= 32u; capacity++)
    {
        for (unsigned int place = 0u; place < 9u; place++)
        {
            setUp();
            VerbaLinea line = line_of(capacity);

            line_entry_written(&line, place);
            TEST_ASSERT_TRUE_MESSAGE(line_terminated_within(capacity),
                                     "a line holds a terminator inside its capacity and stores nothing past it");
        }
    }
}

void test_a_line_over_a_buffer_writes_into_it(void)
{
    char out[8] = "";
    VerbaLinea line = mmgr_line_over(out, sizeof out);

    TEST_ASSERT_EQUAL_size_t(sizeof out, line.capacity);
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_line_put(&line, "abc"));
    TEST_ASSERT_EQUAL_size_t(3u, mmgr_line_finish(&line));
    TEST_ASSERT_EQUAL_STRING("abc", out);
}

void test_a_line_over_no_buffer_has_no_capacity_and_stores_nothing(void)
{
    VerbaLinea line = mmgr_line_over(NULL, 64u);

    TEST_ASSERT_EQUAL_size_t(0u, line.capacity);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_line_put(&line, "abc"));
    mmgr_line_ch(&line, 'x');
    mmgr_line_decimal(&line, 7u, 1u);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_line_finish(&line));
}

void test_a_full_line_at_the_end_of_a_page_stores_nothing_past_it(void)
{
    if (!mmgr_guard_available())
    {
        TEST_IGNORE_MESSAGE("no guard page on this host");
    }
    char *const out = (char *)mmgr_guard_run() + mmgr_guard_page_size() - 8u;
    VerbaLinea line = {out, 8u, 0u, NULL, NULL, 0u};

    mmgr_line_put(&line, "far past the eight bytes the end of the page holds");
    mmgr_line_ch(&line, 'x');
    mmgr_line_decimal(&line, 7u, 1u);
    TEST_ASSERT_EQUAL_size_t(0u, mmgr_line_finish(&line));
    TEST_ASSERT_EQUAL_CHAR('\0', out[7]);
    TEST_ASSERT_EQUAL_STRING("far pas", out);
}
