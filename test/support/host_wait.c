/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file host_wait.c
 * @brief embed_word_wait and embed_word_wake on the address wait of the host. That is WaitOnAddress on
 *        Windows, a futex on Linux, __ulock_wait on macOS, and a timed sleep elsewhere. Also
 *        embed_clock_ns and the millisecond clock of the host.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-05
 */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
/* The GNU feature set declares the futex call and the monotonic clock. A build can set it on the command line */
#define _GNU_SOURCE
#endif
#include "host_wait.h"

#include "embed_turnstile.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    embed_atomic_word_value compare_value = args->old_value;

    if (args->nanoseconds == 0u)
    {
        return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word) != args->old_value;
    }
    // WaitOnAddress takes whole milliseconds. The limit rounds up, so a wait below one millisecond waits one.
    // Explicit casts widen the limit for the sum, and narrow the milliseconds, which Backoff.lean bounds at 1074
    const DWORD timeout_ms = (args->nanoseconds == EMBED_WAIT_INFINITE)
                                 ? INFINITE
                                 : (DWORD)(((embed_u64)args->nanoseconds + 999999ULL) / 1000000ULL);

    (void)WaitOnAddress(&args->word->value, &compare_value, sizeof(compare_value), timeout_ms);
    return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word) != args->old_value;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    // WakeByAddressAll takes the address of the word as a plain pointer. The address goes through uintptr_t, so no
    // cast removes the qualifier of the atomic value.
    WakeByAddressAll((void *)(uintptr_t)&args->word->value);
}

embed_word embed_clock_ns(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter_ticks;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter_ticks);
    // The seconds of the ticks and their rest scale apart, so the product stays within 64 bits. Explicit casts
    // convert the tick counts to unsigned, and Windows gives both counts as positive values.
    const embed_u64 clock_ns =
        ((embed_u64)(counter_ticks.QuadPart / frequency.QuadPart) * 1000000000ULL) +
        (((embed_u64)(counter_ticks.QuadPart % frequency.QuadPart) * 1000000000ULL) / (embed_u64)frequency.QuadPart);

    // Explicit cast takes the nanoseconds modulo the word, which is the contract of the clock
    return (embed_word)clock_ns;
}

unsigned long long host_clock_ms(void)
{
    return (unsigned long long)GetTickCount64();
}
#else
#include <time.h>
#if defined(__linux__)
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <stdint.h>
extern int __ulock_wait(uint32_t operation, void *address, uint64_t value, uint32_t timeout_us);
extern int __ulock_wake(uint32_t operation, void *address, uint64_t wake_value);
#define HOST_ULOCK_COMPARE_AND_WAIT 1u
#define HOST_ULOCK_COMPARE_AND_WAIT64 5u
#define HOST_ULOCK_WAKE_ALL 0x00000100u
#define HOST_ULOCK_NO_ERRNO 0x01000000u
#endif

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    if (args->nanoseconds == 0u)
    {
        return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word) != args->old_value;
    }
#if defined(__linux__)
    // The futex compares 32 bits. On this little-endian host that is the whole word of 32 bits, or the low half of a
    // word of 64 bits. Those bits hold the state bits of the lane word and the low bits of its generation. The sleeper
    // tests the whole word again after the wake. The system call takes the address of an atomic word as a plain
    // pointer. The address goes through uintptr_t, so no cast removes the qualifier.
    struct timespec span;
    // Explicit casts narrow the seconds and the nanoseconds of the limit. Backoff.lean bounds the limit at 2^30 ns,
    // and the nanoseconds are below 10^9, so both fit.
    span.tv_sec = (time_t)(args->nanoseconds / 1000000000u);
    span.tv_nsec = (long)(args->nanoseconds % 1000000000u);
    (void)syscall(SYS_futex, (void *)(uintptr_t)&args->word->value, FUTEX_WAIT_PRIVATE, (unsigned int)args->old_value,
                  (args->nanoseconds == EMBED_WAIT_INFINITE) ? NULL : &span, NULL, 0);
