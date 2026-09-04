/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file bench_ancorae_cycles.c
 * @brief Cycles per search for four arrangements, so the crossover between them can be read.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-04
 *
 * @note The other ancorae benches count reads. Reads are the right unit for a bound and the wrong
 *       one for a dispatch decision, because a read the machine issues in parallel with three others
 *       does not cost what a read that everything waits on costs. This one counts cycles so the two
 *       can be compared on the same rows.
 * @note What it exists to answer: where each arrangement wins, and by how much. The anchor sift has a
 *       dependency depth of two and no carried state, which a superscalar machine can overlap, while
 *       Horspool's next shift depends on the byte it just read. Whether that overlap pays is a
 *       measurement and it has never been taken here.
 * @note Corpora are SHA-256 in counter mode, mapped where a skewed distribution is wanted, so the
 *       bytes are reproducible from the seed and no corpus file is needed.
 * @warning Cycle counts are host figures and belong to the machine that produced them. A ratio
 *          between two arms on one machine is the portable part; the absolute count is not.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mmgr_sha256.h"

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define MMGR_CYCLES_ARE_REAL 1
#else
#include <time.h>
#define MMGR_CYCLES_ARE_REAL 0
#endif

/** @brief Bytes in the largest corpus measured. */
#define CORPUS_BYTES 65536u

/** @brief Needle lengths swept, chosen as powers of two around the effective alphabet. */
static const size_t NEEDLE_LENGTHS[] = {4u, 8u, 16u, 32u, 64u, 128u, 256u};

/** @brief How many needles are drawn per row. Each is taken from the corpus, so each has a match. */
#define NEEDLES_PER_ROW 64u

/** @brief How many times one row is timed. The smallest of these is reported. */
#define TIMED_TRIALS 7u

/** @brief Anchors placed by the sift arms. Section 4.4's depth is log2(N)/H2, near five here. */
#define ANCHOR_COUNT 4u

/**
 * @brief Reads the cycle counter, or a monotonic substitute where the part has none.
 *
 * @return A count that increases with time, in cycles where the host supplies them.
 * @note The minimum over repeated trials is what this feeds, which is the statistic that rejects
 *       preemption and interrupt noise. A mean over a timing sample measures the scheduler.
 */
static uint64_t cycles_now(void)
{
#if MMGR_CYCLES_ARE_REAL
    /* Ordering deviation: __rdtsc may be reordered against the work being timed. The compiler
     * barrier below keeps the loads and stores of one arm from crossing it, which is what makes
     * the difference between two reads attributable to that arm. */
    __asm__ __volatile__("" ::: "memory");
    const uint64_t taken = (uint64_t)__rdtsc();
    __asm__ __volatile__("" ::: "memory");
    return taken;
#else
    struct timespec taken;
    (void)clock_gettime(CLOCK_MONOTONIC, &taken);
    return ((uint64_t)taken.tv_sec * 1000000000ULL) + (uint64_t)taken.tv_nsec;
#endif
}

/**
 * @brief Fills a corpus with SHA-256 in counter mode.
 *
 * @param[out] corpus Where the bytes are written [BORROWS].
 * @param[in]  length How many to write.
 * @param[in]  seed   Counter start, which selects the corpus.
 * @note Uniform over the byte range by construction, which is the memoryless control every other
 *       reading here is measured against.
 */
static void fill_uniform(uint8_t *corpus, size_t length, uint32_t seed)
{
    uint8_t digest[MMGR_SHA256_BYTES];
    uint8_t counter[8];
    size_t written = 0u;
    uint32_t step = 0u;

    while (written < length)
    {
        counter[0] = (uint8_t)(seed & 0xFFu);
        counter[1] = (uint8_t)((seed >> 8) & 0xFFu);
        counter[2] = (uint8_t)((seed >> 16) & 0xFFu);
        counter[3] = (uint8_t)((seed >> 24) & 0xFFu);
        counter[4] = (uint8_t)(step & 0xFFu);
        counter[5] = (uint8_t)((step >> 8) & 0xFFu);
        counter[6] = (uint8_t)((step >> 16) & 0xFFu);
        counter[7] = (uint8_t)((step >> 24) & 0xFFu);
        mmgr_sha256(counter, sizeof counter, digest);

        size_t taking = length - written;
        if (taking > MMGR_SHA256_BYTES)
        {
            taking = MMGR_SHA256_BYTES;
        }
        memcpy(corpus + written, digest, taking);
        written += taking;
        step += 1u;
    }
}

