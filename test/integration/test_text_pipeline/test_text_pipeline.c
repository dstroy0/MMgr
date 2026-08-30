#include "unity.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "numeros_scribo/numeros_scribo.h"
#include "verba_scribo/verba_scribo.h"

void test_built_text_reads_back_at_the_length_it_reported(void)
{
    char buf[128];
    const mmgr_fval fields[] = {MMGR_VSTR("id="), MMGR_VU32(4242u), MMGR_VSTR(" ok")};
    const size_t n = MMGR_CALL(numer.emit, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = fields, .nvals = 3u);

    TEST_ASSERT_GREATER_THAN_size_t(0u, n);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(n, MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = buf, .cap = sizeof buf),
                                     "the builder's length and the scanner's length must agree");
}

void test_scanner_finds_what_the_builder_wrote(void)
{
    char buf[128];
    const mmgr_fval fields[] = {MMGR_VSTR("user="), MMGR_VSTR("dstroy0"), MMGR_VSTR(" id="), MMGR_VU32(7u)};

    MMGR_CALL(numer.emit, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = fields, .nvals = 4u);

    TEST_ASSERT_NOT_NULL(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "dstroy0",
                                   .other_cap = 8u, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "id=7",
                               .other_cap = 5u, .ci = MMGR_FALSE));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "id=8",
                                .other_cap = 5u, .ci = MMGR_FALSE));
}

void test_case_folding_agrees_across_builder_and_scanner(void)
{
    char buf[64];
    const mmgr_fval fields[] = {MMGR_VSTR("Content-Type")};

    MMGR_CALL(numer.emit, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = fields, .nvals = 1u);

    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "content-type",
                               .other_cap = 13u, .ci = MMGR_TRUE));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "content-type",
                                .other_cap = 13u, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(
        MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = buf, .other = "CONTENT-TYPE", .cap = sizeof buf, .ci = MMGR_TRUE));
}

void test_appending_keeps_every_earlier_field_findable(void)
{
    char buf[160];
    const mmgr_fval first[] = {MMGR_VSTR("a=1")};
    const mmgr_fval second[] = {MMGR_VSTR(";b=2")};
    const mmgr_fval third[] = {MMGR_VSTR(";c=3")};

    MMGR_CALL(numer.emit, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = first, .nvals = 1u);
    MMGR_CALL(numer.emit_append, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = second, .nvals = 1u);
    MMGR_CALL(numer.emit_append, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = third, .nvals = 1u);

    TEST_ASSERT_EQUAL_STRING("a=1;b=2;c=3", buf);
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "a=1",
                               .other_cap = 4u, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "b=2",
                               .other_cap = 4u, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "c=3",
                               .other_cap = 4u, .ci = MMGR_FALSE));
    TEST_ASSERT_EQUAL_size_t(11u, MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = buf, .cap = sizeof buf));
}

void test_a_builder_overflow_leaves_nothing_for_the_scanner(void)
{
    char buf[8];
    const mmgr_fval fields[] = {MMGR_VSTR("far too long to fit")};

    TEST_ASSERT_EQUAL_size_t(
        0u, MMGR_CALL(numer.emit, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = fields, .nvals = 1u));
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = buf, .cap = sizeof buf));
    TEST_ASSERT_FALSE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "far",
                                .other_cap = 4u, .ci = MMGR_FALSE));
}

void test_every_rendered_number_is_found_by_the_scanner(void)
{
    static const uint32_t numbers[] = {0u, 1u, 9u, 10u, 99u, 100u, 65535u, 1000000u, 4294967295u};

    for (uint32_t i = 0; i < sizeof numbers / sizeof numbers[0]; i++)
    {
        char buf[64];
        char want[32];
        const mmgr_fval wrapped[] = {MMGR_VSTR("<"), MMGR_VU32(numbers[i]), MMGR_VSTR(">")};
        const mmgr_fval bare[] = {MMGR_VU32(numbers[i])};

        MMGR_CALL(numer.emit, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = wrapped, .nvals = 3u);
        MMGR_CALL(numer.emit, NumerosCfg, .out = want, .cap = sizeof want, .vals = bare, .nvals = 1u);

        TEST_ASSERT_NOT_NULL_MESSAGE(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = buf, .cap = sizeof buf,
                                               .other = want, .other_cap = sizeof want, .ci = MMGR_FALSE),
                                     "a number the builder rendered must be findable in its own output");
    }
}

void test_escaped_output_is_still_scannable(void)
{
    char buf[96];
    const mmgr_fval json[] = {MMGR_VJSON("a\"b")};
    const mmgr_fval xml[] = {MMGR_VXML("a<b&c")};

    MMGR_CALL(numer.emit, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = json, .nvals = 1u);

    TEST_ASSERT_EQUAL_STRING("\"a\\\"b\"", buf);
    TEST_ASSERT_EQUAL_size_t(6u, MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = buf, .cap = sizeof buf));
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "\\\"",
                                       .other_cap = 3u, .ci = MMGR_FALSE),
                             "the escape survives into the text");
    TEST_ASSERT_NOT_NULL(MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "a",
                                   .other_cap = 2u, .ci = MMGR_FALSE));

    MMGR_CALL(numer.emit, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = xml, .nvals = 1u);
    TEST_ASSERT_EQUAL_STRING("a&lt;b&amp;c", buf);
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "&lt;",
                               .other_cap = 5u, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = buf, .cap = sizeof buf, .other = "&amp;",
                               .other_cap = 6u, .ci = MMGR_FALSE));
}

void test_an_empty_render_reports_nothing_written(void)
{
    char buf[32];
    const mmgr_fval fields[] = {MMGR_VSTR("")};

    TEST_ASSERT_EQUAL_size_t(
        0u, MMGR_CALL(numer.emit, NumerosCfg, .out = buf, .cap = sizeof buf, .vals = fields, .nvals = 1u));
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = buf, .cap = sizeof buf));
}
