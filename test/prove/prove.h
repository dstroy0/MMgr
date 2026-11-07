/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file prove.h
 * @brief The three words a proof harness is written in: an input of any value, an assumption that bounds it, and
 *        an assertion that must hold on every input the assumptions leave.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note One harness serves two provers. Under CBMC, which `harness.py prove` runs with PROVE_CBMC defined, the
 *       words are CBMC's own: an undefined function
 *       whose name begins nondet_ answers any value, and __CPROVER_assume and __CPROVER_assert bound and check
 *       the C. Compiled by a toolchain, the words are calls to functions no unit defines; `harness.py prove`'s
 *       machine-code arm hooks each in the symbolic executor, an input a fresh symbol, an assumption a constraint
 *       and an assertion a path to prove unreachable, and the same property is proved over the object code each
 *       compiler lowered.
 */
#ifndef MMGR_PROVE_H
#define MMGR_PROVE_H

#include <stddef.h>
#include <stdint.h>

/** @brief A size of any value. */
size_t nondet_size(void);

/** @brief A byte of any value. */
uint8_t nondet_byte(void);

#if defined(PROVE_CBMC)
#define PROVE_ASSUME(cond_) __CPROVER_assume(cond_)
#define PROVE_ASSERT(cond_, msg_) __CPROVER_assert((cond_), (msg_))
// The reach run defines PROVE_REACH and must fail here: a harness whose assumptions leave no path proves nothing
#if defined(PROVE_REACH)
#define PROVE_DONE() __CPROVER_assert(0, "prove: the end of the harness is reached")
#else
#define PROVE_DONE() ((void)0)
#endif
#else
/** @brief The machine-code arm's mark that a path reached the end of the harness. */
void prove_reached(void);

#define PROVE_DONE() prove_reached()
/** @brief The machine-code arm's assumption: the executor keeps only the paths where cond holds. */
void prove_assume(int holds);

/** @brief The machine-code arm's failure: a path that reaches it with cond false is a counterexample. */
void prove_failed(const char *what);

#define PROVE_ASSUME(cond_) prove_assume((cond_) != 0)
#define PROVE_ASSERT(cond_, msg_)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond_))                                                                                                  \
        {                                                                                                              \
            prove_failed(msg_);                                                                                        \
        }                                                                                                              \
    } while (0)
#endif

/** @brief A cell's header word as LocusCarcerum lays it: a word rounded up to the alignment, ahead of the payload. */
#define PROVE_CELL_HEADER ((sizeof(embed_atomic_usize) + (MMGR_CARCER_ALIGN - 1u)) & ~(MMGR_CARCER_ALIGN - 1u))

/**
 * @brief LocusCarcerum's report of a give or growth that names another owner, defined by the harness that
 *        includes this after locus_carcerum.h: every give a harness makes names the cell's own owner, and the report
 *        is a failure.
 */
#define PROVE_NO_MISNAMED()                                                                                            \
    void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)                   \
    {                                                                                                                  \
        (void)named;                                                                                                   \
        (void)cell_owner;                                                                                              \
        (void)bytes;                                                                                                   \
        PROVE_ASSERT(0, "a give names the cell's own owner");                                                          \
    }

#endif