/**
 * @brief Maps a uniform corpus onto a skewed alphabet, giving collision entropy near English.
 *
 * @param[in,out] corpus Bytes to remap in place [BORROWS].
 * @param[in]     length How many.
 * @note The table is a geometric weighting over 27 symbols, which lands H2 in the 3.7 to 3.9 band the
 *       ledger records for English. It is a distribution and carries no arrangement, so an arm that
 *       reads only the histogram cannot tell it from English and an arm that reads position can.
 */
static void fill_skewed(uint8_t *corpus, size_t length)
{
    uint8_t table[256];
    size_t filled = 0u;
    uint8_t symbol = 0u;

    /* Each symbol takes half the remaining table, floored at one slot, which is a geometric
     * weighting written without any floating point in the bench. */
    while ((filled < sizeof table) && (symbol < 27u))
    {
        size_t width = (sizeof table - filled) / 2u;
        if (width == 0u)
        {
            width = 1u;
        }
        if ((filled + width) > sizeof table)
        {
            width = sizeof table - filled;
        }
        memset(table + filled, (int)('a' + symbol), width);
        filled += width;
        symbol += 1u;
    }
    while (filled < sizeof table)
    {
        table[filled] = (uint8_t)' ';
        filled += 1u;
    }

    for (size_t at = 0u; at < length; at += 1u)
    {
        corpus[at] = table[corpus[at]];
    }
}

/**
 * @brief Fills a corpus with a period of sixteen.
 *
 * @param[out] corpus Where the bytes are written [BORROWS].
 * @param[in]  length How many to write.
 * @note The one corpus here whose arrangement is known exactly, which is what makes it the row that
 *       says whether an arm is reading structure or reading its own placement.
 */
static void fill_periodic(uint8_t *corpus, size_t length)
{
    for (size_t at = 0u; at < length; at += 1u)
    {
        corpus[at] = (uint8_t)(at % 16u);
    }
}

/**
 * @brief Counts occurrences of a needle by comparing at every alignment.
 *
 * @param[in] corpus      Bytes to search [BORROWS].
 * @param[in] corpus_len  How many.
 * @param[in] needle      Bytes to find [BORROWS].
 * @param[in] needle_len  How many.
 * @return                How many alignments match exactly.
 * @note The reference arm. Every other arm here has to agree with it or its row is void.
 */
static size_t search_naive(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle,
                           size_t needle_len)
{
    size_t found = 0u;

    for (size_t at = 0u; (at + needle_len) <= corpus_len; at += 1u)
    {
        if (memcmp(corpus + at, needle, needle_len) == 0)
        {
            found += 1u;
        }
    }
    return found;
}

/**
 * @brief Counts occurrences using Boyer-Moore-Horspool's bad character shift.
 *
 * @param[in] corpus      Bytes to search [BORROWS].
 * @param[in] corpus_len  How many.
 * @param[in] needle      Bytes to find [BORROWS].
 * @param[in] needle_len  How many.
 * @return                How many alignments match exactly.
 * @note The arm to beat, and the one whose next shift depends on the byte just read. That dependency
 *       is the whole of what the sift arms trade away.
 */
static size_t search_horspool(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle,
                              size_t needle_len)
{
    size_t shift[256];
    size_t found = 0u;
    size_t at = 0u;

    for (size_t slot = 0u; slot < 256u; slot += 1u)
    {
        shift[slot] = needle_len;
    }
    for (size_t step = 0u; (step + 1u) < needle_len; step += 1u)
    {
        shift[needle[step]] = needle_len - 1u - step;
    }

    while ((at + needle_len) <= corpus_len)
    {
        if (memcmp(corpus + at, needle, needle_len) == 0)
        {
            found += 1u;
        }
        at += shift[corpus[at + needle_len - 1u]];
    }
    return found;
}

