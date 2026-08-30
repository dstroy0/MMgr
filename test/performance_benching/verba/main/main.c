#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device_bench.h"

#include "clz/clz.h"
#include "fractio/fractio.h"
#include "memoria_operor/memoria_operor.h"
#include "numeros_scribo/numeros_scribo.h"
#include "proximus_operor/proximus_operor.h"
#include "transformo/transformo.h"
#include "verba_scribo/verba_scribo.h"

static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_out[16];

static MMGR_ALIGN(MMGR_ALIGN_BYTES) char g_wide[64];

static volatile size_t g_len = 30u;

static volatile size_t g_len_whole = 32u;

static const char *volatile g_text = "the quick brown fox jumps over";

static volatile uint64_t g_rec_u64 = 18446744073709551615ull;
static volatile uint32_t g_rec_hex = 0xDEADBEEFu;
static volatile double g_rec_real = -2.5;

static volatile uint64_t g_rec_bits = 0xC004000000000000ull;

static volatile size_t g_zero_n = 6u;

static volatile double g_rec_real2 = 3.14159265358979;

static volatile double g_rec_small = 0.000123456789012345;
static volatile uint64_t g_fix_ip = 3u;
static volatile uint64_t g_fix_frac = 141593u;

static uint64_t g_fix_rem = 0x121FB54442D18ull;

static uint64_t g_fix_zero = 0u;

static uint32_t g_pow_lo_only = 0u;

static volatile uint64_t g_g_mant = 31415926535897932ull;

static volatile uint64_t g_g_round = 20000000000000000ull;

static const uint32_t POW10[10] = {1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u, 1000000000u};

static const char PAIRS[201] = "00010203040506070809"
                               "10111213141516171819"
                               "20212223242526272829"
                               "30313233343536373839"
                               "40414243444546474849"
                               "50515253545556575859"
                               "60616263646566676869"
                               "70717273747576777879"
                               "80818283848586878889"
                               "90919293949596979899";

static unsigned dc_scan(uint32_t v)
{
    unsigned digits = 1u;

    while ((digits <= 9u) && (v >= POW10[digits]))
    {
        digits++;
    }
    return digits;
}

static unsigned dc_clz(uint32_t v)
{

    const unsigned lead = (unsigned)MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_u64)(v | 1u));
    const unsigned bits = 64u - lead;
    const unsigned est = ((bits * 1233u) >> 12u);

    return est + (((v | 1u) >= POW10[est]) ? 1u : 0u);
}

static void emit_one(char *out, uint32_t v, unsigned digits)
{
    for (unsigned i = digits; i-- > 0u;)
    {
        out[i] = (char)('0' + (v % 10u));
        v /= 10u;
    }
}

static void emit_pair(char *out, uint32_t v, unsigned digits)
{
    unsigned i = digits;

    while (i >= 2u)
    {
        const uint32_t q = v / 100u;
        const uint32_t r = v - (q * 100u);

        i -= 2u;
        out[i] = PAIRS[r * 2u];
        out[i + 1u] = PAIRS[(r * 2u) + 1u];
        v = q;
    }
    if (i != 0u)
    {
        out[0] = (char)('0' + v);
    }
}

static inline uint32_t div100(uint32_t v)
{

    return (uint32_t)(((uint64_t)v * 0x51EB851FULL) >> 37u);
}

static void emit_recip(char *out, uint32_t v, unsigned digits)
{
    unsigned i = digits;

    while (i >= 2u)
    {
        const uint32_t q = div100(v);
        const uint32_t r = v - (q * 100u);

        i -= 2u;
        out[i] = PAIRS[r * 2u];
        out[i + 1u] = PAIRS[(r * 2u) + 1u];
        v = q;
    }
    if (i != 0u)
    {
        out[0] = (char)('0' + v);
    }
}

#define JEAIII_PAIR(out_, n_)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        const uint32_t d_ = (uint32_t)((n_) >> 32u);                                                                   \
        (out_)[0] = PAIRS[d_ * 2u];                                                                                    \
        (out_)[1] = PAIRS[(d_ * 2u) + 1u];                                                                             \
        (n_) = ((n_) & 0xFFFFFFFFULL) * 100ULL;                                                                        \
    } while (0)

static void emit_jeaiii(char *out, uint32_t v, unsigned digits)
{
    char tmp[12];
    unsigned wide;
    uint64_t n;

    if (digits <= 2u)
    {
        n = 0u;
        wide = 2u;
        tmp[0] = PAIRS[v * 2u];
        tmp[1] = PAIRS[(v * 2u) + 1u];
    }
    else if (digits <= 4u)
    {
        wide = 4u;
        n = (uint64_t)v * 42949673ULL;
        JEAIII_PAIR(&tmp[0], n);
        JEAIII_PAIR(&tmp[2], n);
    }
    else if (digits <= 6u)
    {
        wide = 6u;
        n = (uint64_t)v * 429497ULL;
        JEAIII_PAIR(&tmp[0], n);
        JEAIII_PAIR(&tmp[2], n);
        JEAIII_PAIR(&tmp[4], n);
    }
    else if (digits <= 8u)
    {
        wide = 8u;
        n = ((uint64_t)v * 281474978ULL) >> 16u;
        JEAIII_PAIR(&tmp[0], n);
        JEAIII_PAIR(&tmp[2], n);
        JEAIII_PAIR(&tmp[4], n);
        JEAIII_PAIR(&tmp[6], n);
    }
    else
    {
        wide = 10u;

        n = (digits == 9u) ? (((uint64_t)v * 1441151882ULL) >> 25u) : (((uint64_t)v * 1441151881ULL) >> 25u);
        JEAIII_PAIR(&tmp[0], n);
        JEAIII_PAIR(&tmp[2], n);
        JEAIII_PAIR(&tmp[4], n);
        JEAIII_PAIR(&tmp[6], n);
        JEAIII_PAIR(&tmp[8], n);
    }

    for (unsigned i = 0; i < digits; i++)
    {
        out[i] = tmp[(wide - digits) + i];
    }
}

static size_t was(char *out, uint32_t v)
{
    const unsigned d = dc_scan(v);

    emit_one(out, v, d);
    return d;
}

MMGR_FLATTEN static size_t verba_uint_flat(char *out, uint32_t v)
{
    return MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = out, .cap = 16u, .at = 0u, .val = v, .base = 10u,
                     .min = 1u);
}

static const uint64_t POW10_64[20] = {1ull,
                                      10ull,
                                      100ull,
                                      1000ull,
                                      10000ull,
                                      100000ull,
                                      1000000ull,
                                      10000000ull,
                                      100000000ull,
                                      1000000000ull,
                                      10000000000ull,
                                      100000000000ull,
                                      1000000000000ull,
                                      10000000000000ull,
                                      100000000000000ull,
                                      1000000000000000ull,
                                      10000000000000000ull,
                                      100000000000000000ull,
                                      1000000000000000000ull,
                                      10000000000000000000ull};

#define CUT 8u

static unsigned strip_loop(uint64_t mant, unsigned digits)
{
    while ((digits > 1u) && ((mant % 10u) == 0u))
    {
        mant /= 10u;
        digits--;
    }
    return digits;
}