#elif defined(__APPLE__)
    // __ulock_wait takes microseconds in 32 bits, 0 for no end. The limit rounds up, and Backoff.lean bounds it
    const uint32_t timeout_us =
        (args->nanoseconds == EMBED_WAIT_INFINITE) ? 0u : (uint32_t)(((embed_u64)args->nanoseconds + 999ULL) / 1000ULL);
    (void)__ulock_wait((sizeof(args->old_value) == 8u ? HOST_ULOCK_COMPARE_AND_WAIT64 : HOST_ULOCK_COMPARE_AND_WAIT) |
                           HOST_ULOCK_NO_ERRNO,
                       (void *)(uintptr_t)&args->word->value, (uint64_t)args->old_value, timeout_us);
#else
    // A host with no address wait sleeps the limit, or one millisecond for no end, and the caller tests again
    const embed_u64 sleep_ns = (args->nanoseconds == EMBED_WAIT_INFINITE) ? 1000000ULL : (embed_u64)args->nanoseconds;
    struct timespec span;
    // Explicit casts narrow the seconds and the nanoseconds of the sleep. Backoff.lean bounds the limit at 2^30 ns,
    // and the nanoseconds are below 10^9, so both fit.
    span.tv_sec = (time_t)(sleep_ns / 1000000000u);
    span.tv_nsec = (long)(sleep_ns % 1000000000u);
    if (EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word) == args->old_value)
    {
        (void)nanosleep(&span, NULL);
    }
#endif
    return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word) != args->old_value;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
#if defined(__linux__)
    (void)syscall(SYS_futex, (void *)(uintptr_t)&args->word->value, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
#elif defined(__APPLE__)
    const uint32_t operation =
        (sizeof(embed_atomic_word_value) == 8u) ? HOST_ULOCK_COMPARE_AND_WAIT64 : HOST_ULOCK_COMPARE_AND_WAIT;

    (void)__ulock_wake(operation | HOST_ULOCK_WAKE_ALL | HOST_ULOCK_NO_ERRNO, (void *)(uintptr_t)&args->word->value,
                       0u);
#else
    (void)args->word;
#endif
}

embed_word embed_clock_ns(void)
{
    struct timespec clock_time;
    (void)clock_gettime(CLOCK_MONOTONIC, &clock_time);
    // Explicit casts convert the seconds and the nanoseconds to unsigned, and the monotonic clock gives no negative
    // field
    const embed_u64 clock_ns = ((embed_u64)clock_time.tv_sec * 1000000000ULL) + (embed_u64)clock_time.tv_nsec;

    // Explicit cast takes the nanoseconds modulo the word, which is the contract of the clock
    return (embed_word)clock_ns;
}

unsigned long long host_clock_ms(void)
{
    struct timespec clock_time;
    (void)clock_gettime(CLOCK_MONOTONIC, &clock_time);
    // Explicit casts convert the seconds and the nanoseconds to unsigned, and the monotonic clock gives no negative
    // field
    return ((unsigned long long)clock_time.tv_sec * 1000u) + ((unsigned long long)clock_time.tv_nsec / 1000000u);
}
#endif

/** @brief The thread-local storage class of the compiler: C11's keyword, or the declaration specifier of MSVC. */
#if defined(_MSC_VER) && !defined(__clang__)
#define HOST_THREAD_LOCAL __declspec(thread)
#else
#define HOST_THREAD_LOCAL _Thread_local
#endif

/** @brief The places given out so far. The next thread that calls takes the count as its place. */
static embed_atomic_u32 s_places_given = EMBED_ATOMIC_INIT(0u);

/** @brief The place of this thread plus one, and 0 before its first call. */
static HOST_THREAD_LOCAL embed_u32 t_place_plus_one;

embed_u32 embed_thread_place(void)
{
    if (t_place_plus_one == 0u)
    {
        t_place_plus_one = EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg,
                                      .word = &s_places_given, .value = 1u) +
                           1u;
    }
    return t_place_plus_one - 1u;
}
