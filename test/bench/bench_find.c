// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// How many sieve rows before verifying, and does it beat libc.
//
// One load per step. Row k is eq(w, needle[k]) shifted back by k lanes, so all K rows come out of
// the same word and stack on top of each other; AND them and the surviving lanes are the candidate
// offsets. Each row cuts candidates by roughly the byte's frequency, so two rows over text leave
// almost nothing to verify.
//
// Shifting right by k lanes drops the top k lanes' answers, because those candidates need bytes
// from the next word. The step is WORD - K + 1 rather than WORD, so the next iteration covers them.

#include <stdio.h>
#include <string.h>

#include "bench_harness.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "verbum_scrutor/verbum_scrutor.h"

#define HAY_BYTES 16384u
#define ITERS 100000ul

static char g_hay[HAY_BYTES + 64u];
static char g_needle[64];
static size_t g_nlen;

static int full_match(const char *p, const char *needle, size_t nlen)
{
    for (size_t i = 0; i < nlen; i++)
    {
        if (p[i] != needle[i])
        {
            return 0;
        }
    }
    return 1;
}

#define DEFINE_SIEVE(ROWS)                                                                                             \
    static const char *find_sieve##ROWS(const char *hay, size_t n, const char *needle, size_t nlen)                    \
    {                                                                                                                  \
        if (nlen == 0u)                                                                                                \
        {                                                                                                              \
            return hay;                                                                                                \
        }                                                                                                              \
        const size_t rows = (nlen < (ROWS)) ? nlen : (size_t)(ROWS);                                                   \
        const size_t step = MMGR_SWAR_BYTES - rows + 1u;                                                               \
        /* The broadcast is loop-invariant: one imul per row, here, not per row per word. */                           \
        mmgr_scrut_word bc[(ROWS)];                                                                                    \
        for (size_t k = 0; k < rows; k++)                                                                              \
        {                                                                                                              \
            bc[k] = (mmgr_scrut_word)(MMGR_SWAR_ONES * (mmgr_scrut_word)(uint8_t)needle[k]);                           \
        }                                                                                                              \
        size_t i = 0;                                                                                                  \
        while (i + MMGR_SWAR_BYTES <= n)                                                                               \
        {                                                                                                              \
            const mmgr_scrut_word w = mmgr_scrut_load(hay + i);                                                        \
            mmgr_scrut_word m = mmgr_scrut_has_zero(w ^ bc[0]);                                                        \
            for (size_t k = 1; k < rows; k++)                                                                          \
            {                                                                                                          \
                m &= (mmgr_scrut_word)(mmgr_scrut_has_zero(w ^ bc[k]) >> (k * 8u));                                    \
            }                                                                                                          \
            while (m != 0u)                                                                                            \
            {                                                                                                          \
                const size_t lane = mmgr_scrut_zero_lane(m);                                                           \
                if (lane >= step)                                                                                      \
                {                                                                                                      \
                    break;                                                                                             \
                }                                                                                                      \
                if (i + lane + nlen <= n && full_match(hay + i + lane, needle, nlen))                                  \
                {                                                                                                      \
                    return hay + i + lane;                                                                             \
                }                                                                                                      \
                m &= (mmgr_scrut_word)(m - 1u);                                                                        \
            }                                                                                                          \
            i += step;                                                                                                 \
        }                                                                                                              \
        for (; i + nlen <= n; i++)                                                                                     \
        {                                                                                                              \
            if (full_match(hay + i, needle, nlen))                                                                     \
            {                                                                                                          \
                return hay + i;                                                                                        \
            }                                                                                                          \
        }                                                                                                              \
        return NULL;                                                                                                   \
    }

DEFINE_SIEVE(1)
DEFINE_SIEVE(2)
DEFINE_SIEVE(3)
DEFINE_SIEVE(4)

// English-like prose. Real text is what decides how much a second row buys: letter frequencies are
// what the sieve is sieving, and a haystack of one repeated byte answers a different question.
static const char *CORPUS = "the quick brown fox jumps over the lazy dog while the rain in spain "
                            "falls mainly on the plain and the cat sat on the mat with a hat that "
                            "was rather flat but the dog did not care for hats at all it seems ";

static void plant(const char *needle, size_t at)
{
    const size_t clen = strlen(CORPUS);
    for (size_t i = 0; i < sizeof g_hay; i++)
    {
        g_hay[i] = CORPUS[i % clen];
    }
    g_nlen = strlen(needle);
    memcpy(g_needle, needle, g_nlen + 1u);
    // Overwrite any natural occurrence before `at` so the match position is the one asked for.
    for (size_t i = 0; i + g_nlen < at; i++)
    {
        if (memcmp(g_hay + i, g_needle, g_nlen) == 0)
        {
            g_hay[i] = '#';
        }
    }
    memcpy(g_hay + at, g_needle, g_nlen);
}

#define ROW(LABEL, EXPR)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        double cy_ = 0.0;                                                                                              \
        BENCH_TIME_CYCLES(cy_, ITERS, {                                                                                \
            const size_t o_ = (size_t)(bench_i_ & 7u);                                                                 \
            const void *r_ = (EXPR);                                                                                   \
            BENCH_KEEP(r_);                                                                                            \
        });                                                                                                            \
        printf("find,%s,%s,%u,%u,%.0f,%.3f\n", LABEL, g_needle, (unsigned)g_nlen, MMGR_SWAR_BITS, cy_,                 \
               cy_ / (double)HAY_BYTES);                                                                               \
        fflush(stdout);                                                                                                \
    } while (0)

static void sweep(const char *needle, size_t at)
{
    plant(needle, at);
    const size_t n = HAY_BYTES;

    ROW("libc_strstr", strstr(g_hay + o_, g_needle));
    ROW("cellul_find", cellul.find(g_hay + o_, n - o_, g_needle, g_nlen + 1u, MMGR_FALSE));
    ROW("sieve1", find_sieve1(g_hay + o_, n - o_, g_needle, g_nlen));
    ROW("sieve2", find_sieve2(g_hay + o_, n - o_, g_needle, g_nlen));
    ROW("sieve3", find_sieve3(g_hay + o_, n - o_, g_needle, g_nlen));
    ROW("sieve4", find_sieve4(g_hay + o_, n - o_, g_needle, g_nlen));
}

int main(void)
{
    printf("bench,impl,needle,needle_len,lane_bits,cycles,cycles_per_byte\n");
    fflush(stdout);

    sweep("zqx", 12000u); // first byte is rare: row 0 alone nearly settles it
    sweep("the", 12000u); // first byte is common: row 0 fires constantly
    sweep("plain", 12000u);
    sweep("mainly on the", 12000u);

    return 0;
}