static unsigned strip_once(uint64_t mant, unsigned digits)
{
    while (digits > 1u)
    {
        const uint64_t q = mant / 10u;

        if ((mant - (q * 10u)) != 0u)
        {
            break;
        }
        mant = q;
        digits--;
    }
    return digits;
}

#define INV5 0xCCCCCCCCCCCCCCCDull

#define FIFTH_MAX 0x3333333333333333ull

static unsigned strip_recip(uint64_t mant, unsigned digits)
{
    while (digits > 1u)
    {

        if ((mant & 1u) != 0u)
        {
            break;
        }

        const uint64_t fifth = (mant >> 1) * INV5;

        if (fifth > FIFTH_MAX)
        {
            break;
        }
        mant = fifth;
        digits--;
    }
    return digits;
}

static unsigned strip_pow(uint64_t mant, unsigned digits)
{
    unsigned low = 0u;
    unsigned high = digits - 1u;

    while (low < high)
    {

        const unsigned mid = low + ((high - low + 1u) / 2u);

        if ((mant % POW10_64[mid]) == 0u)
        {
            low = mid;
        }
        else
        {
            high = mid - 1u;
        }
    }
    return digits - low;
}

static void digits_descending(char *out, uint64_t mant, unsigned digits)
{
    uint64_t left = mant;
    uint64_t divisor = POW10_64[digits - 1u];

    for (unsigned index = 0; index < digits; index++)
    {
        const uint64_t digit = left / divisor;

        out[index] = (char)('0' + (uint32_t)digit);
        left -= digit * divisor;
        divisor /= 10u;
    }
}

static void digits_split(char *out, uint64_t mant, unsigned digits)
{
    if (digits <= 9u)
    {
        emit_recip(out, (uint32_t)mant, digits);
        return;
    }

    const uint64_t rest = mant / POW10_64[CUT];

    const uint32_t low = (uint32_t)(mant - (rest * POW10_64[CUT]));

    if (digits <= 17u)
    {
        emit_recip(out, (uint32_t)rest, digits - CUT);
    }
    else
    {

        const uint64_t top = rest / POW10_64[CUT];
        const uint32_t mid = (uint32_t)(rest - (top * POW10_64[CUT]));

        emit_recip(out, (uint32_t)top, digits - (2u * CUT));
        emit_recip(out + (digits - (2u * CUT)), mid, CUT);
    }
    emit_recip(out + (digits - CUT), low, CUT);
}

MMGR_FLATTEN static size_t verba_put_n_flat(char *out, const char *text, size_t len)
{
    return MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = out, .cap = 64u, .at = 0u, .text = text,
                     .text_len = len);
}

MMGR_FLATTEN static void proxim_read_flat(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    MMGR_CALL(proxim.read, ProximusCfg, .dst = dst, .at = src, .size = bytes);
}

static unsigned dc_clz64(uint64_t v)
{

    const unsigned lead = (unsigned)MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_u64)(v | 1u));
    const unsigned bits = 64u - lead;
    const unsigned est = (bits * 1233u) >> 12u;

    return est + (((v | 1u) >= POW10_64[est]) ? 1u : 0u);
}

static void emit20(char *out, uint64_t value, unsigned digits)
{
    if (value <= 0xFFFFFFFFU)
    {

        emit_recip(out, (uint32_t)value, digits);
        return;
    }

    const uint64_t rest = value / POW10_64[8];
    const uint32_t low = (uint32_t)(value - (rest * POW10_64[8]));

    if (rest <= 0xFFFFFFFFU)
    {
        emit_recip(out, (uint32_t)rest, digits - 8u);
    }
    else
    {
        const uint64_t top = rest / POW10_64[8];
        const uint32_t mid = (uint32_t)(rest - (top * POW10_64[8]));

        emit_recip(out, (uint32_t)top, digits - 16u);
        emit_recip(out + (digits - 16u), mid, 8u);
    }
    emit_recip(out + (digits - 8u), low, 8u);
}

static size_t uint_test_inside(char *out, uint64_t val)
{
    const unsigned digits = dc_clz64(val);

    emit20(out, val, digits);
    return digits;
}

static size_t uint_test_outside(char *out, uint64_t val)
{
    const unsigned digits = dc_clz64(val);

    if (val <= 0xFFFFFFFFU)
    {

        emit_recip(out, (uint32_t)val, digits);
    }
    else
    {
        emit20(out, val, digits);
    }
    return digits;
}

typedef mmgr_migro_word bench_aequus_word_t MMGR_ALIAS;

typedef mmgr_migro_word bench_proxim_word_t MMGR_RAW;

typedef struct
{
    uint8_t *dst;
    const uint8_t *src;
    size_t bytes;
} BenchCopyCtx;

static void copy_words_args(BenchCopyCtx *args)
{
    size_t w = args->bytes & ~(size_t)(MMGR_RAW_WORD - 1u);

    if (w == 0u)
    {
        return;
    }
    args->bytes -= w;

    do
    {
        *(bench_aequus_word_t *)args->dst = *(const bench_aequus_word_t *)args->src;
        args->dst += MMGR_RAW_WORD;
        args->src += MMGR_RAW_WORD;
        w -= MMGR_RAW_WORD;
    } while (w);
}

static void copy_words_locals(BenchCopyCtx *args)
{
    size_t w = args->bytes & ~(size_t)(MMGR_RAW_WORD - 1u);

    if (w == 0u)
    {
        return;
    }
    args->bytes -= w;

    uint8_t *to = args->dst;
    const uint8_t *from = args->src;

    do
    {
        *(bench_aequus_word_t *)to = *(const bench_aequus_word_t *)from;
        to += MMGR_RAW_WORD;
        from += MMGR_RAW_WORD;
        w -= MMGR_RAW_WORD;
    } while (w);

    args->dst = to;
    args->src = from;
}

static void copy_words_two(BenchCopyCtx *args)
{
    size_t w = args->bytes & ~(size_t)(MMGR_RAW_WORD - 1u);

    if (w == 0u)
    {
        return;
    }
    args->bytes -= w;

    while (w >= (2u * MMGR_RAW_WORD))
    {
        *(bench_aequus_word_t *)args->dst = *(const bench_aequus_word_t *)args->src;
        *(bench_aequus_word_t *)(args->dst + MMGR_RAW_WORD) = *(const bench_aequus_word_t *)(args->src + MMGR_RAW_WORD);
        args->dst += 2u * MMGR_RAW_WORD;
        args->src += 2u * MMGR_RAW_WORD;
        w -= 2u * MMGR_RAW_WORD;
    }
    if (w != 0u)
    {
        *(bench_aequus_word_t *)args->dst = *(const bench_aequus_word_t *)args->src;
        args->dst += MMGR_RAW_WORD;
        args->src += MMGR_RAW_WORD;
    }
}

static void copy_words_counted(BenchCopyCtx *args)
{
    const size_t words = args->bytes / MMGR_RAW_WORD;

    if (words == 0u)
    {
        return;
    }
    args->bytes -= words * MMGR_RAW_WORD;

    bench_aequus_word_t *const to = (bench_aequus_word_t *)args->dst;
    const bench_aequus_word_t *const from = (const bench_aequus_word_t *)args->src;

    for (size_t index = 0; index < words; index++)
    {
        to[index] = from[index];
    }

    args->dst += words * MMGR_RAW_WORD;
    args->src += words * MMGR_RAW_WORD;
}

