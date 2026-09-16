#include "unity.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "verbum_scrutor/verbum_scrutor.h"

#include <stdio.h>
#include <string.h>

enum
{
    MAXLEN = 300,
    ROOM = MAXLEN + 64
};

static char pool_a[ROOM];
static char pool_b[ROOM];

void setUp(void)
{
}

void tearDown(void)
{
}

static size_t ref_len(const char *s, size_t cap)
{
    size_t n = 0;
    while (n < cap && s[n] != '\0')
    {
        n++;
    }
    return n;
}

static const char *ref_chr(const char *s, size_t cap, unsigned char c)
{
    for (size_t i = 0; i < cap; i++)
    {
        if ((unsigned char)s[i] == c)
        {
            return s + i;
        }
        if (s[i] == '\0')
        {
            return NULL;
        }
    }
    return NULL;
}

static unsigned char fold(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c | 0x20u) : c;
}

static size_t ref_diff(const char *a, const char *b, size_t cap, int ci)
{
    for (size_t i = 0; i < cap; i++)
    {
        const unsigned char x = (unsigned char)a[i];
        const unsigned char y = (unsigned char)b[i];

        if (ci ? (fold(x) != fold(y)) : (x != y))
        {
            return i;
        }
    }
    return cap;
}

static const char *ref_find(const char *hay, size_t cap, const char *needle, size_t nlen, int ci)
{
    if (nlen == 0u)
    {
        return hay;
    }
    if (nlen > cap)
    {
        return NULL;
    }
    for (size_t k = 0; k + nlen <= cap; k++)
    {
        if (hay[k] == '\0')
        {
            return NULL;
        }
        size_t i = 0;
        while (i < nlen)
        {
            const unsigned char h = (unsigned char)hay[k + i];
            const unsigned char n = (unsigned char)needle[i];

            if (h == 0u || (ci ? (fold(h) != fold(n)) : (h != n)))
            {
                break;
            }
            i++;
        }
        if (i == nlen)
        {
            return hay + k;
        }
    }
    return NULL;
}

static uint64_t seed = 0x243F6A8885A308D3ull;

static uint64_t rnd(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}

static void fill(char *p, size_t len, unsigned which)
{
    static const char *sets[] = {"ab", "abc", "aA", "eE", "abcdefghijklmnop", "\x01\x7f\x80\xff"};
    const char *set = sets[which % 6u];
    const size_t n = strlen(set);

    for (size_t i = 0; i < len; i++)
    {
        p[i] = set[rnd() % n];
    }
    p[len] = '\0';
}

static size_t nth_len(unsigned i)
{
    static const size_t big[] = {63, 64, 65, 127, 128, 129, 255, 256, 257, 299};
    return (i < 48u) ? i : big[(i - 48u) % 10u];
}
#define LENS 58u

void test_the_answer_does_not_depend_on_where_the_buffer_starts(void)
{
    static const size_t lens[] = {1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 65};

    for (unsigned li = 0; li < sizeof lens / sizeof lens[0]; li++)
    {
        const size_t len = lens[li];
        size_t first_len = 0;
        ptrdiff_t first_chr = 0;
        ptrdiff_t first_find = 0;

        for (unsigned a = 0; a < 2u * MMGR_SWAR_BYTES; a++)
        {
            char *p = pool_a + a;

            seed = 0x9E3779B97F4A7C15ull + len;
            fill(p, len, 4u);
            if (len >= 3u)
            {
                memcpy(p + len - 3u, "xyz", 3u);
            }

            const size_t l = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = p, .cap = len + 1u);
            const char *c = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = p, .cap = len + 1u, .byte = (uint8_t)'z');
            const char *f = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = p, .cap = len, .other = "xyz",
                                       .other_cap = 3u, .ci = EMBED_FALSE);

            const ptrdiff_t co = c ? c - p : -1;
            const ptrdiff_t fo = f ? f - p : -1;

            if (a == 0u)
            {
                first_len = l;
                first_chr = co;
                first_find = fo;
            }
            else
            {
                char msg[112];
                (void)snprintf(msg, sizeof msg, "length %zu answered differently at offset %u", len, a);
                TEST_ASSERT_EQUAL_size_t_MESSAGE(first_len, l, msg);
                TEST_ASSERT_EQUAL_INT_MESSAGE((int)first_chr, (int)co, msg);
                TEST_ASSERT_EQUAL_INT_MESSAGE((int)first_find, (int)fo, msg);
            }
        }
    }
}

