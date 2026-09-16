// MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
#include "memoria_anularis/memoria_anularis.h"

#include "unity.h"

#include <string.h>

#define CAP 512u
#define SEGS 2u

#define POISON 0xCCu

static EMBED_ALIGN(MMGR_ALIGN_BYTES) uint8_t ring_buf[CAP];
static mmgr_ring ring;

static uint8_t out[4096];
static uint8_t padded[4096];

static uint32_t g_state;
static uint32_t g_esc;
static uint8_t *g_dest;
static int g_done;

void setUp(void)
{
    memset(ring_buf, POISON, sizeof ring_buf);
    memset(out, POISON, sizeof out);
    (void)anularis.init(&(AnularisCfg){.ring = &ring, .buf = ring_buf, .capacity = CAP, .segment_count = SEGS});
    g_state = 1u;
    g_esc = 0u;
    g_dest = out;
    g_done = 0;
}

void tearDown(void)
{
}

static size_t ingest(const uint8_t *src, size_t n)
{
    const size_t room = anularis.vacant(&(AnularisCfg){.ring = &ring});
    const size_t bytes = (n < room) ? n : room;

    if (bytes == 0u)
    {
        return 0u;
    }

    if (!anularis.put(&(AnularisCfg){.ring = &ring, .src = src, .bytes = bytes}))
    {
        return 0u;
    }
    return bytes;
}

static size_t avail(void)
{
    return anularis.available(&(AnularisCfg){.ring = &ring});
}

static uint8_t at(size_t off)
{
    uint8_t b = 0;
    anularis.peek(&(AnularisCfg){.ring = &ring, .dst = &b, .bytes = 1u, .offset = off});
    return b;
}

static void eat(size_t n)
{
    anularis.consume(&(AnularisCfg){.ring = &ring, .bytes = n});
}

static size_t lex(uint8_t curr, uint8_t next)
{
    const uint32_t s = g_state;
    const uint32_t esc = g_esc;

    const uint32_t n_mode = s & 1u;
    const uint32_t s_mode = (s >> 1) & 1u;
    const uint32_t c_mode = (s >> 2) & 1u;
    const uint32_t l_mode = (s >> 3) & 1u;
    const uint32_t b_mode = (s >> 4) & 1u;

    const uint32_t quote = (curr == '"') & (esc ^ 1u);
    const uint32_t tick = (curr == '\'') & (esc ^ 1u);
    g_esc = (curr == '\\') & (esc ^ 1u) & (s_mode | c_mode);

    const uint32_t to_b = n_mode & (curr == '/') & (next == '*');
    const uint32_t to_l = n_mode & (curr == '/') & (next == '/');
    const uint32_t to_s = n_mode & quote;
    const uint32_t to_c = n_mode & tick;

    const uint32_t ex_b = b_mode & (curr == '*') & (next == '/');
    const uint32_t ex_l = l_mode & (curr == '\n');
    const uint32_t ex_s = s_mode & quote;
    const uint32_t ex_c = c_mode & tick;

    const uint32_t write = (n_mode | s_mode | c_mode | ex_l) & (!to_b & !to_l & !ex_b);

    const uint32_t clear =
        ~((to_b | to_l | to_s | to_c)
              ? s
              : (ex_b ? (1u << 4) : (ex_l ? (1u << 3) : (ex_s ? (1u << 1) : (ex_c ? (1u << 2) : 0u)))));

    const uint32_t set = (to_b << 4) | (to_l << 3) | (to_s << 1) | (to_c << 2) | ((ex_b | ex_l | ex_s | ex_c) << 0);

    g_state = (s & clear) | set;

    if (write)
    {
        *g_dest++ = curr;
    }
    return 1u + (to_b | to_l | ex_b);
}

static void drain(int live)
{
    while (!g_done)
    {
        const size_t have = avail();

        if ((have == 0u) || (live && (have < 2u)))
        {
            return;
        }

        const uint8_t curr = at(0);
        if (curr == 0u)
        {
            *g_dest = 0u;
            g_done = 1;
            eat(1u);
            return;
        }
        eat(lex(curr, (have >= 2u) ? at(1) : 0u));
    }
}

