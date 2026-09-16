#ifndef MMGR_TEST_ORACLE_DIVERGENCE_H
#define MMGR_TEST_ORACLE_DIVERGENCE_H

#if defined(MMGR_TEST_ORACLE) && MMGR_TEST_ORACLE
#include "mmgr_oracle_libc.h"

#define MMGR_SKIP_ON_ORACLE(why) TEST_IGNORE_MESSAGE("oracle build: " why)
#else
#define MMGR_SKIP_ON_ORACLE(why)                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (0)
#endif

#endif