static void copy_read_flat(uint8_t *dst, const uint8_t *src, size_t bytes)
{

    const size_t skew = (size_t)((0u - (uintptr_t)dst) & (uintptr_t)(MMGR_RAW_WORD - 1u));
    const size_t head = (skew < bytes) ? skew : bytes;
    const size_t left = bytes - head;
    const size_t words = left / MMGR_RAW_WORD;
    const size_t tail = left - (words * MMGR_RAW_WORD);

    for (size_t index = 0; index < head; index++)
    {
        dst[index] = src[index];
    }
    dst += head;
    src += head;

    if ((((uintptr_t)src) & (uintptr_t)(MMGR_RAW_WORD - 1u)) == 0u)
    {
        bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;
        const bench_aequus_word_t *const from = (const bench_aequus_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = from[index];
        }
    }
    else
    {
        bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;
        const bench_proxim_word_t *const from = (const bench_proxim_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = from[index];
        }
    }
    dst += words * MMGR_RAW_WORD;
    src += words * MMGR_RAW_WORD;

    for (size_t index = 0; index < tail; index++)
    {
        dst[index] = src[index];
    }
}

static void copy_read_overlap(uint8_t *dst, const uint8_t *src, size_t bytes)
{
    if (bytes < MMGR_RAW_WORD)
    {
        for (size_t index = 0; index < bytes; index++)
        {
            dst[index] = src[index];
        }
        return;
    }

    const size_t skew = (size_t)((0u - (uintptr_t)dst) & (uintptr_t)(MMGR_RAW_WORD - 1u));
    uint8_t *const base = dst;
    const uint8_t *const from_base = src;
    const size_t total = bytes;

    for (size_t index = 0; index < skew; index++)
    {
        dst[index] = src[index];
    }
    dst += skew;
    src += skew;

    const size_t words = (total - skew) / MMGR_RAW_WORD;
    bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;

    if ((((uintptr_t)src) & (uintptr_t)(MMGR_RAW_WORD - 1u)) == 0u)
    {
        const bench_aequus_word_t *const wfrom = (const bench_aequus_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = wfrom[index];
        }
    }
    else
    {
        const bench_proxim_word_t *const wfrom = (const bench_proxim_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = wfrom[index];
        }
    }

    *(bench_proxim_word_t *)(base + total - MMGR_RAW_WORD) =
        *(const bench_proxim_word_t *)(from_base + total - MMGR_RAW_WORD);
}

static void copy_read_fastpath(uint8_t *dst, const uint8_t *src, size_t bytes)
{

    if (((((uintptr_t)dst) | ((uintptr_t)src)) & (uintptr_t)(MMGR_RAW_WORD - 1u)) == 0u)
    {
        const size_t words = bytes / MMGR_RAW_WORD;
        bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;
        const bench_aequus_word_t *const wfrom = (const bench_aequus_word_t *)src;

        for (size_t index = 0; index < words; index++)
        {
            to[index] = wfrom[index];
        }

        for (size_t index = words * MMGR_RAW_WORD; index < bytes; index++)
        {
            dst[index] = src[index];
        }
        return;
    }

    copy_read_flat(dst, src, bytes);
}

static void copy_read_dispatch(uint8_t *dst, const uint8_t *src, size_t bytes)
{

    const size_t skew = (size_t)((0u - (uintptr_t)dst) & (uintptr_t)(MMGR_RAW_WORD - 1u));
    const size_t head = (skew < bytes) ? skew : bytes;
    size_t index = 0;

    while (index != head)
    {
        dst[index] = src[index];
        index++;
    }
    dst += head;
    src += head;

    const size_t left = bytes - head;
    const size_t words = left / MMGR_RAW_WORD;
    bench_aequus_word_t *const to = (bench_aequus_word_t *)dst;

    if ((((uintptr_t)src) & (uintptr_t)(MMGR_RAW_WORD - 1u)) != 0u)
    {
        copy_read_flat(dst, src, left);
        return;
    }

    const bench_aequus_word_t *const from = (const bench_aequus_word_t *)src;

    switch (words)
    {
    default: {
        size_t at = 0;

        while (at != words)
        {
            to[at] = from[at];
            at++;
        }
        break;
    }
    case 8u:
        to[7] = from[7];

    case 7u:
        to[6] = from[6];

    case 6u:
        to[5] = from[5];

    case 5u:
        to[4] = from[4];

    case 4u:
        to[3] = from[3];

    case 3u:
        to[2] = from[2];

    case 2u:
        to[1] = from[1];

    case 1u:
        to[0] = from[0];

    case 0u:
        break;
    }

    const size_t done = words * MMGR_RAW_WORD;

    for (size_t at = done; at != left; at++)
    {
        dst[at] = src[at];
    }
}

typedef struct
{
    uint64_t hi;
    uint64_t lo;
} BenchProduct;

