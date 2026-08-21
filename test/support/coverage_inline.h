// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_TEST_COVERAGE_INLINE_H
#define MMGR_TEST_COVERAGE_INLINE_H

/**
 * @file coverage_inline.h
 * @brief Forced onto the front of every translation unit in the coverage build.
 *
 * MMGR_INLINE carries always_inline, which gcc honors even at -O0. Every call site of a header
 * entry therefore gets its own copy of that entry's branch records, and a line holding one
 * condition comes back holding one pair of branches per copy. mmgr_ascii_in is one condition and
 * one guard on one line; the coverage build reported twenty eight branches on it, because the
 * suite calls it from fourteen places. A copy reached from a caller that only ever passes low
 * bytes leaves the guard's other side untaken forever, so the number can never close no matter
 * what the tests do.
 *
 * mmgr_compiler_directives.h guards MMGR_INLINE with #ifndef precisely so a build can say what it
 * wants. This build wants one copy per entry, so a branch belongs to the source and not to the
 * call site that happened to inline it. Nothing in src/ changes, and no other build sees this.
 */
#define MMGR_INLINE static inline

#endif
