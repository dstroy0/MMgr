/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "unity.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "numeros_scribo/numeros_scribo.h"

static const mmgr_fval s_none[1] = {MMGR_VU32(0u)};

void test_numer_header_is_self_contained(void)
{
    TEST_PASS_MESSAGE("numeros_scribo.h compiled with no header before it");
}

void test_numer_namespace_is_wired(void)
{
    const NumerosScriboNs *ns = &numer;
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(NumerosScriboNs), sizeof(*ns),
                                     "the namespace instance is not its own type");
}

void test_emit_a_literal(void)
{
    char b[32];
    const mmgr_fval fields[] = {MMGR_VSTR("hello")};

    TEST_ASSERT_EQUAL_size_t(
        5u, MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = fields, .nvals = 1u));
    TEST_ASSERT_EQUAL_STRING("hello", b);
}

void test_emit_mixes_kinds(void)
{
    char b[64];
    const mmgr_fval fields[] = {MMGR_VSTR("x="), MMGR_VU32(42u), MMGR_VCH(' '), MMGR_VI64(-7)};

    MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = fields, .nvals = 4u);
    TEST_ASSERT_EQUAL_STRING("x=42 -7", b);
}

void test_emit_carries_width(void)
{
    char b[64];
    const mmgr_fval fields[] = {MMGR_VSTR("addr="), MMGR_VHEXW(0xDEADu, 8)};

    MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = fields, .nvals = 2u);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("addr=0000dead", b, "the width rides on the value, not on a spec");
}

void test_emit_default_width_when_unstated(void)
{
    char b[64];
    const mmgr_fval fields[] = {MMGR_VHEX(0xDEADu)};

    MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = fields, .nvals = 1u);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("dead", b, "an unstated width means the kind's default, not zero");
}

void test_emit_takes_as_many_values_as_it_is_given(void)
{
    char b[64];
    const mmgr_fval fields[] = {MMGR_VCH('0'), MMGR_VCH('1'), MMGR_VCH('2'), MMGR_VCH('3'),
                                MMGR_VCH('4'), MMGR_VCH('5'), MMGR_VCH('6'), MMGR_VCH('7'),
                                MMGR_VCH('8'), MMGR_VCH('9'), MMGR_VCH('a'), MMGR_VCH('b'),
                                MMGR_VCH('c'), MMGR_VCH('d'), MMGR_VCH('e'), MMGR_VCH('f')};

    MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = fields,
              .nvals = sizeof fields / sizeof fields[0]);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("0123456789abcdef", b, "the count is the caller's, so it has no ceiling");
}

void test_emit_overflow_reports_and_terminates(void)
{
    char b[6];
    const mmgr_fval fields[] = {MMGR_VSTR("way too long")};

    TEST_ASSERT_EQUAL_size_t_MESSAGE(
        0u, MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = fields, .nvals = 1u),
        "overflow returns 0");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", b, "overflow leaves the buffer terminated, not half written");
}

void test_emit_append_builds_on_what_is_there(void)
{
    char b[32];
    const mmgr_fval head[] = {MMGR_VSTR("head")};
    const mmgr_fval tail[] = {MMGR_VSTR(":tail")};

    MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = head, .nvals = 1u);
    TEST_ASSERT_EQUAL_size_t(
        9u, MMGR_CALL(numer.emit_append, NumerosCfg, .out = b, .cap = sizeof b, .vals = tail, .nvals = 1u));
    TEST_ASSERT_EQUAL_STRING("head:tail", b);
}

void test_emit_escapes(void)
{
    char b[64];
    const mmgr_fval json[] = {MMGR_VJSON("a\"b")};
    const mmgr_fval xml[] = {MMGR_VXML("a<b")};

    MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = json, .nvals = 1u);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("\"a\\\"b\"", b, "json emits a whole JSON string, quotes included");

    MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = xml, .nvals = 1u);
    TEST_ASSERT_EQUAL_STRING("a&lt;b", b);
}

void test_emit_rejects_an_unknown_kind(void)
{
    char b[32];
    mmgr_fval bad = MMGR_VU32(1u);
    bad.kind = 200u;
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = &bad, .nvals = 1u));
    TEST_ASSERT_EQUAL_STRING("", b);
}