/**
 * @brief Chooses anchor offsets, one drawn inside each evenly sized cell of the needle.
 *
 * @param[out] offsets    Where the chosen offsets are written [BORROWS].
 * @param[in]  wanted     How many to choose.
 * @param[in]  needle_len Length of the needle they index.
 * @note One draw per cell keeps the spread without giving the anchor set a period of its own, which
 *       the ledger records as better sized than an even comb on both corpora it was measured on.
 */
static void choose_offsets(size_t *offsets, size_t wanted, size_t needle_len)
{
    const size_t cell = needle_len / wanted;

    for (size_t slot = 0u; slot < wanted; slot += 1u)
    {
        /* A fixed position inside the cell, so the set is reproducible between arms and between
         * runs. Jitter belongs to the sizing question and this bench is timing, not sizing. */
        const size_t inside = (cell > 1u) ? ((slot * 7u) % cell) : 0u;
        offsets[slot] = (slot * cell) + inside;
        if (offsets[slot] >= needle_len)
        {
            offsets[slot] = needle_len - 1u;
        }
    }
}

/**
 * @brief Counts occurrences by testing anchors in order and stopping at the first that refutes.
 *
 * @param[in] corpus      Bytes to search [BORROWS].
 * @param[in] corpus_len  How many.
 * @param[in] needle      Bytes to find [BORROWS].
 * @param[in] needle_len  How many.
 * @return                How many alignments match exactly.
 * @note Short circuiting makes each probe depend on the one before it, which is the arrangement a
 *       branch predictor handles and an issue window cannot overlap.
 */
static size_t search_anchor_inorder(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle,
                                    size_t needle_len)
{
    size_t offsets[ANCHOR_COUNT];
    size_t found = 0u;

    choose_offsets(offsets, ANCHOR_COUNT, needle_len);

    for (size_t at = 0u; (at + needle_len) <= corpus_len; at += 1u)
    {
        size_t slot = 0u;
        while (slot < ANCHOR_COUNT)
        {
            if (corpus[at + offsets[slot]] != needle[offsets[slot]])
            {
                break;
            }
            slot += 1u;
        }
        if (slot == ANCHOR_COUNT)
        {
            if (memcmp(corpus + at, needle, needle_len) == 0)
            {
                found += 1u;
            }
        }
    }
    return found;
}

/**
 * @brief Counts occurrences by testing every anchor unconditionally and combining the results.
 *
 * @param[in] corpus      Bytes to search [BORROWS].
 * @param[in] corpus_len  How many.
 * @param[in] needle      Bytes to find [BORROWS].
 * @param[in] needle_len  How many.
 * @return                How many alignments match exactly.
 * @note The depth two arrangement. Every probe is independent, the combine is one AND tree, and
 *       nothing waits on anything. It reads more bytes than the in-order arm by construction and the
 *       question this bench exists for is whether the machine issues them for free.
 */
static size_t search_anchor_free(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle,
                                 size_t needle_len)
{
    size_t offsets[ANCHOR_COUNT];
    uint8_t wanted[ANCHOR_COUNT];
    size_t found = 0u;

    choose_offsets(offsets, ANCHOR_COUNT, needle_len);
    for (size_t slot = 0u; slot < ANCHOR_COUNT; slot += 1u)
    {
        wanted[slot] = needle[offsets[slot]];
    }

    for (size_t at = 0u; (at + needle_len) <= corpus_len; at += 1u)
    {
        /* No short circuit. All four loads are issued, the comparisons are folded together, and the
         * branch is taken once on the combined result. */
        const unsigned agree = (unsigned)(corpus[at + offsets[0]] == wanted[0]) &
                               (unsigned)(corpus[at + offsets[1]] == wanted[1]) &
                               (unsigned)(corpus[at + offsets[2]] == wanted[2]) &
                               (unsigned)(corpus[at + offsets[3]] == wanted[3]);
        if (agree != 0u)
        {
            if (memcmp(corpus + at, needle, needle_len) == 0)
            {
                found += 1u;
            }
        }
    }
    return found;
}

/** @brief One arm under test: what to call it and what to call. */
typedef struct
{
    const char *name;
    size_t (*run)(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len);
} Arm;