static void bench_mul_wide(uint64_t a, uint64_t b, BenchProduct *out)
{
    const uint64_t half = (uint64_t)0xFFFFFFFFu;
    const uint64_t a0 = a & half;
    const uint64_t a1 = a >> 32;
    const uint64_t b0 = b & half;
    const uint64_t b1 = b >> 32;

    const uint64_t p00 = a0 * b0;
    const uint64_t p01 = a0 * b1;
    const uint64_t p10 = a1 * b0;
    const uint64_t p11 = a1 * b1;
    const uint64_t mid = (p00 >> 32) + (p01 & half) + (p10 & half);

    out->lo = (p00 & half) | (mid << 32);
    out->hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

static void bench_mul_narrow(uint64_t a, uint64_t b, BenchProduct *out)
{

    const uint32_t a0 = (uint32_t)a;
    const uint32_t a1 = (uint32_t)(a >> 32);
    const uint32_t b0 = (uint32_t)b;
    const uint32_t b1 = (uint32_t)(b >> 32);

    const uint64_t p00 = (uint64_t)a0 * b0;
    const uint64_t p01 = (uint64_t)a0 * b1;
    const uint64_t p10 = (uint64_t)a1 * b0;
    const uint64_t p11 = (uint64_t)a1 * b1;
    const uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;

    out->lo = ((uint64_t)(uint32_t)p00) | (mid << 32);
    out->hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

typedef struct
{
    uint64_t hi;
    uint64_t lo;
    int32_t fe2;
    uint32_t rest;
} BenchWide;

static void bench_norm(BenchWide *w)
{
    if (w->hi == 0u)
    {
        if (w->lo == 0u)
        {
            return;
        }
        w->hi = w->lo;
        w->lo = 0u;
        w->fe2 -= 64;
    }

    const int32_t n = (int32_t)MMGR_CALL(clz.lead, ClzCfg, .val = (mmgr_u64)w->hi);
    if (n != 0)
    {
        w->hi = (w->hi << n) | (w->lo >> (64 - n));
        w->lo <<= n;
        w->fe2 -= n;
    }
}

static void bench_pow_128(BenchWide *w, uint64_t ghi, uint64_t glo, int32_t ge2)
{
    BenchProduct hh;
    BenchProduct hl;
    BenchProduct lh;
    BenchProduct ll;

    bench_mul_narrow(w->hi, ghi, &hh);
    bench_mul_narrow(w->hi, glo, &hl);
    bench_mul_narrow(w->lo, ghi, &lh);
    bench_mul_narrow(w->lo, glo, &ll);

    uint64_t carry = 0u;
    uint64_t col1 = ll.hi + hl.lo;
    carry += (col1 < ll.hi) ? 1u : 0u;
    const uint64_t col1b = col1 + lh.lo;
    carry += (col1b < col1) ? 1u : 0u;
    col1 = col1b;

    uint64_t col2 = hh.lo + hl.hi;
    uint64_t carry2 = (col2 < hh.lo) ? 1u : 0u;
    const uint64_t col2b = col2 + lh.hi;
    carry2 += (col2b < col2) ? 1u : 0u;
    col2 = col2b + carry;
    carry2 += (col2 < col2b) ? 1u : 0u;

    if ((ll.lo != 0u) || (col1 != 0u))
    {
        w->rest = 1u;
    }
    w->hi = hh.hi + carry2;
    w->lo = col2;
    w->fe2 = w->fe2 + ge2 + 128;
    bench_norm(w);
}

static void bench_pow_64(BenchWide *w, uint64_t g)
{
    BenchProduct hh;
    BenchProduct lh;

    bench_mul_narrow(w->hi, g, &hh);
    bench_mul_narrow(w->lo, g, &lh);

    const uint64_t col = hh.lo + lh.hi;
    const uint64_t carry = (col < hh.lo) ? 1u : 0u;

    if (lh.lo != 0u)
    {
        w->rest = 1u;
    }
    w->hi = hh.hi + carry;
    w->lo = col;
    w->fe2 = w->fe2 + 64;
    bench_norm(w);
}

static void bench_walk_fixed(BenchWide *w, int32_t ex)
{
    const int32_t k = (ex < 0) ? -ex : ex;

    for (int32_t i = 0; i < MMGR_POW5_STEPS; ++i)
    {
        if (((k >> i) & 1) != 0)
        {
            const MmgrPow5 *const p = (ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i];

            bench_pow_128(w, p->hi, p->lo, (int32_t)p->e2);
        }
    }
    w->fe2 += ex;
}

static void bench_walk_early(BenchWide *w, int32_t ex)
{
    int32_t k = (ex < 0) ? -ex : ex;

    for (int32_t i = 0; (i < MMGR_POW5_STEPS) && (k != 0); ++i)
    {
        if ((k & 1) != 0)
        {
            const MmgrPow5 *const p = (ex < 0) ? &mmgr_pow5_down[i] : &mmgr_pow5_up[i];

            bench_pow_128(w, p->hi, p->lo, (int32_t)p->e2);
        }
        k >>= 1;
    }
    w->fe2 += ex;
}

static const uint64_t BENCH_POW10_U64[19] = {1ull,
                                             10ull,
                                             100ull,
                                             1000ull,
                                             10000ull,
                                             100000ull,
                                             1000000ull,
                                             10000000ull,
                                             100000000ull,
                                             1000000000ull,
                                             10000000000ull,
                                             100000000000ull,
                                             1000000000000ull,
                                             10000000000000ull,
                                             100000000000000ull,
                                             1000000000000000ull,
                                             10000000000000000ull,
                                             100000000000000000ull,
                                             1000000000000000000ull};

static void bench_walk_exact(BenchWide *w, int32_t ex)
{
    int32_t left = ex;

    while (left > 0)
    {
        const int32_t take = (left > 18) ? 18 : left;

        bench_pow_64(w, BENCH_POW10_U64[take]);
        left -= take;
    }
    w->fe2 += 0;
}

static uint32_t pow_is_correct(void)
{
    static const uint64_t seeds[5] = {0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0x9E3779B97F4A7C15ull,
                                      0x0123456789ABCDEFull, 0xC000000000000000ull};
    uint32_t bad = 0u;

    for (unsigned which = 0; which < 5u; which++)
    {
        for (int32_t ex = 1; ex <= 60; ex++)
        {
            BenchWide walked = {seeds[which], 0u, -51, 0u};
            BenchWide chunked = {seeds[which], 0u, -51, 0u};

            bench_walk_fixed(&walked, ex);
            bench_walk_exact(&chunked, ex);

            if ((walked.hi != chunked.hi) || (walked.fe2 != chunked.fe2))
            {
                bad++;
            }
            else if (walked.lo != chunked.lo)
            {
                g_pow_lo_only++;
            }
        }
    }
    return bad;
}

static volatile uint64_t g_mul_a = 0x123456789ABCDEFull;
static volatile uint64_t g_mul_b = 0xFEDCBA987654321ull;

static volatile uint64_t g_pow_ten = 1000000ull;

static BenchWide g_wide_a;
static BenchWide g_wide_b;

static BenchProduct g_mul_out;

#define CUT_MAGIC 0xABCC77118461CEFDull

#define CUT_SHIFT 26u

static uint64_t cut_magic(uint64_t value)
{
    BenchProduct p;

    bench_mul_narrow(value, CUT_MAGIC, &p);
    return p.hi >> CUT_SHIFT;
}

static uint32_t cut_is_correct(void)
{
    uint32_t bad = 0u;
    uint64_t probe = 1u;

    for (unsigned k = 0; k < 20u; k++)
    {
        const uint64_t around[3] = {probe - 1u, probe, probe + 1u};

        for (unsigned which = 0; which < 3u; which++)
        {
            if (cut_magic(around[which]) != (around[which] / POW10_64[8]))
            {
                bad++;
            }
        }
        probe *= 10u;
    }

    uint64_t walk = 7u;

    for (unsigned step = 0; step < 4000u; step++)
    {
        if (cut_magic(walk) != (walk / POW10_64[8]))
        {
            bad++;
        }

        walk = (walk * 6364136223846793005ull) + 1442695040888963407ull;
    }
    return bad;
}

static uint8_t g_check_src[192];
static uint8_t g_check_dst[192];

static uint32_t copy_is_correct(void)
{
    uint32_t bad = 0u;

    for (uint32_t index = 0; index < sizeof g_check_src; index++)
    {

        g_check_src[index] = (uint8_t)(index + 1u);
    }

    for (uint32_t soff = 0; soff < 8u; soff++)
    {
        for (uint32_t doff = 0; doff < 8u; doff++)
        {
            for (uint32_t len = 0; len <= 64u; len++)
            {
                memset(g_check_dst, 0xA5, sizeof g_check_dst);

                MMGR_CALL(proxim.read, ProximusCfg, .dst = g_check_dst + doff, .at = g_check_src + soff, .size = len);

                if (doff != 0u)
                {
                    bad += (g_check_dst[doff - 1u] != 0xA5u) ? 1u : 0u;
                }
                bad += (g_check_dst[doff + len] != 0xA5u) ? 1u : 0u;

                for (uint32_t index = 0; index < len; index++)
                {
                    if (g_check_dst[doff + index] != g_check_src[soff + index])
                    {
                        bad++;
                        break;
                    }
                }
            }
        }
    }
    return bad;
}

static size_t libc_put_n(char *out, size_t cap, size_t at, const char *src, size_t len)
{
    if ((at >= cap) || (len > ((cap - at) - 1u)))
    {
        return cap;
    }
    memcpy(out + at, src, len);
    return at + len;
}

static size_t zeros_per_byte(char *out, size_t cap, size_t at, size_t n)
{
    while (n-- != 0u)
    {
        if ((at >= cap) || (1u > ((cap - at) - 1u)))
        {
            return cap;
        }
        out[at] = '0';
        at += 1u;
    }
    return at;
}

static size_t zeros_one_test(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    for (size_t k = 0; k < n; k++)
    {
        out[at + k] = '0';
    }
    return at + n;
}

static size_t zeros_one_test_noopt(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    for (size_t k = 0; k < n; k++)
    {

        *(volatile char *)(out + at + k) = '0';
    }
    return at + n;
}

static size_t zeros_memor_set(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    MMGR_CALL(memor.set, MemoriaCfg, .dst = out + at, .bytes = n, .val = (uint8_t)'0');
    return at + n;
}

static size_t zeros_hybrid(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    if (n <= 8u)
    {
        for (size_t k = 0; k < n; k++)
        {

            *(volatile char *)(out + at + k) = '0';
        }
        return at + n;
    }
    MMGR_CALL(memor.set, MemoriaCfg, .dst = out + at, .bytes = n, .val = (uint8_t)'0');
    return at + n;
}

static size_t zeros_built(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }

    char *const to = out + at;

    switch (n)
    {
    default:
        MMGR_CALL(memor.set, MemoriaCfg, .dst = to, .bytes = n, .val = (uint8_t)'0');
        break;
    case 8u:
        to[7] = '0';

    case 7u:
        to[6] = '0';

    case 6u:
        to[5] = '0';

    case 5u:
        to[4] = '0';

    case 4u:
        to[3] = '0';

    case 3u:
        to[2] = '0';

    case 2u:
        to[1] = '0';

    case 1u:
        to[0] = '0';

    case 0u:
        break;
    }
    return at + n;
}

static size_t zeros_proxim(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }

    char *const to = out + at;

    switch (n)
    {
    default: {

        const uint64_t lanes = ((~(uint64_t)0) / 0xFFu) * (uint64_t)'0';
        size_t k = 0u;

        while ((n - k) >= MMGR_RAW_WORD)
        {
            MMGR_CALL(proxim.put, ProximusCfg, .dst = (uint8_t *)to + k, .val = lanes);
            k += MMGR_RAW_WORD;
        }
        while (k != n)
        {
            to[k] = '0';
            k += 1u;
        }
        break;
    }
    case 8u:
        to[7] = '0';

    case 7u:
        to[6] = '0';

    case 6u:
        to[5] = '0';

    case 5u:
        to[4] = '0';

    case 4u:
        to[3] = '0';

    case 3u:
        to[2] = '0';

    case 2u:
        to[1] = '0';

    case 1u:
        to[0] = '0';

    case 0u:
        break;
    }
    return at + n;
}

