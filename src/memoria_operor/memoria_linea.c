/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file memoria_linea.c
 * @brief The calls of memor with plain arguments. Each is a compound literal of the members that its call reads.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note Each entry is one call of memor, and it does no other work. The members that the entry does not name are zero,
 * and the call that the entry makes does not read them.
 */
#include "memoria_operor/memoria_linea.h"
#include "memoria_operor/memoria_operor.h"

void mmgr_bytes_copy(void *dst, const void *src, size_t bytes)
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = dst, .src = src, .bytes = bytes);
}

void mmgr_bytes_move(void *dst, const void *src, size_t bytes)
{
    // Explicit casts to a byte address, because a relational test wants both sides to point at a complete object
    // type and void is not one. The comparison picks which way the copy walks
    if ((const uint8_t *)dst <= (const uint8_t *)src)
    {
        EMBED_CALL(memor.move_down, MemoriaCfg, .dst = dst, .src = src, .bytes = bytes);
    }
    else
    {
        EMBED_CALL(memor.move_up, MemoriaCfg, .dst = dst, .src = src, .bytes = bytes);
    }
}

void mmgr_bytes_set(void *dst, uint8_t value, size_t bytes)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = dst, .bytes = bytes, .val = value);
}

embed_iword mmgr_bytes_compare(const void *left, const void *right, size_t bytes)
{
    return EMBED_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right, .bytes = bytes);
}

const void *mmgr_bytes_find(const void *region, uint8_t value, size_t bytes)
{
    return EMBED_CALL(memor.chr, MemoriaCfg, .src = region, .bytes = bytes, .val = value);
}