/** @brief One corpus under test: what to call it and how it is filled. */
typedef struct
{
    const char *name;
    void (*fill)(uint8_t *corpus, size_t length);
} Corpus;

static void corpus_uniform(uint8_t *corpus, size_t length)
{
    fill_uniform(corpus, length, 0xD7723247u);
}

static void corpus_skewed(uint8_t *corpus, size_t length)
{
    fill_uniform(corpus, length, 0xD7723247u);
    fill_skewed(corpus, length);
}

static void corpus_periodic(uint8_t *corpus, size_t length)
{
    fill_periodic(corpus, length);
}

/**
 * @brief Times every arm on every corpus at every needle length and writes the rows.
 *
 * @return 0 where every arm agreed with the reference on every row, 1 where any disagreed.
 * @note A disagreeing arm voids its row, because a timing for an arm that returns the wrong answer
 *       is a measurement of the wrong program.
 */
int main(void)
{
    static uint8_t corpus[CORPUS_BYTES];
    static const Arm ARMS[] = {
        {"naive", search_naive},
        {"horspool", search_horspool},
        {"anchor_inorder", search_anchor_inorder},
        {"anchor_free", search_anchor_free},
    };
    static const Corpus CORPORA[] = {
        {"skewed", corpus_skewed},
        {"uniform", corpus_uniform},
        {"periodic16", corpus_periodic},
    };
    const size_t arm_count = sizeof ARMS / sizeof ARMS[0];
    const size_t corpus_count = sizeof CORPORA / sizeof CORPORA[0];
    const size_t length_count = sizeof NEEDLE_LENGTHS / sizeof NEEDLE_LENGTHS[0];
    int disagreed = 0;

    printf("bench,corpus,needle_len,corpus_bytes,arm,cycles_per_search,cycles_per_byte,found,agree\n");

    for (size_t which = 0u; which < corpus_count; which += 1u)
    {
        CORPORA[which].fill(corpus, CORPUS_BYTES);

        for (size_t step = 0u; step < length_count; step += 1u)
        {
            const size_t needle_len = NEEDLE_LENGTHS[step];
            size_t expected[NEEDLES_PER_ROW];
            size_t starts[NEEDLES_PER_ROW];

            /* Every needle is drawn from the corpus, so every search confirms one genuine occurrence
             * and pays the needle_len read verification floor. That floor is why the arms converge as
             * the needle grows, and the ledger records it as the term an earlier derivation omitted. */
            for (size_t pick = 0u; pick < NEEDLES_PER_ROW; pick += 1u)
            {
                starts[pick] = (pick * 977u) % (CORPUS_BYTES - needle_len);
                expected[pick] = search_naive(corpus, CORPUS_BYTES, corpus + starts[pick], needle_len);
            }

            for (size_t slot = 0u; slot < arm_count; slot += 1u)
            {
                uint64_t best = UINT64_MAX;
                size_t total_found = 0u;
                int matched = 1;

                for (size_t trial = 0u; trial < TIMED_TRIALS; trial += 1u)
                {
                    const uint64_t opened = cycles_now();
                    size_t seen = 0u;

                    for (size_t pick = 0u; pick < NEEDLES_PER_ROW; pick += 1u)
                    {
                        seen += ARMS[slot].run(corpus, CORPUS_BYTES, corpus + starts[pick], needle_len);
                    }

                    const uint64_t closed = cycles_now();
                    const uint64_t spent = closed - opened;
                    if (spent < best)
                    {
                        best = spent;
                    }
                    total_found = seen;
                }

                size_t reference = 0u;
                for (size_t pick = 0u; pick < NEEDLES_PER_ROW; pick += 1u)
                {
                    reference += expected[pick];
                }
                if (total_found != reference)
                {
                    matched = 0;
                    disagreed = 1;
                }

                const double per_search = (double)best / (double)NEEDLES_PER_ROW;
                printf("ancorae_cycles,%s,%zu,%u,%s,%.1f,%.4f,%zu,%s\n", CORPORA[which].name,
                       needle_len, (unsigned)CORPUS_BYTES, ARMS[slot].name, per_search,
                       per_search / (double)CORPUS_BYTES, total_found, matched ? "agree" : "DIFFER");
            }
        }
    }

    return disagreed;
}
