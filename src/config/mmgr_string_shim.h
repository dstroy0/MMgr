// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_STRING_SHIM_H
#define MMGR_STRING_SHIM_H

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "config/mmgr_config.h"

/**
 * @file mmgr_string_shim.h
 * @brief Optional. Point the libc string names at this library and keep <string.h> out.
 *
 * Nothing in the library includes this and nothing changes if it is never included. Include it in a
 * translation unit and two things happen there:
 *
 * memcpy, memmove, memcmp, memchr, memset, strlen, strstr, strcmp, strncmp and the rest become the
 * library's own entries, from this line to the end of the translation unit. Existing code keeps its
 * spelling and gets the SWAR implementations.
 *
 * <string.h> stops being includable, because this claims its include guards. A later include of it
 * expands to nothing.
 *
 * Include order does not matter. Claiming a guard only helps when this comes first, but the aliases
 * do not depend on it: they are macros, so every call site below this line is rewritten whether or
 * not libc's declarations are already in scope. Coming first avoids pulling a header nobody needs;
 * coming later still redirects every call after it.
 *
 * These are aliases, not wrappers. The library's entries are bounded and libc's are not, but the
 * bound is not a runtime argument being smuggled in - it is MMGR_STR_MAX, a compile time constant
 * the whole library is already built around. The compiler folds it in like any other constant.
 * Nothing here survives to run time.
 *
 * @note Does not stop the compiler emitting calls to memcpy and memset on its own. A struct
 *       assignment, a large initializer and a loop the optimizer recognizes all lower to libc calls
 *       that no macro is involved in. The flags for that are -ffreestanding and -fno-builtin. This
 *       covers what the source says, not what the back end decides.
 */

/**
 * @brief Longest string this build can hold.
 *
 * Not SIZE_MAX and not a guess. A string lives inside one tenant's confinium and the loculus sizes are
 * fixed at compile time, so the read cap is a constant every call site shares.
 *
 * A build that keeps strings somewhere a custodia did not hand out - a caller's own static buffer,
 * a memory mapped region - sets this itself.
 */
#ifndef MMGR_STR_MAX
#define MMGR_STR_MAX MMGR_CONFIN_MAX
#endif

/*
 * Claimed for the libcs that matter: glibc and musl (_STRING_H), newlib, Cygwin and the BSDs
 * (_STRING_H_), and the odd spellings. Setting one that is already set is a no-op, which is why
 * this needs no guard and no complaint about include order - a libc already in scope keeps its
 * declarations, and the macros below still win at every call site from here down.
 *
 * A libc whose guard is not listed will still include its header. Same result by a longer road.
 */
#ifndef _STRING_H
#define _STRING_H 1
#endif
#ifndef _STRING_H_
#define _STRING_H_ 1
#endif
#ifndef __STRING_H__
#define __STRING_H__ 1
#endif
#ifndef _STRING_H_INCLUDED
#define _STRING_H_INCLUDED 1
#endif
#ifndef _INC_STRING
#define _INC_STRING 1
#endif

MMGR_INCIPE_DECLS

/*
 * Every alias goes through the module's dispatch table rather than naming a free function.
 * cellularum_laboro has no free functions to name - the table is its whole surface - and going
 * through it costs nothing, because a call through a const namespace devirtualizes to the inlined
 * body.
 */

/**
 * @brief memcpy, returning the destination.
 * @param dst Destination.
 * @param src Source.
 * @param n Byte count.
 * @return @p dst.
 *
 * The library's copy returns nothing. A comma expression would supply @p dst, but then every
 * ordinary `memcpy(a, b, n);` statement discards a value and -Wunused-value says so. An inline has
 * the same code and no warning.
 */
MMGR_INLINE void *mmgr_shim_cpy(void *dst, const void *src, size_t n)
{
    memor.cpy(dst, src, n);
    return dst;
}

/**
 * @brief memmove, returning the destination.
 * @param dst Destination.
 * @param src Source.
 * @param n Byte count.
 * @return @p dst.
 */
MMGR_INLINE void *mmgr_shim_move(void *dst, const void *src, size_t n)
{
    memor.move(dst, src, n);
    return dst;
}

/**
 * @brief memset, returning the destination.
 * @param dst Destination.
 * @param c Byte to write.
 * @param n Byte count.
 * @return @p dst.
 */
MMGR_INLINE void *mmgr_shim_set(void *dst, int c, size_t n)
{
    memor.set(dst, (unsigned char)c, n);
    return dst;
}

#define memcpy(dst, src, n) mmgr_shim_cpy((dst), (src), (n))
#define memmove(dst, src, n) mmgr_shim_move((dst), (src), (n))
#define memset(dst, c, n) mmgr_shim_set((dst), (c), (n))
#define memcmp(a, b, n) memor.cmp((a), (b), (n))

/*
 * Argument order differs - the library takes the length before the byte - and it returns const
 * where libc returns void *. Casting the const away is what libc's own signature does; memchr
 * taking a const pointer and handing back a writable one is a known hole in the standard.
 */
#define memchr(p, c, n) ((void *)(size_t)memor.chr((p), (n), (uint8_t)(c)))

#define strlen(s) cellul.len((s), MMGR_STR_MAX)
#define strnlen(s, n) cellul.len((s), (n))

#define strstr(hay, needle) ((char *)(size_t)cellul.find((hay), MMGR_STR_MAX, (needle), MMGR_STR_MAX, MMGR_FALSE))
#define strcasestr(hay, needle) ((char *)(size_t)cellul.find((hay), MMGR_STR_MAX, (needle), MMGR_STR_MAX, MMGR_TRUE))

/*
 * Equality only. The library reports where two strings first differ, not which sorts first, so
 * these give the zero/nonzero half of strcmp's contract and not its sign. A caller ordering strings
 * by the sign of strcmp wants cellul.diff and the bytes at the offset it returns.
 */
#define strcmp(a, b) (!cellul.eq((a), (b), MMGR_STR_MAX, MMGR_FALSE))
#define strcasecmp(a, b) (!cellul.eq((a), (b), MMGR_STR_MAX, MMGR_TRUE))

/*
 * The n-byte forms are diff, not eq. diff returns where two strings first differ and returns the
 * cap itself when they do not differ within it, so "equal in the first n" is exactly diff >= n. eq
 * answers a different question - whether the whole strings match, with the cap only bounding how
 * far it may read - and using it here reported "abcXX" and "abcYY" as different at n = 3.
 */
#define strncmp(a, b, n) (cellul.diff((a), (b), (n), MMGR_FALSE) < (n))
#define strncasecmp(a, b, n) (cellul.diff((a), (b), (n), MMGR_TRUE) < (n))

/*
 * strlcpy's contract, not strcpy's: bounded, returning the length written. strcpy and strcat are
 * not defined at all - they cannot be bounded, so there is no honest alias, and leaving them
 * undefined turns a call into a compile error instead of an overflow.
 */
#define strlcpy(dst, src, cap) cellul.copy((dst), (src), (cap))

/*
 * One pass. memor.chr over cellul.len(s)+1 walks the string twice to ask two questions about the
 * same bytes; cellul.chr asks both from one loaded word.
 */
#define strchr(s, c) ((char *)(size_t)cellul.chr((s), MMGR_STR_MAX, (uint8_t)(c)))

MMGR_FINIS_DECLS

#endif
