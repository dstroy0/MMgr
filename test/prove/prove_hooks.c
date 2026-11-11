/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file prove_hooks.c
 * @brief The machine-code arm's three words as functions of their own, which the symbolic executor replaces by
 *        name. Their bodies never run under it, and a program built with them runs as a plain program otherwise.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 * @note Each stays out of line, so that the compiler leaves a call the executor can hook in the harness's code.
 */
#include "prove.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#define PROVE_OUT_OF_LINE __declspec(noinline)
#else
#define PROVE_OUT_OF_LINE __attribute__((noinline))
#endif

static volatile size_t s_size_input;
static volatile uint8_t s_byte_input;

PROVE_OUT_OF_LINE size_t nondet_size(void)
{
    return s_size_input;
}

PROVE_OUT_OF_LINE uint8_t nondet_byte(void)
{
    return s_byte_input;
}

PROVE_OUT_OF_LINE void prove_assume(int holds)
{
    if (!holds)
    {
        exit(0);
    }
}

PROVE_OUT_OF_LINE void prove_reached(void)
{
    fprintf(stderr, "prove: the end of the harness is reached\n");
}

PROVE_OUT_OF_LINE void prove_failed(const char *what)
{
    fprintf(stderr, "prove: %s\n", what);
    exit(1);
}
