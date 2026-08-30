#include "oracle_divergence.h"
#include "unity.h"

#include "verba_scribo/verba_scribo.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buf[256];
static size_t cap;
static size_t at;

static void fresh(size_t room)
{
    for (uint32_t i = 0; i < sizeof buf; i++)
    {
        buf[i] = 0x7Fu;
    }
    cap = room;
    at = 0;
}

static void put(const char *text)
{
    at = MMGR_CALL(verba_textus.put, VerbaTextusCfg, .out = buf, .cap = cap, .at = at, .text = text);
}

static void put_n(const char *text, size_t len)
{
    at = MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = buf, .cap = cap, .at = at, .text = text, .text_len = len);
}

static void put_clip(const char *text)
{
    at = MMGR_CALL(verba_textus.put_clip, VerbaTextusCfg, .out = buf, .cap = cap, .at = at, .text = text);
}

static void ch(char value)
{
    at = MMGR_CALL(verba_littera.ch, VerbaLitteraCfg, .out = buf, .cap = cap, .at = at, .ch = value);
}

static void u32(uint32_t value)
{
    at = MMGR_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = buf, .cap = cap, .at = at, .val = value);
}

static void u32w(uint32_t value, uint8_t width)
{
    at = MMGR_CALL(verba_numerus.u32w, VerbaNumerusCfg, .out = buf, .cap = cap, .at = at, .val = value, .min = width);
}

static void u64(uint64_t value)
{
    at = MMGR_CALL(verba_numerus.u64, VerbaNumerusCfg, .out = buf, .cap = cap, .at = at, .val = value);
}

static void u64_clip(uint64_t value, uint8_t columns)
{
    at = MMGR_CALL(verba_numerus.u64_clip, VerbaNumerusCfg, .out = buf, .cap = cap, .at = at, .val = value,
                   .columns = columns);
}

static void i64(int64_t value)
{
    at = MMGR_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = buf, .cap = cap, .at = at, .sval = value);
}

static void uint_of(uint64_t value, uint8_t base, uint8_t min)
{
    at = MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = buf, .cap = cap, .at = at, .val = value, .base = base,
                   .min = min);
}

static void hex(uint64_t value, uint8_t min)
{
    at = MMGR_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = buf, .cap = cap, .at = at, .val = value, .min = min);
}

static void g(double value, uint8_t sig)
{
    at = MMGR_CALL(verba_fractio.g, VerbaFractioCfg, .out = buf, .cap = cap, .at = at, .real = value, .sig = sig);
}

static void fixed(double value, uint8_t decimals)
{
    at = MMGR_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = buf, .cap = cap, .at = at, .real = value,
                   .decimals = decimals);
}

static void json(const char *text)
{
    at = MMGR_CALL(verba_textus.json, VerbaTextusCfg, .out = buf, .cap = cap, .at = at, .text = text);
}

static void xml(const char *text)
{
    at = MMGR_CALL(verba_textus.xml, VerbaTextusCfg, .out = buf, .cap = cap, .at = at, .text = text);
}

static size_t finish(void)
{
    return MMGR_CALL(verba_finis.finish, VerbaFinisCfg, .out = buf, .cap = cap, .at = at);
}

static mmgr_bool ok(void)
{
    return MMGR_CALL(verba_finis.ok, VerbaFinisCfg, .cap = cap, .at = at);
}

static double an_inf(void)
{
    return 1e308 * 10.0;
}

static double a_nan(void)
{
    const double inf = an_inf();
    return inf - inf;
}

void setUp(void)
{
    fresh(sizeof buf);
}

void tearDown(void)
{
}

static void want_printf(const char *fmt, ...)
{
    char ref[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ref, sizeof ref, fmt, ap);
    va_end(ap);

    finish();
    TEST_ASSERT_EQUAL_STRING(ref, buf);
}

void test_verba_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("verba_scribo.h compiled with no header before it");
}

void test_put_and_put_n(void)
{
    put("abc");
    put_n("defgh", 3u);
    TEST_ASSERT_EQUAL_size_t(6u, finish());
    TEST_ASSERT_EQUAL_STRING("abcdef", buf);
}

