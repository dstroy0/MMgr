#include "host_threads.h"

#include <stddef.h>

static void (*s_each)(void);

#if defined(_WIN32)
#include <windows.h>

static DWORD WINAPI host_threads_entry(LPVOID unused)
{
    (void)unused;
    s_each();
    return 0u;
}

int mmgr_host_threads_run(void (*each)(void), unsigned int count)
{
    if (count > MMGR_HOST_THREADS_MOST)
    {
        return 0;
    }
    HANDLE threads[MMGR_HOST_THREADS_MOST];
    int started = 1;

    s_each = each;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        threads[at] = CreateThread(NULL, 0u, host_threads_entry, NULL, 0u, NULL);
        started = started && (threads[at] != NULL);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (threads[at] != NULL)
        {
            (void)WaitForSingleObject(threads[at], INFINITE);
            (void)CloseHandle(threads[at]);
        }
    }
    return started;
}
#else
#include <pthread.h>

static void *host_threads_entry(void *unused)
{
    (void)unused;
    s_each();
    return NULL;
}

int mmgr_host_threads_run(void (*each)(void), unsigned int count)
{
    if (count > MMGR_HOST_THREADS_MOST)
    {
        return 0;
    }
    pthread_t threads[MMGR_HOST_THREADS_MOST];
    int made[MMGR_HOST_THREADS_MOST];
    int started = 1;

    s_each = each;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        made[at] = pthread_create(&threads[at], NULL, host_threads_entry, NULL) == 0;
        started = started && made[at];
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (made[at])
        {
            (void)pthread_join(threads[at], NULL);
        }
    }
    return started;
}
#endif