void test_emit_of_nothing_is_empty_not_garbage(void)
{
    char b[32];
    b[0] = 'x';
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.emit, NumerosCfg, .out = b, .cap = sizeof b, .vals = s_none, .nvals = 0u));
    TEST_ASSERT_EQUAL_STRING("", b);
}


void test_build_renders_a_spec(void)
{
    char out[64];
    static const mmgr_field spec[] = {{MMGR_FK_LIT, 0, 3, "id="}, MMGR_U32, MMGR_END};
    const mmgr_fval v[] = {MMGR_VU32(42u)};

    TEST_ASSERT_EQUAL_size_t(5u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = v, .nvals = 1u));
    TEST_ASSERT_EQUAL_STRING("id=42", out);
}

void test_build_rejects_a_value_of_the_wrong_kind(void)
{
    char out[64];
    static const mmgr_field spec[] = {MMGR_U32, MMGR_END};
    const mmgr_fval v[] = {MMGR_VSTR("not a number")};

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = v, .nvals = 1u), "kind mismatch is refused");
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_build_rejects_too_few_and_too_many_values(void)
{
    char out[64];
    static const mmgr_field spec[] = {MMGR_U32, MMGR_U32, MMGR_END};
    const mmgr_fval one[] = {MMGR_VU32(1u)};
    const mmgr_fval three[] = {MMGR_VU32(1u), MMGR_VU32(2u), MMGR_VU32(3u)};

    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = one, .nvals = 1u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = three, .nvals = 3u),
                                     "a leftover value is refused");
}

void test_build_guards_its_arguments(void)
{
                    char out[64];
    static const mmgr_field spec[] = {MMGR_U32, MMGR_END};
    const mmgr_fval v[] = {MMGR_VU32(1u)};

    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = (size_t)0, .spec = spec, .vals = v, .nvals = 1u));
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = v, .nvals = 0u),
                                     "a spec wanting a value gets none");
}

void test_build_of_a_spec_with_only_literals(void)
{
    char out[64];
    static const mmgr_field spec[] = {{MMGR_FK_LIT, 0, 5, "hello"}, {MMGR_FK_LIT, 0, 1, "!"}, MMGR_END};
    TEST_ASSERT_EQUAL_size_t(6u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = s_none, .nvals = 0u));
    TEST_ASSERT_EQUAL_STRING("hello!", out);
}

void test_build_covers_every_kind(void)
{
    char out[128];
    static const mmgr_field spec[] = {MMGR_STR, MMGR_U32, MMGR_U64, MMGR_I64, MMGR_CH, MMGR_JSON, MMGR_XML, MMGR_END};
    const mmgr_fval v[] = {MMGR_VSTR("s"), MMGR_VU32(1u),   MMGR_VU64(2u),   MMGR_VI64(-3),
                           MMGR_VCH('c'),  MMGR_VJSON("j"), MMGR_VXML("<x>")};

    TEST_ASSERT_GREATER_THAN_size_t(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = v, .nvals = 7u));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "\"j\"", .other_cap = 4u, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "&lt;x&gt;", .other_cap = 10u, .ci = MMGR_FALSE));
}

void test_build_covers_the_width_bearing_kinds(void)
{
    char out[128];
    static const mmgr_field spec[] = {{MMGR_FK_DEC, 4, 0, NULL}, {MMGR_FK_HEX, 4, 0, NULL}, {MMGR_FK_OCT, 0, 0, NULL},
                                      {MMGR_FK_G, 3, 0, NULL},   {MMGR_FK_FIX, 2, 0, NULL}, MMGR_END};
    const mmgr_fval v[] = {MMGR_VDEC(7u), MMGR_VHEX(0xABu), MMGR_VOCT(8u), MMGR_VG(1.25), MMGR_VFIX(2.5)};

    TEST_ASSERT_GREATER_THAN_size_t(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = v, .nvals = 5u));
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "0007", .other_cap = 5u, .ci = MMGR_FALSE), "DEC pads to its width");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "00ab", .other_cap = 5u, .ci = MMGR_FALSE), "HEX pads to its width");
    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "10", .other_cap = 3u, .ci = MMGR_FALSE), "OCT of 8 is 10");
}