void test_put_of_nothing(void)
{
    put("");
    put_n("x", 0u);
    TEST_ASSERT_EQUAL_size_t(0u, finish());
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_ch(void)
{
    for (char c = 'a'; c <= 'e'; c++)
    {
        ch(c);
    }
    finish();
    TEST_ASSERT_EQUAL_STRING("abcde", buf);
}

void test_unsigned_decimal_matches_printf(void)
{
    static const uint64_t vals[] = {
        0ull, 1ull, 9ull, 10ull, 99ull, 100ull, 12345ull, 4294967295ull, 18446744073709551615ull};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        fresh(sizeof buf);
        u64(vals[i]);
        want_printf("%llu", (unsigned long long)vals[i]);
    }
}

void test_u32_matches_printf(void)
{
    static const uint32_t vals[] = {0u, 1u, 42u, 65535u, 4294967295u};
    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        fresh(sizeof buf);
        u32(vals[i]);
        want_printf("%lu", (unsigned long)vals[i]);
    }
}

void test_signed_decimal_matches_printf(void)
{
    static const int64_t vals[] = {
        0, 1, -1, 42, -42, 2147483647, -2147483648ll, 9223372036854775807ll, (-9223372036854775807ll - 1ll)};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        fresh(sizeof buf);
        i64(vals[i]);
        want_printf("%lld", (long long)vals[i]);
    }
}

void test_hex_matches_printf(void)
{
    static const uint64_t vals[] = {0ull, 1ull, 0xFull, 0x10ull, 0xDEADBEEFull, 0xFFFFFFFFFFFFFFFFull};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        fresh(sizeof buf);
        hex(vals[i], 1u);
        want_printf("%llx", (unsigned long long)vals[i]);
    }
}

void test_hex_zero_pads_like_printf(void)
{
    for (unsigned w = 1u; w <= 16u; w++)
    {
        fresh(sizeof buf);
        hex(0xABCu, w);
        want_printf("%0*llx", (int)w, 0xABCull);
    }
}

void test_u32w_zero_pads_like_printf(void)
{
    for (unsigned w = 1u; w <= 10u; w++)
    {
        fresh(sizeof buf);
        u32w(42u, w);
        want_printf("%0*lu", (int)w, 42ul);
    }
}

void test_uint_in_every_base(void)
{
    fresh(sizeof buf);
    uint_of(255u, 16u, 1u);
    want_printf("%x", 255u);

    fresh(sizeof buf);
    uint_of(255u, 8u, 1u);
    want_printf("%o", 255u);

    fresh(sizeof buf);
    uint_of(255u, 10u, 1u);
    want_printf("%u", 255u);

    fresh(sizeof buf);
    uint_of(5u, 2u, 1u);
    want_printf("%u", 5u);
}

void test_u64_clip_pads_to_a_column(void)
{
    fresh(sizeof buf);
    u64_clip(42u, 5u);
    finish();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(5u, at, "a narrow value is right aligned in the column");
    TEST_ASSERT_EQUAL_STRING("   42", buf);

    fresh(sizeof buf);
    u64_clip(1234567890123ull, 4u);
    finish();
    TEST_ASSERT_EQUAL_size_t_MESSAGE(13u, at, "a value wider than the column is not cut short");
    TEST_ASSERT_EQUAL_STRING("1234567890123", buf);
}

void test_put_clip_truncates_instead_of_latching(void)
{
    fresh(8u);
    put_clip("far too long for this");
    TEST_ASSERT_TRUE_MESSAGE(ok(), "clip truncates rather than latching an overflow");
    finish();
    TEST_ASSERT_LESS_THAN_size_t(8u, at);
}

void test_json_wraps_and_escapes(void)
{
    json("a\"b\\c");
    finish();
    TEST_ASSERT_EQUAL_STRING("\"a\\\"b\\\\c\"", buf);
}

