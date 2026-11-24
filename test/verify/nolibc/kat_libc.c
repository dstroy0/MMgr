/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file kat_libc.c
 * @brief The known answer of harness.py nolibc that calls strtol of the C library.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-23
 *
 * @note The ir stage must name strtol where the toolchain gives IR, the object stage must name it, and the link
 *       with no default library must fail on it. A toolchain that misses any of the three gives no verdict that
 *       counts.
 * @note strtol has no builtin form in gcc, clang or cl, and so each of them writes a call to it at every level. A
 *       fill with a length that the code computes is no known answer: gcc at -Os writes it in line.
 */
long strtol(const char *text, char **end, int base);

long kat_libc_parse(const char *text);

long kat_libc_parse(const char *text)
{
    return strtol(text, (char **)0, 10);
}
