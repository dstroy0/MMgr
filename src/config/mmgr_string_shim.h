#ifndef MMGR_STRING_SHIM_H
#define MMGR_STRING_SHIM_H

#include "cellularum_laboro/cellularum_laboro.h"
#include "config/mmgr_config.h"
#include "memoria_operor/memoria_operor.h"

#ifndef MMGR_STR_MAX
#define MMGR_STR_MAX MMGR_CARCER_MAX
#endif

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

MMGR_INLINE void *mmgr_shim_cpy(void *dest, const void *source, size_t bytes)
{
    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = dest, .src = source, .bytes = bytes);
    return dest;
}

MMGR_INLINE void *mmgr_shim_move(void *dest, const void *source, size_t bytes)
{
    if ((const uint8_t *)dest <= (const uint8_t *)source)
    {
        MMGR_CALL(memor.move_down, MemoriaCfg, .dst = dest, .src = source, .bytes = bytes);
    }
    else
    {
        MMGR_CALL(memor.move_up, MemoriaCfg, .dst = dest, .src = source, .bytes = bytes);
    }
    return dest;
}

MMGR_INLINE void *mmgr_shim_set(void *dest, mmgr_iword value, size_t bytes)
{
    MMGR_CALL(memor.set, MemoriaCfg, .dst = dest, .val = (uint8_t)value, .bytes = bytes);
    return dest;
}

MMGR_INLINE mmgr_iword mmgr_shim_cmp(const void *left, const void *right, size_t bytes)
{
    return MMGR_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right, .bytes = bytes);
}

MMGR_INLINE void *mmgr_shim_chr(const void *region, mmgr_iword value, size_t bytes)
{
    return (void *)(size_t)MMGR_CALL(memor.chr, MemoriaCfg, .src = region, .bytes = bytes, .val = (uint8_t)value);
}

#define memcpy(dest, source, bytes) mmgr_shim_cpy((dest), (source), (bytes))
#define memmove(dest, source, bytes) mmgr_shim_move((dest), (source), (bytes))
#define memset(dest, value, bytes) mmgr_shim_set((dest), (value), (bytes))
#define memcmp(left, right, bytes) mmgr_shim_cmp((left), (right), (bytes))

#define memchr(region, value, bytes) mmgr_shim_chr((region), (value), (bytes))

#define strlen(text) MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = (text), .cap = MMGR_STR_MAX)
#define strnlen(text, limit) MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = (text), .cap = (limit))

#define strstr(haystack, needle)                                                                                       \
    ((char *)(size_t)MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = (haystack), .cap = MMGR_STR_MAX, .other = (needle),      \
                               .other_cap = MMGR_STR_MAX, .ci = MMGR_FALSE))
#define strcasestr(haystack, needle)                                                                                   \
    ((char *)(size_t)MMGR_CALL(cellul.find, CatenaFinitaCfg, .src = (haystack), .cap = MMGR_STR_MAX, .other = (needle),      \
                               .other_cap = MMGR_STR_MAX, .ci = MMGR_TRUE))

#define strcmp(left, right)                                                                                            \
    (!MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = (left), .other = (right), .cap = MMGR_STR_MAX, .ci = MMGR_FALSE))
#define strcasecmp(left, right)                                                                                        \
    (!MMGR_CALL(cellul.eq, CatenaFinitaCfg, .src = (left), .other = (right), .cap = MMGR_STR_MAX, .ci = MMGR_TRUE))

#define strncmp(left, right, limit)                                                                                    \
    (MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = (left), .other = (right), .cap = (limit), .ci = MMGR_FALSE) < (limit))
#define strncasecmp(left, right, limit)                                                                                \
    (MMGR_CALL(cellul.diff, CatenaFinitaCfg, .src = (left), .other = (right), .cap = (limit), .ci = MMGR_TRUE) < (limit))

#define strlcpy(dest, source, limit)                                                                                   \
    MMGR_CALL(cellul.copy, CatenaFinitaCfg, .dst = (dest), .src = (source), .cap = (limit))

#define strchr(text, value)                                                                                            \
    ((char *)(size_t)MMGR_CALL(cellul.chr, CatenaFinitaCfg, .src = (text), .cap = MMGR_STR_MAX, .byte = (uint8_t)(value)))

MMGR_FINIS_DECLS

#endif