void test_json_escapes_control_bytes(void)
{
    json("a\nb\tc");
    finish();
    TEST_ASSERT_EQUAL_STRING("\"a\\nb\\tc\"", buf);

    fresh(sizeof buf);
    json("\x01");
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("\"\\u0001\"", buf, "a control byte with no short escape goes to \\u");
}

void test_json_of_null_is_an_empty_string(void)
{
    json(NULL);
    finish();
    TEST_ASSERT_EQUAL_STRING("\"\"", buf);
}

void test_xml_escapes_its_five(void)
{
    xml("a<b>c&d\"e");
    finish();
    TEST_ASSERT_EQUAL_STRING("a&lt;b&gt;c&amp;d&quot;e", buf);
}

void test_xml_passes_ordinary_text_through(void)
{
    xml("plain text 123");
    finish();
    TEST_ASSERT_EQUAL_STRING("plain text 123", buf);
}

void test_float_predicates(void)
{
    const double inf = 1e308 * 10.0;
    const double nan = inf - inf;

    TEST_ASSERT_TRUE(MMGR_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = inf));
    TEST_ASSERT_TRUE(MMGR_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = -inf));
    TEST_ASSERT_FALSE(MMGR_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = 1.0));

    TEST_ASSERT_TRUE(MMGR_CALL(verba_fractio.is_nan, VerbaFractioCfg, .real = nan));
    TEST_ASSERT_FALSE(MMGR_CALL(verba_fractio.is_nan, VerbaFractioCfg, .real = 1.0));
    TEST_ASSERT_FALSE(MMGR_CALL(verba_fractio.is_nan, VerbaFractioCfg, .real = inf));

    TEST_ASSERT_TRUE(MMGR_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = -1.0));
    TEST_ASSERT_TRUE(MMGR_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = -0.0));
    TEST_ASSERT_FALSE(MMGR_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = 1.0));
    TEST_ASSERT_FALSE(MMGR_CALL(verba_fractio.sign_bit, VerbaFractioCfg, .real = 0.0));
}

void test_fixed_matches_printf(void)
{
    static const double vals[] = {0.0, 1.0, -1.0, 0.25, 0.75, 3.14159265358979, -2.4, 123.456, 1000.0};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        for (unsigned d = 2u; d <= 4u; d++)
        {
            fresh(sizeof buf);
            fixed(vals[i], d);
            want_printf("%.*f", (int)d, vals[i]);
        }
    }
}