static size_t zeros_aligned_fill(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }

    char *const to = out + at;

    switch (n)
    {
    default: {

        const uint64_t lanes = ((~(uint64_t)0) / 0xFFu) * (uint64_t)'0';

        const size_t skew = (size_t)((0u - (uintptr_t)to) & (uintptr_t)(MMGR_RAW_WORD - 1u));
        const size_t head = (skew < n) ? skew : n;
        uint8_t *put = (uint8_t *)to;
        size_t left = n;

        for (size_t k = 0; k < head; k++)
        {
            *put++ = (uint8_t)'0';
        }
        left -= head;

        size_t words = left - (left & (MMGR_RAW_WORD - 1u));

        while (words >= (4u * MMGR_RAW_WORD))
        {
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put, .val = lanes);
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put + MMGR_RAW_WORD, .val = lanes);
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put + (2u * MMGR_RAW_WORD), .val = lanes);
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put + (3u * MMGR_RAW_WORD), .val = lanes);
            put += 4u * MMGR_RAW_WORD;
            words -= 4u * MMGR_RAW_WORD;
            left -= 4u * MMGR_RAW_WORD;
        }
        while (words != 0u)
        {
            MMGR_CALL(proxim.al_put, ProximusCfg, .dst = put, .val = lanes);
            put += MMGR_RAW_WORD;
            words -= MMGR_RAW_WORD;
            left -= MMGR_RAW_WORD;
        }
        while (left != 0u)
        {
            *put++ = (uint8_t)'0';
            left -= 1u;
        }
        break;
    }
    case 8u:
        to[7] = '0';

    case 7u:
        to[6] = '0';

    case 6u:
        to[5] = '0';

    case 5u:
        to[4] = '0';

    case 4u:
        to[3] = '0';

    case 3u:
        to[2] = '0';

    case 2u:
        to[1] = '0';

    case 1u:
        to[0] = '0';

    case 0u:
        break;
    }
    return at + n;
}

static size_t zeros_plain(char *out, size_t cap, size_t at, size_t n)
{
    if ((at >= cap) || (n > ((cap - at) - 1u)))
    {
        return cap;
    }
    for (size_t k = 0; k < n; k++)
    {
        out[at + k] = '0';
    }
    return at + n;
}

static size_t digits_per_char(char *out, size_t cap, size_t at, const char *src, size_t n, size_t point)
{
    for (size_t index = 0; index < n; index++)
    {
        if ((index == point) && (index != 0u))
        {
            if ((at >= cap) || (1u > ((cap - at) - 1u)))
            {
                return cap;
            }
            out[at] = '.';
            at += 1u;
        }
        if ((at >= cap) || (1u > ((cap - at) - 1u)))
        {
            return cap;
        }
        out[at] = src[index];
        at += 1u;
    }
    return at;
}

static size_t digits_one_test(char *out, size_t cap, size_t at, const char *src, size_t n, size_t point)
{
    const size_t width = n + (((point != 0u) && (point < n)) ? 1u : 0u);

    if ((at >= cap) || (width > ((cap - at) - 1u)))
    {
        return cap;
    }

    size_t put = at;

    for (size_t index = 0; index < n; index++)
    {
        if ((index == point) && (index != 0u))
        {
            out[put] = '.';
            put += 1u;
        }
        out[put] = src[index];
        put += 1u;
    }
    return put;
}

static size_t digits_two_runs(char *out, size_t cap, size_t at, const char *src, size_t n, size_t point)
{
    const mmgr_bool has_point = (mmgr_bool)((point != 0u) && (point < n));
    const size_t width = n + (has_point ? 1u : 0u);

    if ((at >= cap) || (width > ((cap - at) - 1u)))
    {
        return cap;
    }

    const size_t lead = has_point ? point : n;
    size_t put = at;

    for (size_t index = 0; index < lead; index++)
    {
        out[put + index] = src[index];
    }
    put += lead;

    if (has_point)
    {
        out[put] = '.';
        put += 1u;

        for (size_t index = lead; index < n; index++)
        {
            out[put + (index - lead)] = src[index];
        }
        put += n - lead;
    }
    return put;
}

static size_t digits_scratch(char *out, size_t cap, size_t at, uint64_t mant, unsigned digits, size_t point)
{
    char scratch[24];
    const mmgr_bool has_point = (mmgr_bool)((point != 0u) && (point < digits));
    const size_t width = digits + (has_point ? 1u : 0u);

    if ((at >= cap) || (width > ((cap - at) - 1u)))
    {
        return cap;
    }

    emit20(scratch, mant, digits);

    const size_t lead = has_point ? point : digits;
    size_t put = at;

    for (size_t index = 0; index < lead; index++)
    {
        out[put + index] = scratch[index];
    }
    put += lead;

    if (has_point)
    {
        out[put] = '.';
        put += 1u;

        for (size_t index = lead; index < digits; index++)
        {
            out[put + (index - lead)] = scratch[index];
        }
        put += digits - lead;
    }
    return put;
}

