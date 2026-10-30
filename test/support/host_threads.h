#ifndef MMGR_TEST_HOST_THREADS_H
#define MMGR_TEST_HOST_THREADS_H

// The platform's thread headers live in host_threads.c alone. Unity's generator copies every
// #include of a suite into its runner and reads no #if, and therefore a suite naming windows.h on one arm and
// pthread.h on the other would hand the runner both.

#define MMGR_HOST_THREADS_MOST 16u

// Runs count threads, each calling each once, and waits for every one to end. 1 where every thread
// started and ended, 0 where the system refused one or count passed MMGR_HOST_THREADS_MOST.
int mmgr_host_threads_run(void (*each)(void), unsigned int count);

#endif