void test_fixed_rounds_a_tie_to_even(void)
{
    fresh(sizeof buf);
    fixed(1.5, 0u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("2", buf, "1 is odd, so the tie goes up");

    fresh(sizeof buf);
    fixed(2.5, 0u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("2", buf, "2 is even, so the tie stays");

    fresh(sizeof buf);
    fixed(3.5, 0u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("4", buf, "3 is odd, so the tie goes up");

    fresh(sizeof buf);
    fixed(0.5, 0u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("0", buf, "0 is even, so the tie stays");

    fresh(sizeof buf);
    fixed(-1.5, 0u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("-2", buf, "the sign is written first and does not change it");

    fresh(sizeof buf);
    fixed(0.125, 2u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("0.12", buf, "2 is even, so the tie stays");

    fresh(sizeof buf);
    fixed(0.375, 2u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("0.38", buf, "7 is odd, so the tie goes up");
}

void test_fixed_is_exact_below_a_64_bit_shift(void)
{
    fresh(sizeof buf);
    fixed(2.0447843820796629e-41, 9u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("0.000000000", buf, "was 0.006958041");

    fresh(sizeof buf);
    fixed(5e-324, 18u);
    finish();
    TEST_ASSERT_EQUAL_STRING("0.000000000000000000", buf);

    fresh(sizeof buf);
    fixed(0x1p-63, 18u);
    finish();
    TEST_ASSERT_EQUAL_STRING("0.000000000000000000", buf);

    fresh(sizeof buf);
    fixed(0x1p-64, 18u);
    finish();
    TEST_ASSERT_EQUAL_STRING("0.000000000000000000", buf);

    fresh(sizeof buf);
    fixed(0x1p-65, 18u);
    finish();
    TEST_ASSERT_EQUAL_STRING("0.000000000000000000", buf);

    fresh(sizeof buf);
    fixed(1.0 / 3.0, 17u);
    finish();
    TEST_ASSERT_EQUAL_STRING("0.33333333333333331", buf);
}

void test_g_rounds_a_tie(void)
{
    MMGR_SKIP_ON_ORACLE("C leaves the tie to the implementation and the two disagree, which is the point");
    fresh(sizeof buf);
    g(1.5, 1u);
    finish();
    TEST_ASSERT_EQUAL_STRING("2", buf);

    fresh(sizeof buf);
    g(2.5, 1u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("2", buf, "half to even, like the IEEE default");

    fresh(sizeof buf);
    g(0.25, 1u);
    finish();
    TEST_ASSERT_EQUAL_STRING("0.2", buf);
}

void test_g_matches_printf(void)
{
    static const double vals[] = {0.0, 1.0, -1.0, 0.1, 100.0, 0.001, 1e10, 123.456, 2.0};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        for (unsigned s = 1u; s <= 6u; s++)
        {
            fresh(sizeof buf);
            g(vals[i], s);
            want_printf("%.*g", (int)s, vals[i]);
        }
    }
}

void test_g_and_fixed_of_the_specials(void)
{
    const double inf = 1e308 * 10.0;
    const double nan = inf - inf;

    fresh(sizeof buf);
    g(inf, 6u);
    finish();
    TEST_ASSERT_TRUE_MESSAGE(buf[0] != '\0', "an infinity renders as something rather than nothing");

    fresh(sizeof buf);
    g(-inf, 6u);
    finish();
    TEST_ASSERT_EQUAL_CHAR('-', buf[0]);

    fresh(sizeof buf);
    g(nan, 6u);
    finish();
    TEST_ASSERT_TRUE(buf[0] != '\0');

    fresh(sizeof buf);
    fixed(-0.0, 2u);
    finish();
    TEST_ASSERT_EQUAL_CHAR_MESSAGE('-', buf[0], "negative zero keeps its sign");
}

void test_overflow_latches_and_finish_reports_it(void)
{
    fresh(4u);
    put("way too long for four bytes");
    TEST_ASSERT_FALSE_MESSAGE(ok(), "overflow latches");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, finish(), "finish reports nothing usable");
}

void test_writes_after_overflow_are_ignored(void)
{
    fresh(4u);
    put("too long already");
    TEST_ASSERT_FALSE(ok());

    ch('x');
    u32(1u);
    hex(1u, 1u);
    json("x");
    xml("x");
    fixed(1.0, 1u);
    g(1.0, 1u);
    TEST_ASSERT_FALSE_MESSAGE(ok(), "every entry stays quiet once overflow has latched");
}

void test_each_entry_can_overflow_on_its_own(void)
{
    fresh(2u);
    u64(18446744073709551615ull);
    TEST_ASSERT_FALSE_MESSAGE(ok(), "a number too wide for the buffer overflows");

    fresh(2u);
    hex(0xFFFFFFFFull, 8u);
    TEST_ASSERT_FALSE(ok());

    fresh(2u);
    json("abcdef");
    TEST_ASSERT_FALSE(ok());

    fresh(2u);
    xml("a<b<c");
    TEST_ASSERT_FALSE(ok());

    fresh(3u);
    fixed(123456.789, 3u);
    TEST_ASSERT_FALSE(ok());
}

void test_a_zero_capacity_builder_cannot_write(void)
{
    fresh(0u);
    ch('x');
    TEST_ASSERT_EQUAL_size_t(0u, finish());
}

void test_the_literal_helper(void)
{
    put_n("literal", sizeof "literal" - 1u);
    finish();
    TEST_ASSERT_EQUAL_STRING("literal", buf);
}

void test_namespace_is_wired(void)
{
    TEST_ASSERT_NOT_NULL(verba_textus.put_n);
    TEST_ASSERT_NOT_NULL(verba_littera.ch);
    TEST_ASSERT_NOT_NULL(verba_numerus.uint);
    TEST_ASSERT_NOT_NULL(verba_fractio.g);
    TEST_ASSERT_NOT_NULL(verba_finis.finish);
}

void test_the_clipping_entries_stay_quiet_after_overflow(void)
{
    fresh(4u);
    put("too long already");
    TEST_ASSERT_FALSE(ok());
    const size_t was = at;

    put_n("abc", 3u);
    put_clip("abc");
    u64_clip(42u, 4u);
    TEST_ASSERT_FALSE(ok());
    TEST_ASSERT_EQUAL_size_t_MESSAGE(was, at, "a latched builder took a write anyway");
}

void test_put_clip_of_null_writes_nothing(void)
{
    put_clip(NULL);
    TEST_ASSERT_EQUAL_size_t(0u, finish());
    TEST_ASSERT_TRUE_MESSAGE(ok(), "nothing to write is not an overflow");
}

void test_put_clip_with_no_room_left_writes_nothing(void)
{
    fresh(4u);
    put("abc");
    TEST_ASSERT_TRUE(ok());

    put_clip("more");
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("abc", buf, "clipping does not push past the terminator");
    TEST_ASSERT_TRUE_MESSAGE(ok(), "clipping truncates, it does not latch");
}

void test_u64_clip_that_does_not_fit_writes_nothing(void)
{
    fresh(4u);
    u64_clip(123456789u, 9u);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, finish(), "a column too wide for the buffer is dropped whole");
}

void test_xml_of_null_writes_nothing(void)
{
    xml(NULL);
    TEST_ASSERT_EQUAL_size_t(0u, finish());
}

void test_fixed_of_nan(void)
{
    fixed(a_nan(), 2u);
    finish();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buf, "nan"), "a nan did not render as a nan");
}

void test_fixed_of_the_infinities(void)
{
    fixed(an_inf(), 2u);
    finish();
    TEST_ASSERT_EQUAL_STRING("inf", buf);

    fresh(sizeof buf);
    fixed(-an_inf(), 2u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("-inf", buf, "the sign is written before the value is known to be infinite");
}

void test_fixed_of_a_value_too_large_for_the_integer_path(void)
{
    MMGR_SKIP_ON_ORACLE("printf writes all thirty one digits rather than handing the value to %g");
    fixed(1.0e30, 2u);
    finish();

    TEST_ASSERT_EQUAL_CHAR('1', buf[0]);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buf, "e"), "a value out of integer range comes back in exponent form");
}

void test_fixed_of_a_value_with_no_fraction_left(void)
{
    fixed(1.8014398509481984e16, 0u);
    want_printf("%.0f", 1.8014398509481984e16);
}

void test_fixed_clamps_its_decimals(void)
{
    fixed(1.5, 25u);
    const size_t n = finish();

    TEST_ASSERT_EQUAL_CHAR('1', buf[0]);
    TEST_ASSERT_EQUAL_CHAR('.', buf[1]);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(20u, n, "one digit, a point and eighteen decimals");
}

void test_fixed_carries_a_fraction_that_rounds_to_one(void)
{
    fixed(0.999, 2u);
    want_printf("%.2f", 0.999);
}

void test_fixed_of_negative_zero(void)
{
    fixed(-0.0, 1u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("-0.0", buf, "the sign of a negative zero survives");
}

void test_g_of_nan(void)
{
    g(a_nan(), 3u);
    finish();
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buf, "nan"), "a nan did not render as a nan");
}

void test_g_of_zero(void)
{
    g(0.0, 3u);
    finish();
    TEST_ASSERT_EQUAL_CHAR('0', buf[0]);
}

void test_g_of_a_very_small_value(void)
{
    g(1.0e-300, 4u);
    finish();
    TEST_ASSERT_NOT_NULL(strstr(buf, "e-"));
}

void test_g_of_a_very_large_value(void)
{
    g(1.0e300, 4u);
    finish();
    TEST_ASSERT_NOT_NULL(strstr(buf, "e+"));
}

void test_g_of_one_significant_digit(void)
{
    const double v = 9.9e-5;
    g(v, 1u);
    finish();
    TEST_ASSERT_DOUBLE_WITHIN(1e-20, 1e-4, strtod(buf, NULL));
}

void test_g_of_zero_significant_digits_is_one(void)
{
    char one[64];
    g(1.25, 0u);
    finish();
    memcpy(one, buf, sizeof one);

    fresh(sizeof buf);
    g(1.25, 1u);
    finish();
    TEST_ASSERT_EQUAL_STRING_MESSAGE(one, buf, "asking for no digits is asking for one");
}

void test_json_escapes_the_two_character_forms(void)
{
    json("a\"b\\c");
    finish();
    TEST_ASSERT_EQUAL_STRING("\"a\\\"b\\\\c\"", buf);
}

void test_json_escapes_the_named_control_bytes(void)
{
    json("a\nb\tc\rd\be\f");
    finish();
    TEST_ASSERT_NOT_NULL(strstr(buf, "\\n"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\\t"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\\r"));
}

void test_json_escapes_an_unnamed_control_byte_as_a_code_point(void)
{
    const char s[] = {'a', 0x01, 'b', 0x1F, '\0'};
    json(s);
    finish();
    TEST_ASSERT_EQUAL_STRING("\"a\\u0001b\\u001f\"", buf);
}

void test_json_overflows_on_each_escape_form(void)
{
    fresh(3u);
    json("\"");
    TEST_ASSERT_FALSE_MESSAGE(ok(), "a two character escape did not fit");

    fresh(3u);
    json("\x01");
    TEST_ASSERT_FALSE_MESSAGE(ok(), "a six character escape did not fit");

    fresh(3u);
    json("ab");
    TEST_ASSERT_FALSE_MESSAGE(ok(), "a plain byte did not fit");
}

void test_finish_of_a_zero_capacity_builder_reports_nothing(void)
{
    fresh(0u);
    TEST_ASSERT_EQUAL_size_t(0u, finish());
}

void test_g_over_every_precision(void)
{
    static const double vals[] = {1.0,    3.0,    7.0,        1.0 / 3.0, 2.0 / 7.0, 1234.5678,
                                  1.0e-7, 1.0e-3, 9.87654e12, 6.02e23,   1.0e18,    1.0e19};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        for (unsigned sig = 1u; sig <= 19u; sig++)
        {
            fresh(sizeof buf);
            g(vals[i], sig);
            finish();

            const double back = strtod(buf, NULL);
            const double want = vals[i];
            const double tol = (want < 0.0 ? -want : want) * 0.5;

            TEST_ASSERT_TRUE_MESSAGE(buf[0] != '\0', "g produced nothing");
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(tol, want, back, "g does not read back as the value it was given");
        }
    }
}

void test_g_of_a_subnormal(void)
{
    const double tiny = 4.9406564584124654e-324;

    g(tiny, 3u);
    finish();
    TEST_ASSERT_TRUE_MESSAGE(buf[0] != '\0', "the smallest double there is came back as nothing");
    TEST_ASSERT_NOT_NULL(strstr(buf, "e-"));
}

void test_g_of_the_largest_finite_double(void)
{
    g(1.7976931348623157e308, 17u);
    finish();
    TEST_ASSERT_EQUAL_CHAR('1', buf[0]);
    TEST_ASSERT_NOT_NULL(strstr(buf, "e+"));
}

void test_is_inf_says_no_to_a_nan(void)
{
    TEST_ASSERT_FALSE_MESSAGE(MMGR_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = a_nan()),
                              "a nan is not an infinity");
    TEST_ASSERT_TRUE(MMGR_CALL(verba_fractio.is_inf, VerbaFractioCfg, .real = an_inf()));
}

