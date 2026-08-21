// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The POSIX branch below needs declarations glibc hides from this build. CMAKE_C_STANDARD 11 with
// CMAKE_C_EXTENSIONS OFF is -std=c11, which defines __STRICT_ANSI__, and glibc then exposes only
// the ISO C subset of <signal.h>, <sys/mman.h> and <unistd.h>. struct sigaction, sigaction,
// sigemptyset, mmap, mprotect and sysconf all vanish, and the file fails with "storage size of
// 'sa' isn't known" - which names the symptom and not one word of the cause.
//
// _POSIX_C_SOURCE brings the POSIX ones back. MAP_ANONYMOUS is not POSIX, it is a BSD extension
// glibc gates behind _DEFAULT_SOURCE, and asking for _POSIX_C_SOURCE explicitly is what switches
// _DEFAULT_SOURCE off - so both are needed, and needing both is not obvious from either.
//
// Here rather than in CMAKE_C_FLAGS because a workflow that sets CMAKE_C_FLAGS for its own reasons
// replaces that variable rather than appending to it, and this file then stops compiling in that
// one build while every other build stays green. A feature test macro belongs to the translation
// unit that needs it. They must also precede every include, this file's own header included.
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#endif

#include "guard_page.h"

#if MMGR_GUARD_SUPPORTED

#include <setjmp.h>

/*
 * Leaving a signal handler by jumping out of it has to restore the signal mask, and on POSIX
 * plain longjmp does not.
 *
 * The kernel blocks SIGSEGV for the duration of the handler and unblocks it when the handler
 * returns normally. longjmp never returns normally, so the unblock never happens and SIGSEGV stays
 * blocked for the rest of the process. The first guarded read traps and is caught; the second
 * faults with SIGSEGV already blocked, which the kernel resolves by forcing the default action.
 * The suite dies on its second probe with no diagnostic - which is what it did.
 *
 * sigsetjmp with a nonzero second argument saves the mask, and siglongjmp puts it back. Windows
 * has no signal mask and no sigsetjmp, so it keeps the plain pair.
 */
#if defined(_WIN32)
typedef jmp_buf mmgr_guard_jmp;
#define MMGR_GUARD_SETJMP(b) setjmp(b)
#define MMGR_GUARD_LONGJMP(b) longjmp((b), 1)
#else
typedef sigjmp_buf mmgr_guard_jmp;
#define MMGR_GUARD_SETJMP(b) sigsetjmp((b), 1)
#define MMGR_GUARD_LONGJMP(b) siglongjmp((b), 1)
#endif

static mmgr_guard_jmp s_trap;
/* Written by a fault handler and read by the code that armed it. That is a concurrent access and
   an atomic is what says so - volatile describes storage that changes underneath you for reasons
   outside the program, which is not this. */
static _Atomic int s_trapped;
static _Atomic int s_armed;

/** @brief Where the probing read goes, so that it is a read. */
static _Atomic unsigned char s_touched;

#if defined(_WIN32)

#include <windows.h>

static LONG CALLBACK on_fault(PEXCEPTION_POINTERS info)
{
    if (s_armed && info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
        s_trapped = 1;
        MMGR_GUARD_LONGJMP(s_trap);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static size_t page_bytes(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
}

/** @brief Three pages, the middle one writable and the outer two not. */
static unsigned char *reserve(size_t page)
{
    unsigned char *p = (unsigned char *)VirtualAlloc(NULL, page * 3u, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD old;

    if (p == NULL)
    {
        return NULL;
    }
    if (!VirtualProtect(p, page, PAGE_NOACCESS, &old) || !VirtualProtect(p + page * 2u, page, PAGE_NOACCESS, &old))
    {
        return NULL;
    }
    AddVectoredExceptionHandler(1, on_fault);
    return p;
}

#else

#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

static void on_fault(int sig)
{
    (void)sig;
    if (s_armed)
    {
        s_trapped = 1;
        MMGR_GUARD_LONGJMP(s_trap);
    }
    _exit(1);
}

static size_t page_bytes(void)
{
    return (size_t)sysconf(_SC_PAGESIZE);
}

static unsigned char *reserve(size_t page)
{
    unsigned char *p =
        (unsigned char *)mmap(NULL, page * 3u, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    struct sigaction sa;

    if (p == MAP_FAILED)
    {
        return NULL;
    }
    if (mprotect(p, page, PROT_NONE) != 0 || mprotect(p + page * 2u, page, PROT_NONE) != 0)
    {
        return NULL;
    }

    sa.sa_handler = on_fault;
    sa.sa_flags = 0;
    (void)sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGSEGV, &sa, NULL);
    (void)sigaction(SIGBUS, &sa, NULL);
    return p;
}

#endif

static size_t s_page;
static unsigned char *s_region;

/** @brief Reserve once, on the first question anyone asks. */
static void ensure(void)
{
    if (s_page == 0u)
    {
        s_page = page_bytes();
        s_region = reserve(s_page);
        if (s_region == NULL)
        {
            s_page = 0u;
        }
    }
}

int mmgr_guard_available(void)
{
    ensure();
    return s_region != NULL;
}

size_t mmgr_guard_page_size(void)
{
    ensure();
    return s_region != NULL ? s_page : 0u;
}

unsigned char *mmgr_guard_run(void)
{
    ensure();
    return s_region != NULL ? s_region + s_page : NULL;
}

int mmgr_guard_traps_on(const unsigned char *p)
{
    ensure();
    s_trapped = 0;
    s_armed = 1;
    if (MMGR_GUARD_SETJMP(s_trap) == 0)
    {
        /* The read is the whole point, so it lands somewhere the optimizer cannot discard. */
        s_touched = *p;
    }
    s_armed = 0;
    return s_trapped;
}

int mmgr_guard_run_thunk(void (*fn)(void *), void *ctx)
{
    ensure();
    s_trapped = 0;
    s_armed = 1;
    if (MMGR_GUARD_SETJMP(s_trap) == 0)
    {
        fn(ctx);
    }
    s_armed = 0;
    return s_trapped;
}

#else

int mmgr_guard_available(void)
{
    return 0;
}

size_t mmgr_guard_page_size(void)
{
    return 0u;
}

unsigned char *mmgr_guard_run(void)
{
    return NULL;
}

int mmgr_guard_traps_on(const unsigned char *p)
{
    (void)p;
    return 0;
}

int mmgr_guard_run_thunk(void (*fn)(void *), void *ctx)
{
    (void)fn;
    (void)ctx;
    return 0;
}

#endif
