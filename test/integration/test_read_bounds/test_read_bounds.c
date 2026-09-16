#include "unity.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "verbum_scrutor/verbum_scrutor.h"

#include "guard_page.h"
#include "oracle_divergence.h"

#include <stdio.h>
#include <string.h>

#define CAPS 200u

void setUp(void)
{
}

void tearDown(void)
{
}

static size_t word_rounded(size_t n)
{
    return EMBED_CALL(word.count, ScrutWordCfg, .bytes = n) * MMGR_SWAR_BYTES;
}

static unsigned char *place(size_t cap, size_t reserved)
{
    unsigned char *run = mmgr_guard_run();
    const size_t page = mmgr_guard_page_size();

    (void)cap;
    memset(run, 'a', page);
    return run + page - reserved;
}

typedef struct
{
    const char *s;
    size_t cap;
} Ask;

static size_t mmgr_probe_sink;
static void keep(size_t v)
{
    mmgr_probe_sink += v;
}

static void ask_len(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = a->s, .cap = a->cap)));
}
static void ask_chr(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = a->s, .cap = a->cap, .byte = 0x02u)));
}
static void ask_eq(void *v)
{
    const Ask *a = (const Ask *)v;
    keep(
        (size_t)(EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = a->s, .other = a->s, .cap = a->cap, .ci = EMBED_FALSE)));
}
static void ask_eq_ci(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = a->s, .other = a->s, .cap = a->cap, .ci = EMBED_TRUE)));
}
static void ask_starts(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = a->s, .other = "aaa", .cap = a->cap,
                             .ci = EMBED_FALSE)));
}
static void ask_diff(void *v)
{
    const Ask *a = (const Ask *)v;
    keep((size_t)(EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = a->s, .other = a->s, .cap = a->cap,
                             .ci = EMBED_FALSE)));
}
static void ask_copy(void *v)
{
    const Ask *a = (const Ask *)v;
    static char dst[CAPS + 8u];
    keep((size_t)(EMBED_CALL(cellul.copy, CatenaFinitaCfg, .dst = dst, .src = a->s,
                             .cap = a->cap < sizeof dst ? a->cap : sizeof dst)));
}
static void ask_memor_cmp(void *v)
{
    const Ask *a = (const Ask *)v;

    keep((size_t)(EMBED_CALL(memor.cmp, MemoriaCfg, .src = a->s, .other = a->s, .bytes = a->cap)));
}
static void ask_memor_chr(void *v)
{
    const Ask *a = (const Ask *)v;

    keep((size_t)(EMBED_CALL(memor.chr, MemoriaCfg, .src = a->s, .bytes = a->cap, .val = 0x02u)));
}
typedef struct
{
    const char *s;
    size_t cap;
    const char *needle;
    size_t nlen;
    embed_bool ci;
} Hunt;

static void ask_find(void *v)
{
    const Hunt *h = (const Hunt *)v;
    keep((size_t)(EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = h->s, .cap = h->cap, .other = h->needle,
                             .other_cap = h->nlen, .ci = h->ci)));
}
static void ask_has(void *v)
{
    const Hunt *h = (const Hunt *)v;
    keep((size_t)(EMBED_CALL(cellul.has, CatenaFinitaCfg, .src = h->s, .cap = h->cap, .other = h->needle,
                             .other_cap = h->nlen, .ci = h->ci)));
}