void test_len_matches_the_reference_at_every_length(void)
{
    for (unsigned li = 0; li < LENS; li++)
    {
        const size_t len = nth_len(li);
        fill(pool_a, len, li);

        const size_t cap = len + 1u;
        char msg[80];
        (void)snprintf(msg, sizeof msg, "len at length %zu", len);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(ref_len(pool_a, cap),
                                         EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = pool_a, .cap = cap), msg);
    }
}

void test_len_stops_at_the_cap_at_every_length(void)
{
    memset(pool_a, 'x', MAXLEN);
    pool_a[MAXLEN] = '\0';

    for (size_t cap = 0; cap <= 72u; cap++)
    {
        char msg[80];
        (void)snprintf(msg, sizeof msg, "len with cap %zu", cap);
        TEST_ASSERT_EQUAL_size_t_MESSAGE(cap, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = pool_a, .cap = cap), msg);
    }
}

void test_chr_matches_the_reference_at_every_length(void)
{
    for (unsigned li = 0; li < LENS; li++)
    {
        const size_t len = nth_len(li);
        fill(pool_a, len, li);

        static const unsigned char wanted[] = {'a', 'z', 'b'};
        for (unsigned w = 0; w < 3u; w++)
        {
            const size_t cap = len + 1u;
            char msg[96];
            (void)snprintf(msg, sizeof msg, "chr 0x%02x at length %zu", wanted[w], len);
            TEST_ASSERT_EQUAL_PTR_MESSAGE(
                ref_chr(pool_a, cap, wanted[w]),
                EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = pool_a, .cap = cap, .byte = wanted[w]), msg);
        }
    }
}

void test_diff_matches_the_reference_at_every_length(void)
{
    for (unsigned li = 0; li < LENS; li++)
    {
        const size_t len = nth_len(li);
        fill(pool_a, len, li);
        memcpy(pool_b, pool_a, len + 1u);
        if (len > 0u && (li & 1u) != 0u)
        {
            pool_b[rnd() % len] ^= 0x20u;
        }

        for (int ci = 0; ci <= 1; ci++)
        {
            char msg[96];
            (void)snprintf(msg, sizeof msg, "diff at length %zu ci %d", len, ci);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(ref_diff(pool_a, pool_b, len, ci),
                                             EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = pool_a, .other = pool_b,
                                                        .cap = len, .ci = ci ? EMBED_TRUE : EMBED_FALSE),
                                             msg);
        }
    }
}

void test_find_matches_the_reference_at_every_needle_and_hay_length(void)
{
    for (size_t nlen = 1u; nlen <= 2u * MMGR_SWAR_BYTES; nlen++)
    {
        for (unsigned li = 4u; li < 32u; li++)
        {
            const size_t hlen = nth_len(li);
            if (nlen > hlen)
            {
                continue;
            }
            fill(pool_a, hlen, li);
            fill(pool_b, nlen, li + 3u);

            if ((li & 1u) != 0u)
            {
                memcpy(pool_a + (rnd() % (hlen - nlen + 1u)), pool_b, nlen);
            }

            for (int ci = 0; ci <= 1; ci++)
            {
                const embed_bool f = ci ? EMBED_TRUE : EMBED_FALSE;
                char msg[112];
                (void)snprintf(msg, sizeof msg, "find hay %zu needle %zu ci %d", hlen, nlen, ci);
                TEST_ASSERT_EQUAL_PTR_MESSAGE(ref_find(pool_a, hlen, pool_b, nlen, ci),
                                              EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = pool_a, .cap = hlen,
                                                         .other = pool_b, .other_cap = nlen, .ci = f),
                                              msg);
            }
        }
    }
}

void test_find_matches_the_reference_with_the_match_at_every_position(void)
{
    const size_t hlen = 96u;
    const size_t nlen = 5u;

    for (size_t at = 0; at + nlen <= hlen; at++)
    {
        memset(pool_a, '.', hlen);
        pool_a[hlen] = '\0';
        memcpy(pool_b, "xyzzy", nlen + 1u);
        memcpy(pool_a + at, pool_b, nlen);

        char msg[80];
        (void)snprintf(msg, sizeof msg, "find with the match at %zu", at);
        TEST_ASSERT_EQUAL_PTR_MESSAGE(ref_find(pool_a, hlen, pool_b, nlen, 0),
                                      EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = pool_a, .cap = hlen,
                                                 .other = pool_b, .other_cap = nlen, .ci = EMBED_FALSE),
                                      msg);
    }
}
