// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "guard_page.h"

#if MMGR_GUARD_SUPPORTED

#include <setjmp.h>

static jmp_buf s_trap;
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
        longjmp(s_trap, 1);
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
        longjmp(s_trap, 1);
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
    if (setjmp(s_trap) == 0)
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
    if (setjmp(s_trap) == 0)
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