void test_build_of_a_null_string_value(void)
{
    char out[64];
    static const mmgr_field spec[] = {MMGR_STR, MMGR_END};
    mmgr_fval v[1];
    v[0].kind = MMGR_FK_STR;
    v[0].as.text = NULL;
    v[0].width = 0;

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = v, .nvals = 1u), "a null string renders empty");
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_build_overflow_leaves_an_empty_buffer(void)
{
    char out[4];
    static const mmgr_field spec[] = {{MMGR_FK_LIT, 0, 20, "far too long for four"}, MMGR_END};
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = s_none, .nvals = 0u));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_build_rejects_an_unknown_kind(void)
{
    char out[64];
    static const mmgr_field spec[] = {{200u, 0, 0, NULL}, MMGR_END};
    mmgr_fval v[1];
    v[0].kind = 200u;
    v[0].width = 0;
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = v, .nvals = 1u));
}

void test_append_builds_on_what_is_there(void)
{
    char out[64];
    static const mmgr_field head[] = {{MMGR_FK_LIT, 0, 4, "head"}, MMGR_END};
    static const mmgr_field tail[] = {{MMGR_FK_LIT, 0, 5, ":tail"}, MMGR_END};

    TEST_ASSERT_EQUAL_size_t(4u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = head, .vals = s_none, .nvals = 0u));
    TEST_ASSERT_EQUAL_size_t(9u, MMGR_CALL(numer.append, NumerosCfg, .out = out, .cap = sizeof out, .spec = tail, .vals = s_none, .nvals = 0u));
    TEST_ASSERT_EQUAL_STRING("head:tail", out);
}

void test_append_guards_its_arguments(void)
{
            char out[64] = "x";
    static const mmgr_field spec[] = {{MMGR_FK_LIT, 0, 1, "y"}, MMGR_END};

    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.append, NumerosCfg, .out = out, .cap = (size_t)0, .spec = spec, .vals = s_none, .nvals = 0u));
}

void test_append_that_does_not_fit_leaves_the_original(void)
{
    char out[8] = "abcdefg";
    static const mmgr_field spec[] = {{MMGR_FK_LIT, 0, 10, "way too long"}, MMGR_END};
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.append, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = s_none, .nvals = 0u));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("abcdefg", out, "a failed append must not damage what was there");
}

void test_append_to_a_full_buffer(void)
{
    char out[4] = "abc";
    static const mmgr_field spec[] = {{MMGR_FK_LIT, 0, 1, "d"}, MMGR_END};
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.append, NumerosCfg, .out = out, .cap = 3u, .spec = spec, .vals = s_none, .nvals = 0u));
}

void test_emit_guards_its_arguments(void)
{
    char out[32];
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.emit, NumerosCfg, .out = out, .cap = (size_t)0, .vals = s_none, .nvals = 0u));
}

void test_emit_append_guards_its_arguments(void)
{
    char out[32] = "x";
    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.emit_append, NumerosCfg, .out = out, .cap = (size_t)0, .vals = s_none, .nvals = 0u));
}

void test_emit_covers_the_width_bearing_kinds(void)
{
    char out[128];
    const mmgr_fval fields[] = {MMGR_VOCTW(8u, 3), MMGR_VGW(1.25, 3), MMGR_VFIXW(2.5, 2), MMGR_VDECW(7u, 4)};

    TEST_ASSERT_GREATER_THAN_size_t(
        0u, MMGR_CALL(numer.emit, NumerosCfg, .out = out, .cap = sizeof out, .vals = fields, .nvals = 4u));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "010", .other_cap = 4u, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "0007", .other_cap = 5u, .ci = MMGR_FALSE));
}


void test_a_g_field_with_no_width_gets_six_digits(void)
{
    char spec_out[64];
    char emit_out[64];
    static const mmgr_field spec[] = {{MMGR_FK_G, 0, 0, NULL}, MMGR_END};
    const mmgr_fval v[] = {MMGR_VG(1.0 / 3.0)};

    TEST_ASSERT_GREATER_THAN_size_t(0u, MMGR_CALL(numer.build, NumerosCfg, .out = spec_out, .cap = sizeof spec_out, .spec = spec, .vals = v, .nvals = 1u));
    TEST_ASSERT_GREATER_THAN_size_t(
        0u, MMGR_CALL(numer.emit, NumerosCfg, .out = emit_out, .cap = sizeof emit_out, .vals = v, .nvals = 1u));

    TEST_ASSERT_EQUAL_STRING_MESSAGE(spec_out, emit_out, "the spec path and the variadic path disagree on the default");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = spec_out, .cap = sizeof spec_out),
                                     "no width asked for is six significant digits");
}

