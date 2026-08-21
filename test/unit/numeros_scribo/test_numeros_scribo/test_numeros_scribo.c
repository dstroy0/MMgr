// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include "unity.h"

#include "numeros_scribo/numeros_scribo.h"

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

void test_write_literal(void)
{
    char b[32];
    TEST_ASSERT_EQUAL_size_t(5u, mmgr_write(b, sizeof b, MMGR_VSTR("hello")));
    TEST_ASSERT_EQUAL_STRING("hello", b);
}

void test_write_mixes_kinds(void)
{
    char b[64];
    mmgr_write(b, sizeof b, MMGR_VSTR("x="), MMGR_VU32(42u), MMGR_VCH(' '), MMGR_VI64(-7));
    TEST_ASSERT_EQUAL_STRING("x=42 -7", b);
}

void test_write_carries_width(void)
{
    char b[64];
    mmgr_write(b, sizeof b, MMGR_VSTR("addr="), MMGR_VHEXW(0xDEADu, 8));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("addr=0000dead", b, "the width rides on the value, not on a spec");
}

void test_write_default_width_when_unstated(void)
{
    char b[64];
    mmgr_write(b, sizeof b, MMGR_VHEX(0xDEADu));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("dead", b, "an unstated width means the kind's default, not zero");
}

void test_write_counts_its_own_arguments(void)
{
    char b[64];
    mmgr_write(b, sizeof b, MMGR_VCH('0'), MMGR_VCH('1'), MMGR_VCH('2'), MMGR_VCH('3'), MMGR_VCH('4'), MMGR_VCH('5'),
               MMGR_VCH('6'), MMGR_VCH('7'), MMGR_VCH('8'), MMGR_VCH('9'), MMGR_VCH('a'), MMGR_VCH('b'), MMGR_VCH('c'),
               MMGR_VCH('d'), MMGR_VCH('e'), MMGR_VCH('f'));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("0123456789abcdef", b, "the count is sizeof over sizeof, so it has no ceiling");
}

void test_write_overflow_reports_and_terminates(void)
{
    char b[6];
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, mmgr_write(b, sizeof b, MMGR_VSTR("way too long")), "overflow returns 0");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", b, "overflow leaves the buffer terminated, not half written");
}

void test_write_append_builds_on_what_is_there(void)
{
    char b[32];
    mmgr_write(b, sizeof b, MMGR_VSTR("head"));
    TEST_ASSERT_EQUAL_size_t(9u, mmgr_write_append(b, sizeof b, MMGR_VSTR(":tail")));
    TEST_ASSERT_EQUAL_STRING("head:tail", b);
}

void test_write_escapes(void)
{
    char b[64];
    mmgr_write(b, sizeof b, MMGR_VJSON("a\"b"));
    TEST_ASSERT_EQUAL_STRING_MESSAGE("\"a\\\"b\"", b, "json emits a whole JSON string, quotes included");

    mmgr_write(b, sizeof b, MMGR_VXML("a<b"));
    TEST_ASSERT_EQUAL_STRING("a&lt;b", b);
}

void test_write_matches_the_namespace_entry(void)
{
    char viamacro[64];
    char vians[64];
    const mmgr_fval v[] = {MMGR_VSTR("n="), MMGR_VU32(9u)};

    mmgr_write(viamacro, sizeof viamacro, MMGR_VSTR("n="), MMGR_VU32(9u));
    numer.emit(vians, sizeof vians, v, sizeof v / sizeof v[0]);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(vians, viamacro, "the macro is the array, spelled shorter");
}

void test_emit_rejects_an_unknown_kind(void)
{
    char b[32];
    mmgr_fval bad = MMGR_VU32(1u);
    bad.kind = 200u;
    TEST_ASSERT_EQUAL_size_t(0u, numer.emit(b, sizeof b, &bad, 1u));
    TEST_ASSERT_EQUAL_STRING("", b);
}

void test_emit_of_nothing_is_empty_not_garbage(void)
{
    char b[32];
    b[0] = 'x';
    TEST_ASSERT_EQUAL_size_t(0u, numer.emit(b, sizeof b, NULL, 0u));
    TEST_ASSERT_EQUAL_STRING("", b);
}
