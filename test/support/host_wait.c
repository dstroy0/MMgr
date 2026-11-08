/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file host_wait.c
 * @brief embed_word_wait and embed_word_wake on the host's address wait: WaitOnAddress on Windows, a futex
 *        on Linux, __ulock_wait on macOS, and a timed sleep elsewhere; embed_clock_ns and the host's
 *        millisecond clock.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-05
 */
#if !defined(_WIN32)
/* the futex call and the monotonic clock are declared under the GNU feature set */
#define _GNU_SOURCE
#endif
#include "host_wait.h"

#include "embed_turnstile.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u64 nanoseconds)
{
    embed_atomic_word_value was = seen;

    if (nanoseconds == 0u)
    {
        return embed_atomic_word_load(word) != seen;
    }
    // WaitOnAddress takes whole milliseconds: the limit is rounded up, so a wait below one millisecond waits one.
    // Explicit cast narrows the milliseconds, which Backoff.lean bounds at 1074 for each limit below forever.
    const DWORD waited =
        (nanoseconds == EMBED_WAIT_FOREVER) ? INFINITE : (DWORD)((nanoseconds + 999999u) / 1000000u);

    (void)WaitOnAddress(&word->value, &was, sizeof(was), waited);
    return embed_atomic_word_load(word) != seen;
}

void embed_word_wake(embed_atomic_word *word)
{
    WakeByAddressAll(&word->value);
}

embed_u64 embed_clock_ns(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER count;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&count);
    // the count's seconds and its rest scaled apart, so the product stays within 64 bits
    return ((embed_u64)(count.QuadPart / frequency.QuadPart) * 1000000000ull) +
           (((embed_u64)(count.QuadPart % frequency.QuadPart) * 1000000000ull) / (embed_u64)frequency.QuadPart);
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

int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u64 nanoseconds)
{
    if (nanoseconds == 0u)
    {
        return embed_atomic_word_load(word) != seen;
    }
#if defined(__linux__)
    // The futex compares 32 bits: on this little-endian host the low half of the word, where the lane word's state
    // bits and the low bits of its generation lie. The sleeper tests the whole word again on waking. The system call
    // takes the address of an atomic word as a plain pointer, and the address goes through uintptr_t so that no
    // cast removes the qualifier.
    struct timespec span;
    span.tv_sec = (time_t)(nanoseconds / 1000000000u);
    span.tv_nsec = (long)(nanoseconds % 1000000000u);
    (void)syscall(SYS_futex, (void *)(uintptr_t)&word->value, FUTEX_WAIT_PRIVATE, (unsigned int)seen,
                  (nanoseconds == EMBED_WAIT_FOREVER) ? NULL : &span, NULL, 0);
#elif defined(__APPLE__)
    // __ulock_wait takes microseconds in 32 bits, 0 for no end: the limit is rounded up, and Backoff.lean bounds it
    const uint32_t timeout =
        (nanoseconds == EMBED_WAIT_FOREVER) ? 0u : (uint32_t)((nanoseconds + 999u) / 1000u);
    (void)__ulock_wait((sizeof(seen) == 8u ? HOST_ULOCK_COMPARE_AND_WAIT64 : HOST_ULOCK_COMPARE_AND_WAIT) |
                           HOST_ULOCK_NO_ERRNO,
                       (void *)(uintptr_t)&word->value, (uint64_t)seen, timeout);
#else
    // A host with no address wait sleeps the limit, or one millisecond for no end, and the caller tests again
    const embed_u64 slept = (nanoseconds == EMBED_WAIT_FOREVER) ? 1000000u : nanoseconds;
    struct timespec span;
    span.tv_sec = (time_t)(slept / 1000000000u);
    span.tv_nsec = (long)(slept % 1000000000u);
    if (embed_atomic_word_load(word) == seen)
    {
        (void)nanosleep(&span, NULL);
    }
#endif
    return embed_atomic_word_load(word) != seen;
}

void embed_word_wake(embed_atomic_word *word)
{
#if defined(__linux__)
    (void)syscall(SYS_futex, (void *)(uintptr_t)&word->value, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
#elif defined(__APPLE__)
    (void)__ulock_wake((sizeof(embed_atomic_word_value) == 8u ? HOST_ULOCK_COMPARE_AND_WAIT64 : HOST_ULOCK_COMPARE_AND_WAIT) |
                           HOST_ULOCK_WAKE_ALL | HOST_ULOCK_NO_ERRNO,
                       (void *)(uintptr_t)&word->value, 0u);
#else
    (void)word;
#endif
}

embed_u64 embed_clock_ns(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return ((embed_u64)now.tv_sec * 1000000000ull) + (embed_u64)now.tv_nsec;
}

unsigned long long host_clock_ms(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return ((unsigned long long)now.tv_sec * 1000u) + ((unsigned long long)now.tv_nsec / 1000000u);
}
#endif