void test_a_hex_field_with_no_width_gets_one_digit(void)
{
    char out[64];
    static const mmgr_field spec[] = {{MMGR_FK_HEX, 0, 0, NULL}, MMGR_END};
    const mmgr_fval v[] = {MMGR_VHEX(0u)};

    TEST_ASSERT_EQUAL_size_t(1u, MMGR_CALL(numer.build, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = v, .nvals = 1u));
    TEST_ASSERT_EQUAL_STRING("0", out);
}

void test_an_oct_field_with_no_width_gets_one_digit(void)
{
    char out[64];
    const mmgr_fval fields[] = {MMGR_VOCT(0u)};

    TEST_ASSERT_EQUAL_size_t(
        1u, MMGR_CALL(numer.emit, NumerosCfg, .out = out, .cap = sizeof out, .vals = fields, .nvals = 1u));
    TEST_ASSERT_EQUAL_STRING("0", out);
}

void test_append_to_a_buffer_with_no_terminator_is_refused(void)
{
            char out[4];
    for (unsigned i = 0; i < sizeof out; i++)
    {
        out[i] = 'x';
    }

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, MMGR_CALL(numer.emit_append, NumerosCfg, .out = out, .cap = sizeof out, .vals = s_none, .nvals = 0u),
                                     "there is no room after a buffer that is already full");
}

void test_an_append_that_does_not_fit_puts_the_terminator_back(void)
{
    char out[8] = "abc";
    static const mmgr_field spec[] = {{MMGR_FK_LIT, 0, 12, "far too long"}, MMGR_END};

    TEST_ASSERT_EQUAL_size_t(0u, MMGR_CALL(numer.append, NumerosCfg, .out = out, .cap = sizeof out, .spec = spec, .vals = s_none, .nvals = 0u));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("abc", out, "a failed append left the buffer without its terminator");
}


void test_the_emit_path_carries_every_kind(void)
{
    char out[192];
    const mmgr_fval fields[] = {MMGR_VSTR("s"),    MMGR_VU32(1u),     MMGR_VU64(2u),  MMGR_VI64(-3),
                                MMGR_VCH('c'),     MMGR_VJSON("j"),   MMGR_VXML("<x>"), MMGR_VHEX(0xABu),
                                MMGR_VDEC(7u),     MMGR_VG(1.25),     MMGR_VFIX(2.5)};

    TEST_ASSERT_GREATER_THAN_size_t(0u, MMGR_CALL(numer.emit, NumerosCfg, .out = out, .cap = sizeof out,
                                                  .vals = fields,
                                                  .nvals = sizeof fields / sizeof fields[0]));

    TEST_ASSERT_TRUE_MESSAGE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "2", .other_cap = 2u, .ci = MMGR_FALSE), "the 64 bit value is missing");
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "-3", .other_cap = 3u, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "\"j\"", .other_cap = 4u, .ci = MMGR_FALSE));
    TEST_ASSERT_TRUE(MMGR_CALL(cellul.has, CatenaFinitaCfg, .src = out, .cap = sizeof out, .other = "&lt;x&gt;", .other_cap = 10u, .ci = MMGR_FALSE));
}

void test_a_u64_of_its_largest_value(void)
{
    char out[64];
    const mmgr_fval fields[] = {MMGR_VU64(18446744073709551615ull)};

    TEST_ASSERT_EQUAL_size_t(
        20u, MMGR_CALL(numer.emit, NumerosCfg, .out = out, .cap = sizeof out, .vals = fields, .nvals = 1u));
    TEST_ASSERT_EQUAL_STRING("18446744073709551615", out);
}

void test_an_emit_append_that_does_not_fit_puts_the_terminator_back(void)
{
    char out[8] = "abc";
    const mmgr_fval fields[] = {MMGR_VSTR("far too long for this")};

    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u,
                                     MMGR_CALL(numer.emit_append, NumerosCfg, .out = out, .cap = sizeof out,
                                               .vals = fields, .nvals = 1u),
                                     "an append that does not fit still reported a length");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("abc", out, "a failed append left the buffer without its terminator");
}