static void run(const char *src, size_t chunk)
{
    const size_t n = strlen(src) + 1u;

    memset(padded, 0, sizeof padded);
    memcpy(padded, src, n);

    size_t sent = 0;
    while ((sent < n) && !g_done)
    {
        size_t want = n - sent;
        if (want > chunk)
        {
            want = chunk;
        }

        const size_t got = ingest(padded + sent, want);
        if (got == 0u)
        {
            const size_t before = avail();
            drain(1);
            if (avail() == before)
            {
                break;
            }
            continue;
        }
        sent += got;
        drain(sent < n);
    }
    drain(0);
}

static void expect(const char *src, const char *want)
{
    static const size_t chunks[] = {1u, 7u, 64u, 256u, 4096u};

    for (size_t i = 0; i < sizeof chunks / sizeof chunks[0]; i++)
    {
        setUp();
        run(src, chunks[i]);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(want, (const char *)out, src);
    }
}

void test_plain_code_arrives_unchanged(void)
{
    expect("int x = 1;\n", "int x = 1;\n");
    expect("aaaabbbbccccddddeeee\n", "aaaabbbbccccddddeeee\n");
    expect("a/b;\n", "a/b;\n");
    expect("a*b;\n", "a*b;\n");
}

void test_comments_are_dropped(void)
{
    expect("a;// gone\nb;\n", "a;\nb;\n");
    expect("a;/* gone */b;\n", "a;b;\n");
    expect("a;/* x\ny */b;\n", "a;b;\n");
    expect("a;/*x*//*y*/b;\n", "a;b;\n");
    expect("a;/**/b;\n", "a;b;\n");
    expect("a/b;/*c*/d;\n", "a/b;d;\n");
    expect("/*x*/", "");
}

void test_a_comment_that_never_closes_ends_at_the_terminator(void)
{
    expect("a;//x", "a;");
    expect("a;/*x", "a;");
}

void test_literals_survive_intact(void)
{
    expect("s = \"hello\";\n", "s = \"hello\";\n");
    expect("c = 'x';\n", "c = 'x';\n");
    expect("s = \"a\\\"b\";\n", "s = \"a\\\"b\";\n");
    expect("c = '\\'';\n", "c = '\\'';\n");
    expect("c = '\\\\';\n", "c = '\\\\';\n");
}

void test_a_token_inside_a_literal_is_not_a_token(void)
{
    expect("s = \"a/*b\";\n", "s = \"a/*b\";\n");
    expect("s = \"a//b\";\n", "s = \"a//b\";\n");
    expect("s = \"*/\";\n", "s = \"*/\";\n");
    expect("a;/* \" */b;\n", "a;b;\n");
}

void test_a_token_at_every_offset_in_the_word(void)
{

    char src[64];
    char want[64];

    for (unsigned off = 0; off < 16u; off++)
    {
        unsigned k = 0;
        unsigned w = 0;

        for (unsigned i = 0; i < off; i++)
        {
            src[k++] = 'a';
            want[w++] = 'a';
        }
        src[k++] = '/';
        src[k++] = '*';
        src[k++] = 'X';
        src[k++] = '*';
        src[k++] = '/';
        for (unsigned i = 0; i < 8u; i++)
        {
            src[k++] = 'b';
            want[w++] = 'b';
        }
        src[k] = '\0';
        want[w] = '\0';

        expect(src, want);
    }
}

void test_more_than_the_ring_holds(void)
{

    static char src[4000];
    static char want[4000];
    unsigned k = 0;
    unsigned w = 0;

    for (unsigned i = 0; i < 300u; i++)
    {
        src[k++] = 'a';
        src[k++] = 'b';
        want[w++] = 'a';
        want[w++] = 'b';
        src[k++] = '/';
        src[k++] = '*';
        src[k++] = 'z';
        src[k++] = '*';
        src[k++] = '/';
    }
    src[k] = '\0';
    want[w] = '\0';

    TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(CAP, k, "the case has to be longer than the ring to mean anything");
    expect(src, want);
}

void test_nothing_the_channel_did_not_send_reaches_the_output(void)
{

    expect("ab", "ab");

    for (size_t i = 0; i < sizeof out; i++)
    {
        if (out[i] == POISON)
        {
            break;
        }
        TEST_ASSERT_TRUE_MESSAGE(out[i] == 'a' || out[i] == 'b' || out[i] == '\0', "ring poison reached the output");
    }
}
