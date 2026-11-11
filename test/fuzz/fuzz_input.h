/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file fuzz_input.h
 * @brief The reader every fuzz harness takes its input through. Bytes, words and lengths drawn off the front of
 *        libFuzzer's input, and zeros once it runs out. Also the stop a harness calls when its oracle disagrees.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-06
 */
#ifndef MMGR_FUZZ_INPUT_H
#define MMGR_FUZZ_INPUT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/** @brief The input a harness reads: its bytes, how many there are and how many came out. */
typedef struct
{
    const uint8_t *data;
    size_t size;
    size_t at;
} FuzzInput;

/** @brief The reader over libFuzzer's input. */
static inline FuzzInput fuzz_input(const uint8_t *data, size_t size)
{
    return (FuzzInput){data, size, 0u};
}

/** @brief Whether bytes remain to read. */
static inline int fuzz_left(const FuzzInput *input)
{
    return input->at < input->size;
}

/** @brief The next byte, or 0 once the input runs out. */
static inline uint8_t fuzz_byte(FuzzInput *input)
{
    if (input->at >= input->size)
    {
        return 0u;
    }
    const uint8_t byte = input->data[input->at];
    input->at += 1u;
    return byte;
}

/** @brief The next eight bytes as a little-endian word, zeros past the end. */
static inline uint64_t fuzz_word(FuzzInput *input)
{
    uint64_t gathered = 0u;
    for (unsigned int shift = 0u; shift < 64u; shift += 8u)
    {
        gathered |= (uint64_t)fuzz_byte(input) << shift;
    }
    return gathered;
}

/** @brief A count below @p most, drawn from two bytes. */
static inline size_t fuzz_below(FuzzInput *input, size_t most)
{
    const size_t drawn = (size_t)fuzz_byte(input) | ((size_t)fuzz_byte(input) << 8);
    return (most == 0u) ? 0u : (drawn % most);
}

/** @brief A word skewed toward the edges: zero, all ones, a single bit, a byte, or any word. */
static inline uint64_t fuzz_edge_word(FuzzInput *input)
{
    const uint8_t kind = fuzz_byte(input);
    switch (kind & 7u)
    {
    case 0:
        return 0u;
    case 1:
        return ~(uint64_t)0;
    case 2:
        return (uint64_t)1 << (kind >> 2);
    case 3:
        return ~((uint64_t)1 << (kind >> 2));
    case 4:
        return (uint64_t)fuzz_byte(input);
    default:
        return fuzz_word(input);
    }
}

/** @brief The stop a harness calls where its module and its oracle disagree, with what disagreed. */
static inline void fuzz_failed(const char *what)
{
    fprintf(stderr, "fuzz: %s\n", what);
    abort();
}

/** @brief The check a harness makes of each answer: a false @p agrees stops the run with @p what. */
static inline void fuzz_check(int agrees, const char *what)
{
    if (!agrees)
    {
        fuzz_failed(what);
    }
}

#endif