void test_fixed_over_every_decimal_count(void)
{
    static const double vals[] = {0.0, 1.0, 1234.5678, 0.000123, 99.9999, 1.0 / 3.0, 2.0 / 7.0, 123.456};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        for (unsigned d = 0u; d <= 18u; d++)
        {
            fresh(sizeof buf);
            fixed(vals[i], d);
            finish();

            char ref[256];
            (void)snprintf(ref, sizeof ref, "%.*f", (int)d, vals[i]);
            TEST_ASSERT_EQUAL_STRING_MESSAGE(ref, buf, "fixed disagrees with printf");
        }
    }
}

void test_fixed_of_a_fraction_that_lands_on_a_tie(void)
{
    static const double vals[] = {0.5, 1.5, 2.5, 0.25, 0.75, 1.25, 3.375, 0.0625};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        for (unsigned d = 0u; d <= 4u; d++)
        {
            fresh(sizeof buf);
            fixed(vals[i], d);
            finish();
            TEST_ASSERT_TRUE_MESSAGE(buf[0] != '\0', "a tie produced nothing");
        }
    }
}

void test_g_where_the_exponent_estimate_overshoots(void)
{
    static const double vals[] = {
        0.9999999999, 9.999999999,  99.99999999,  999.9999999, 9999.999999, 1.000000001,  10.00000001,
        100.0000001,  1000.000001,  10000.00001,  0.09999999,  0.009999999, 0.0009999999, 9.999999e-10,
        9.999999e-20, 1.000001e-10, 1.000001e-20, 9.999999e20, 1.000001e20, 9.999999e100,
    };

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        for (unsigned sig = 1u; sig <= 17u; sig++)
        {
            fresh(sizeof buf);
            g(vals[i], sig);
            finish();

            const double back = strtod(buf, NULL);
            const double want = vals[i];
            const double tol = (want < 0.0 ? -want : want) * 0.5;

            TEST_ASSERT_TRUE_MESSAGE(buf[0] != '\0', "g produced nothing");
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(tol, want, back, "g does not read back as the value it was given");
        }
    }
}