static size_t digits_inplace(char *out, size_t cap, size_t at, uint64_t mant, unsigned digits, size_t point)
{
    const mmgr_bool has_point = (mmgr_bool)((point != 0u) && (point < digits));
    const size_t width = digits + (has_point ? 1u : 0u);

    if ((at >= cap) || (width > ((cap - at) - 1u)))
    {
        return cap;
    }

    if (!has_point)
    {
        emit20(out + at, mant, digits);
        return at + digits;
    }

    emit20(out + at + 1u, mant, digits);

    for (size_t index = 0; index < point; index++)
    {
        out[at + index] = out[at + index + 1u];
    }
    out[at + point] = '.';
    return at + width;
}

static int frexp_exponent(double value)
{
    int found = 0;

    (void)frexp(value, &found);
    return found;
}

static uint64_t bits_via_memcpy(double value)
{
    uint64_t held = 0u;

    memcpy(&held, &value, sizeof held);
    return held;
}

void dbench_run(void)
{
    static const uint32_t vals[] = {7u, 314u, 65535u, 4294967295u};
    static const unsigned wid[] = {1u, 3u, 5u, 10u};

    for (;;)
    {
        DBENCH_BANNER("verba itoa emitters and digit counters");

        printf("DB copy_check      disagreements=%u\n", (unsigned)copy_is_correct());
        printf("DB cut_check       disagreements=%u\n", (unsigned)cut_is_correct());
        g_pow_lo_only = 0u;
        printf("DB pow_check       hi_or_exp=%u lo_only=%u\n", (unsigned)pow_is_correct(), (unsigned)g_pow_lo_only);

        for (unsigned vi = 0; vi < (sizeof vals / sizeof vals[0]); vi++)
        {
            const uint32_t v = vals[vi];
            const unsigned d = wid[vi];
            const uint32_t iters = 20000u;

            DBENCH_AB("count", iters, d, DBENCH_KEEP(dc_scan(v)), DBENCH_KEEP(dc_clz(v)));

            DBENCH_AB("pair", iters, d, (emit_one(g_out, v, d), DBENCH_KEEP(g_out)),
                      (emit_pair(g_out, v, d), DBENCH_KEEP(g_out)));

            DBENCH_AB("recip", iters, d, (emit_one(g_out, v, d), DBENCH_KEEP(g_out)),
                      (emit_recip(g_out, v, d), DBENCH_KEEP(g_out)));

            DBENCH_AB("jeaiii", iters, d, (emit_one(g_out, v, d), DBENCH_KEEP(g_out)),
                      (emit_jeaiii(g_out, v, d), DBENCH_KEEP(g_out)));

            DBENCH_AB("call", iters, d,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = g_out, .cap = sizeof g_out,
                                            .at = 0u, .val = v, .base = 10u, .min = 1u)),
                      DBENCH_KEEP(verba_uint_flat(g_out, v)));

            DBENCH_AB("arith", iters, d, DBENCH_KEEP(was(g_out, v)), DBENCH_KEEP(verba_uint_flat(g_out, v)));
        }

        {
            static const uint64_t mantissas[] = {123456ull, 1234567890ull, 12345678901234567ull, 123456789012345678ull};
            static const unsigned widths[] = {6u, 10u, 17u, 18u};

            for (unsigned which = 0; which < 4u; which++)
            {
                DBENCH_AB("digits", 20000u, widths[which],
                          (digits_descending(g_out, mantissas[which], widths[which]), DBENCH_KEEP(g_out)),
                          (digits_split(g_out, mantissas[which], widths[which]), DBENCH_KEEP(g_out)));
            }
        }

        {
            static const char text[] = "the quick brown fox jumps over";
            const uint32_t iters = 5000u;

            DBENCH_AB("s:put", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%s", text)));

            DBENCH_AB("s:put_n", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text, .text_len = sizeof text - 1u)),
                      (memcpy(g_wide, text, sizeof text - 1u), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:uint32", iters, 10u, DBENCH_KEEP(uint_test_inside(g_wide, 4294967295ull)),
                      DBENCH_KEEP(uint_test_outside(g_wide, 4294967295ull)));

            DBENCH_AB("s:uint64", iters, 20u, DBENCH_KEEP(uint_test_inside(g_wide, 18446744073709551615ull)),
                      DBENCH_KEEP(uint_test_outside(g_wide, 18446744073709551615ull)));

            {
                BenchCopyCtx one = {.dst = (uint8_t *)g_wide, .src = (const uint8_t *)text, .bytes = g_len};
                BenchCopyCtx two = {.dst = (uint8_t *)g_wide, .src = (const uint8_t *)text, .bytes = g_len};

                DBENCH_AB("s:words", iters, sizeof text - 1u,
                          (one.dst = g_check_dst, one.src = g_check_src, one.bytes = g_len, copy_words_args(&one),
                           DBENCH_KEEP(g_check_dst)),
                          (two.dst = g_check_dst + 64u, two.src = g_check_src + 64u, two.bytes = g_len,
                           copy_words_locals(&two), DBENCH_KEEP(g_check_dst)));

                DBENCH_AB("s:counted", iters, sizeof text - 1u,
                          (one.dst = (uint8_t *)g_wide, one.src = (const uint8_t *)text, one.bytes = g_len,
                           copy_words_args(&one), DBENCH_KEEP(g_wide)),
                          (two.dst = (uint8_t *)g_wide, two.src = (const uint8_t *)text, two.bytes = g_len,
                           copy_words_counted(&two), DBENCH_KEEP(g_wide)));

                DBENCH_AB("s:wordsvlibc", iters, sizeof text - 1u,
                          (one.dst = g_check_dst, one.src = g_check_src, one.bytes = g_len, copy_words_args(&one),
                           DBENCH_KEEP(g_check_dst)),
                          (memcpy(g_check_dst + 64u, g_check_src + 64u, g_len), DBENCH_KEEP(g_check_dst)));

                DBENCH_AB("s:words2", iters, sizeof text - 1u,
                          (one.dst = g_check_dst, one.src = g_check_src, one.bytes = g_len, copy_words_args(&one),
                           DBENCH_KEEP(g_check_dst)),
                          (two.dst = g_check_dst + 64u, two.src = g_check_src + 64u, two.bytes = g_len,
                           copy_words_two(&two), DBENCH_KEEP(g_check_dst)));
            }

            DBENCH_AB(
                "s:dispatch", iters, sizeof text - 1u,
                (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = (const char *)g_check_src, .size = g_len),
                 DBENCH_KEEP(g_wide)),
                (copy_read_dispatch((uint8_t *)g_wide, g_check_src, g_len), DBENCH_KEEP(g_wide)));

            DBENCH_AB(
                "s:overlap", iters, sizeof text - 1u,
                (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len), DBENCH_KEEP(g_wide)),
                (copy_read_overlap((uint8_t *)g_wide, (const uint8_t *)text, g_len), DBENCH_KEEP(g_wide)));

            DBENCH_AB(
                "s:fastpath", iters, sizeof text - 1u,
                (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len), DBENCH_KEEP(g_wide)),
                (copy_read_fastpath((uint8_t *)g_wide, (const uint8_t *)text, g_len), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:flat", iters, sizeof text - 1u,
                      (copy_read_flat((uint8_t *)g_wide, (const uint8_t *)text, g_len), DBENCH_KEEP(g_wide)),
                      (memcpy(g_wide, text, g_len), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:copy32", iters, 32u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len_whole),
                       DBENCH_KEEP(g_wide)),
                      (memcpy(g_wide, text, g_len_whole), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:copy_call", iters, 32u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = g_len_whole),
                       DBENCH_KEEP(g_wide)),
                      (proxim_read_flat((uint8_t *)g_wide, (const uint8_t *)text, g_len_whole), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:copy", iters, sizeof text - 1u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = sizeof text - 1u),
                       DBENCH_KEEP(g_wide)),
                      (memcpy(g_wide, text, sizeof text - 1u), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:put_flat", iters, sizeof text - 1u,
                      DBENCH_KEEP(verba_put_n_flat(g_wide, text, sizeof text - 1u)),
                      (memcpy(g_wide, text, sizeof text - 1u), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:copy1", iters, 1u,
                      (MMGR_CALL(proxim.read, ProximusCfg, .dst = g_wide, .at = text, .size = 1u), DBENCH_KEEP(g_wide)),
                      (memcpy(g_wide, text, 1u), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:put_n1", iters, 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text, .text_len = 1u)),
                      (memcpy(g_wide, text, 1u), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:put_safe", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text, .text_len = g_len)),
                      DBENCH_KEEP(libc_put_n(g_wide, sizeof g_wide, 0u, text, g_len)));

            DBENCH_AB("s:put_meas", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = g_text)),
                      (memcpy(g_wide, g_text, strlen(g_text)), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:put_run", iters, sizeof text - 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_textus.put_n, VerbaTextusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .text = text, .text_len = g_len)),
                      (memcpy(g_wide, text, g_len), DBENCH_KEEP(g_wide)));

            DBENCH_AB("s:ch", iters, 1u,
                      DBENCH_KEEP(MMGR_CALL(verba_littera.ch, VerbaLitteraCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .ch = 'x')),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%c", 'x')));

            DBENCH_AB("s:u32", iters, 10u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.u32, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = 4294967295u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%u", 4294967295u)));

            DBENCH_AB(
                "s:u64clip", iters, 20u,
                DBENCH_KEEP(MMGR_CALL(verba_numerus.u64_clip, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                      .at = 0u, .val = 18446744073709551615ull, .columns = 24u)),
                DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%24llu", 18446744073709551615ull)));

            DBENCH_AB("s:u64", iters, 20u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.u64, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = 18446744073709551615ull)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%llu", 18446744073709551615ull)));

            DBENCH_AB("s:i64", iters, 19u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.i64, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .sval = -9223372036854775807ll)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%lld", -9223372036854775807ll)));

            DBENCH_AB("s:hex", iters, 8u,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.hex, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = 0xDEADBEEFu)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%x", 0xDEADBEEFu)));

            DBENCH_AB("s:g", iters, 17u,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.g, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = 3.14159265358979, .sig = 17u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%.*g", 17, 3.14159265358979)));

            DBENCH_AB("s:fixed", iters, 6u,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = 3.14159265358979, .decimals = 6u)),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "%.*f", 6, 3.14159265358979)));

            DBENCH_OP("f:whole", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.fixed, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = g_rec_real2, .decimals = 6u)));

            DBENCH_OP("f:ip", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = g_fix_ip, .base = 10u, .min = 1u)));

            DBENCH_OP("f:frac", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = g_fix_frac, .base = 10u, .min = 6u)));

            DBENCH_OP("f:point", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_littera.ch, VerbaLitteraCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .ch = '.')));

            DBENCH_OP("f:scale", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 6,
                                            .above = 0u)));

            DBENCH_OP("g:whole", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.g, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = g_rec_real2, .sig = 17u)));

            DBENCH_OP("g:strip", iters, DBENCH_KEEP((g_g_mant % 10u) == 0u));

            DBENCH_AB("g:strip_pi", iters, 17u, DBENCH_KEEP(strip_loop(g_g_mant, 17u)),
                      DBENCH_KEEP(strip_pow(g_g_mant, 17u)));

            DBENCH_AB("g:strip_rnd", iters, 17u, DBENCH_KEEP(strip_loop(g_g_round, 17u)),
                      DBENCH_KEEP(strip_pow(g_g_round, 17u)));

            DBENCH_AB("g:strip_one_pi", iters, 17u, DBENCH_KEEP(strip_loop(g_g_mant, 17u)),
                      DBENCH_KEEP(strip_once(g_g_mant, 17u)));

            DBENCH_AB("g:strip_one_rnd", iters, 17u, DBENCH_KEEP(strip_loop(g_g_round, 17u)),
                      DBENCH_KEEP(strip_once(g_g_round, 17u)));

            DBENCH_AB("g:strip_rec_pi", iters, 17u, DBENCH_KEEP(strip_loop(g_g_mant, 17u)),
                      DBENCH_KEEP(strip_recip(g_g_mant, 17u)));

            DBENCH_AB("g:strip_rec_rnd", iters, 17u, DBENCH_KEEP(strip_loop(g_g_round, 17u)),
                      DBENCH_KEEP(strip_recip(g_g_round, 17u)));

            DBENCH_OP("g:small", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_fractio.g, VerbaFractioCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .real = g_rec_small, .sig = 17u)));

            DBENCH_OP("g:digits", iters,
                      DBENCH_KEEP(MMGR_CALL(verba_numerus.uint, VerbaNumerusCfg, .out = g_wide, .cap = sizeof g_wide,
                                            .at = 0u, .val = g_g_mant, .base = 10u, .min = 17u)));

            DBENCH_OP("g:emit", iters, (emit20(g_wide, g_g_mant, 17u), DBENCH_KEEP(g_wide)));

            DBENCH_OP("g:cut", iters, DBENCH_KEEP(g_g_mant / POW10_64[8]));

            DBENCH_AB("g:cut_magic", iters, 8u, DBENCH_KEEP(g_g_mant / POW10_64[8]), DBENCH_KEEP(cut_magic(g_g_mant)));

            DBENCH_OP("f:pow_zero", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_zero, .e2 = -51, .ex = 6,
                                            .above = 0u)));

            DBENCH_OP("f:pow0", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 0,
                                            .above = 0u)));

            DBENCH_OP("f:pow1", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 1,
                                            .above = 0u)));

            DBENCH_OP("f:pow3", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 3,
                                            .above = 0u)));

            DBENCH_OP("f:pow7", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 7,
                                            .above = 0u)));

            DBENCH_OP("f:scale_neg", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = -4,
                                            .above = 0u)));

            DBENCH_OP("f:scale20", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = 20,
                                            .above = 0u)));

            DBENCH_OP("f:scale_neg13", iters,
                      DBENCH_KEEP(MMGR_CALL(muto.scale_to_u64, TransformoCfg, .mant = &g_fix_rem, .e2 = -51, .ex = -13,
                                            .above = 0u)));

            DBENCH_AB("f:mul", iters, 8u, (bench_mul_wide(g_mul_a, g_mul_b, &g_mul_out), DBENCH_KEEP(g_mul_out.hi)),
                      (bench_mul_narrow(g_mul_a, g_mul_b, &g_mul_out), DBENCH_KEEP(g_mul_out.hi)));

            DBENCH_AB("f:walk20", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, 20), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_exact(&g_wide_b, 20), DBENCH_KEEP(g_wide_b.hi)));

            DBENCH_AB("f:walk40", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, 40), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_exact(&g_wide_b, 40), DBENCH_KEEP(g_wide_b.hi)));

            DBENCH_AB("f:walk0", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, 0), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_early(&g_wide_b, 0), DBENCH_KEEP(g_wide_b.hi)));

            DBENCH_AB("f:walkneg1", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, -1), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_early(&g_wide_b, -1), DBENCH_KEEP(g_wide_b.hi)));

            DBENCH_AB("f:walkneg7", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_walk_fixed(&g_wide_a, -7), DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_walk_early(&g_wide_b, -7), DBENCH_KEEP(g_wide_b.hi)));

            DBENCH_AB("f:pow_shape", iters, 8u,
                      (g_wide_a.hi = g_mul_a, g_wide_a.lo = 0u, g_wide_a.fe2 = -51, g_wide_a.rest = 0u,
                       bench_pow_128(&g_wide_a, mmgr_pow5_up[1].hi, mmgr_pow5_up[1].lo, (int32_t)mmgr_pow5_up[1].e2),
                       bench_pow_128(&g_wide_a, mmgr_pow5_up[2].hi, mmgr_pow5_up[2].lo, (int32_t)mmgr_pow5_up[2].e2),
                       DBENCH_KEEP(g_wide_a.hi)),
                      (g_wide_b.hi = g_mul_a, g_wide_b.lo = 0u, g_wide_b.fe2 = -51, g_wide_b.rest = 0u,
                       bench_pow_64(&g_wide_b, g_pow_ten), DBENCH_KEEP(g_wide_b.hi)));
        }

        {
            const mmgr_fval fields[] = {MMGR_VSTR("id="),         MMGR_VU64(g_rec_u64), MMGR_VSTR(" x="),
                                        MMGR_VHEXW(g_rec_hex, 8), MMGR_VSTR(" f="),     MMGR_VFIXW(g_rec_real, 4)};

            DBENCH_AB("s:record", 2000u, 34u,
                      DBENCH_KEEP(MMGR_CALL(numer.emit, NumerosCfg, .out = g_wide, .cap = sizeof g_wide, .vals = fields,
                                            .nvals = sizeof fields / sizeof fields[0])),
                      DBENCH_KEEP(snprintf(g_wide, sizeof g_wide, "id=%llu x=%08lx f=%.4f",
                                           (unsigned long long)g_rec_u64, (unsigned long)g_rec_hex, g_rec_real)));
        }

        {
            const uint32_t iters = 5000u;

            for (unsigned which = 0; which < 3u; which++)
            {
                static const size_t runs[] = {2u, 6u, 18u};

                g_zero_n = runs[which];

                DBENCH_AB("s:z_fill", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_one_test(g_wide, sizeof g_wide, 0u, g_zero_n)));

                DBENCH_AB("s:z_test", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_one_test_noopt(g_wide, sizeof g_wide, 0u, g_zero_n)));

                DBENCH_AB("s:z_memor", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_memor_set(g_wide, sizeof g_wide, 0u, g_zero_n)));

                DBENCH_AB("s:z_hybrid", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_hybrid(g_wide, sizeof g_wide, 0u, g_zero_n)));

                DBENCH_AB("s:z_built", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_built(g_wide, sizeof g_wide, 0u, g_zero_n)));

                DBENCH_AB("s:z_proxim", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_proxim(g_wide, sizeof g_wide, 0u, g_zero_n)));

                DBENCH_AB("s:z_align", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_aligned_fill(g_wide, sizeof g_wide, 0u, g_zero_n)));

                DBENCH_AB("s:z_plain", iters, (unsigned)runs[which],
                          DBENCH_KEEP(zeros_per_byte(g_wide, sizeof g_wide, 0u, g_zero_n)),
                          DBENCH_KEEP(zeros_plain(g_wide, sizeof g_wide, 0u, g_zero_n)));
            }

            {
                static const char laid[24] = "31415926535897932384";

                DBENCH_AB("s:d_point", iters, 17u,
                          DBENCH_KEEP(digits_per_char(g_wide, sizeof g_wide, 0u, laid, 17u, 1u)),
                          DBENCH_KEEP(digits_one_test(g_wide, sizeof g_wide, 0u, laid, 17u, 1u)));

                DBENCH_AB("s:d_plain", iters, 6u, DBENCH_KEEP(digits_per_char(g_wide, sizeof g_wide, 0u, laid, 6u, 0u)),
                          DBENCH_KEEP(digits_one_test(g_wide, sizeof g_wide, 0u, laid, 6u, 0u)));

                DBENCH_AB("s:d_runs", iters, 17u,
                          DBENCH_KEEP(digits_one_test(g_wide, sizeof g_wide, 0u, laid, 17u, 1u)),
                          DBENCH_KEEP(digits_two_runs(g_wide, sizeof g_wide, 0u, laid, 17u, 1u)));

                DBENCH_AB("s:d_runs6", iters, 6u, DBENCH_KEEP(digits_one_test(g_wide, sizeof g_wide, 0u, laid, 6u, 0u)),
                          DBENCH_KEEP(digits_two_runs(g_wide, sizeof g_wide, 0u, laid, 6u, 0u)));

                DBENCH_AB("s:d_ip_exp", iters, 17u,
                          DBENCH_KEEP(digits_scratch(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 1u)),
                          DBENCH_KEEP(digits_inplace(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 1u)));

                DBENCH_AB("s:d_ip_mid", iters, 17u,
                          DBENCH_KEEP(digits_scratch(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 9u)),
                          DBENCH_KEEP(digits_inplace(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 9u)));

                DBENCH_AB("s:d_ip_none", iters, 17u,
                          DBENCH_KEEP(digits_scratch(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 0u)),
                          DBENCH_KEEP(digits_inplace(g_wide, sizeof g_wide, 0u, g_g_mant, 17u, 0u)));
            }

            DBENCH_AB("s:sign", iters, 8u, DBENCH_KEEP(MMGR_CALL(fract.sign, FractioCfg, .bits = (mmgr_u64)g_rec_bits)),
                      DBENCH_KEEP(signbit(g_rec_real) ? 1 : 0));

            DBENCH_AB("s:exp", iters, 8u, DBENCH_KEEP(MMGR_CALL(fract.exp, FractioCfg, .bits = (mmgr_u64)g_rec_bits)),
                      DBENCH_KEEP(frexp_exponent(g_rec_real)));

            DBENCH_AB("s:to_bits", iters, 8u, DBENCH_KEEP(MMGR_CALL(fract.to_bits, FractioCfg, .val = g_rec_real)),
                      DBENCH_KEEP(bits_via_memcpy(g_rec_real)));
        }

        DBENCH_OP("floor_loop", 20000u, DBENCH_KEEP(g_out));

        DBENCH_DONE();
    }
}

DBENCH_MAIN("verba")