static void none_past(const char *what, void (*fn)(void *), int raw_bound)
{
    for (size_t cap = 1; cap <= CAPS; cap++)
    {
        const size_t reserved = raw_bound ? cap : word_rounded(cap);
        Ask a;
        a.s = (const char *)place(cap, reserved);
        a.cap = cap;

        if (mmgr_guard_run_thunk(fn, &a))
        {
            char msg[128];
            (void)snprintf(msg, sizeof msg, "%s read past its bound at cap %zu (reserved %zu)", what, cap, reserved);
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

static void needs_our_bounds(void)
{
    MMGR_SKIP_ON_ORACLE("a read cap is this library's, and libc's equivalents do not take one");

    if (!mmgr_guard_available())
    {
        TEST_IGNORE_MESSAGE("no page protection on this platform, so there is no instrument to read a bound with");
    }
}

void test_the_guard_is_armed(void)
{
    needs_our_bounds();

    unsigned char *run = mmgr_guard_run();
    const size_t page = mmgr_guard_page_size();

    TEST_ASSERT_TRUE_MESSAGE(mmgr_guard_traps_on(run + page), "the tail guard did not trap, so nothing below means "
                                                              "anything");
    TEST_ASSERT_TRUE_MESSAGE(mmgr_guard_traps_on(run - 1), "the head guard did not trap");
    TEST_ASSERT_FALSE_MESSAGE(mmgr_guard_traps_on(run), "the run itself must be readable");
}

void test_len_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("len", ask_len, 0);
}

void test_chr_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("chr", ask_chr, 0);
}

void test_eq_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("eq", ask_eq, 0);
    none_past("eq ignoring case", ask_eq_ci, 0);
}

void test_starts_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("starts", ask_starts, 0);
}

void test_diff_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("diff", ask_diff, 0);
}

void test_copy_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("copy", ask_copy, 0);
}

void test_memor_cmp_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("memor.cmp", ask_memor_cmp, 0);
}

void test_memor_chr_stays_inside_the_reserved_extent(void)
{
    needs_our_bounds();
    none_past("memor.chr", ask_memor_chr, 0);
}

static void find_none_past(const char *what, void (*fn)(void *))
{
    for (size_t nlen = 1; nlen <= 2u * MMGR_SWAR_BYTES; nlen++)
    {
        for (size_t rare = 0; rare < nlen; rare++)
        {
            char needle[2u * MMGR_SWAR_BYTES + 1u];
            memset(needle, 'e', nlen);
            needle[rare] = 'q';
            needle[nlen] = '\0';

            for (size_t cap = nlen; cap <= CAPS; cap++)
            {
                for (int ci = 0; ci <= 1; ci++)
                {
                    Hunt h;
                    h.s = (const char *)place(cap, cap);
                    h.cap = cap;
                    h.needle = needle;
                    h.nlen = nlen;
                    h.ci = ci ? EMBED_TRUE : EMBED_FALSE;

                    if (mmgr_guard_run_thunk(fn, &h))
                    {
                        char msg[160];
                        (void)snprintf(msg, sizeof msg,
                                       "%s read past read_cap: cap %zu, needle %zu, rare byte at %zu, ci %d", what, cap,
                                       nlen, rare, ci);
                        TEST_FAIL_MESSAGE(msg);
                    }
                }
            }
        }
    }
}

void test_find_stays_inside_the_raw_cap(void)
{
    needs_our_bounds();
    find_none_past("find", ask_find);
}

void test_has_stays_inside_the_raw_cap(void)
{
    needs_our_bounds();
    find_none_past("has", ask_has);
}

void test_find_still_finds_things_with_the_buffer_flush_to_the_guard(void)
{
    needs_our_bounds();

    for (size_t cap = 8u; cap <= CAPS; cap++)
    {
        unsigned char *p = place(cap, cap);
        memset(p, 'e', cap);
        memcpy(p + cap - 3u, "qzj", 3u);

        Hunt h;
        h.s = (const char *)p;
        h.cap = cap;
        h.needle = "qzj";
        h.nlen = 3u;
        h.ci = EMBED_FALSE;

        TEST_ASSERT_FALSE_MESSAGE(mmgr_guard_run_thunk(ask_find, &h), "find read past the cap looking for a match");
        TEST_ASSERT_EQUAL_PTR_MESSAGE((const char *)p + cap - 3u,
                                      EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = h.s, .cap = cap, .other = "qzj",
                                                 .other_cap = 3u, .ci = EMBED_FALSE),
                                      "find missed a match flush with the end of the buffer");
    }
}