static unsigned significant_digits(const char *s)
{
    unsigned n = 0;
    int started = 0;

    for (const char *p = s; *p != '\0' && *p != 'e' && *p != 'E'; p++)
    {
        if (*p >= '1' && *p <= '9')
        {
            started = 1;
        }
        if (*p >= '0' && *p <= '9' && started)
        {
            n++;
        }
    }
    return n;
}

void test_g_clamps_its_digit_count(void)
{
    char at_max[128];

    g(1.8464766514526577e-301, MMGR_G_MAX_SIG);
    finish();
    memcpy(at_max, buf, sizeof at_max);

    fresh(sizeof buf);
    g(1.8464766514526577e-301, MMGR_G_MAX_SIG + 7u);
    finish();

    TEST_ASSERT_EQUAL_STRING_MESSAGE(at_max, buf, "asking past the maximum did not come back at the maximum");
    TEST_ASSERT_EQUAL_UINT(MMGR_G_MAX_SIG, significant_digits(buf));
}

void test_g_at_its_maximum_still_reads_back(void)
{
    const double v = 1.8447470568367377e-236;

    g(v, MMGR_G_MAX_SIG);
    finish();

    TEST_ASSERT_EQUAL_UINT(MMGR_G_MAX_SIG, significant_digits(buf));
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(v * 1e-15, v, strtod(buf, NULL), "the clamped rendering does not read back");
}

void test_g_at_two_to_the_sixty_four(void)
{
    const double v = 1.8446744073709552e+22;

    g(v, MMGR_G_MAX_SIG + 1u);
    finish();
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(v * 1e-15, v, strtod(buf, NULL), "2^64 does not read back");
}

void test_g_is_exact_at_every_precision_it_can_carry(void)
{
    static const double vals[] = {1.8464766514526577e-301, 1.8447470568367377e-236, 1.8446744073709552e+22,
                                  2.2250738585072014e-308, 1.2345678901234567e+300};

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        for (unsigned sig = 1u; sig <= MMGR_G_MAX_SIG; sig++)
        {
            fresh(sizeof buf);
            g(vals[i], sig);
            finish();

            const double back = strtod(buf, NULL);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(vals[i] * 0.5, vals[i], back, "g did not read back as its own value");
        }
    }
}

void test_g_of_the_smallest_normal_double(void)
{
    const double v = 2.2250738585072014e-308;

    g(v, 18u);
    finish();
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(v * 0.5, v, strtod(buf, NULL), "the smallest normal double did not survive");
}
