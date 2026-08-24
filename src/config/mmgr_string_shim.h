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

MMGR_INLINE void *mmgr_shim_cpy(void *dst, const void *src, size_t n)
{
    MMGR_CALL(memor.cpy, MemoriaCfg, .dst = dst, .src = src, .n = n);
    return dst;
}

MMGR_INLINE void *mmgr_shim_move(void *dst, const void *src, size_t n)
{
    if ((const uint8_t *)dst <= (const uint8_t *)src)
    {
        MMGR_CALL(memor.move_down, MemoriaCfg, .dst = dst, .src = src, .n = n);
    }
    else
    {
        MMGR_CALL(memor.move_up, MemoriaCfg, .dst = dst, .src = src, .n = n);
    }
    return dst;
}

MMGR_INLINE void *mmgr_shim_set(void *dst, mmgr_iword c, size_t n)
{
    MMGR_CALL(memor.set, MemoriaCfg, .dst = dst, .v = (uint8_t)c, .n = n);
    return dst;
}

MMGR_INLINE mmgr_iword mmgr_shim_cmp(const void *a, const void *b, size_t n)
{
    return MMGR_CALL(memor.cmp, MemoriaCfg, .src = a, .other = b, .n = n);
}

MMGR_INLINE void *mmgr_shim_chr(const void *p, mmgr_iword c, size_t n)
{
    return (void *)(size_t)MMGR_CALL(memor.chr, MemoriaCfg, .src = p, .n = n, .v = (uint8_t)c);
}

#define memcpy(dst, src, n) mmgr_shim_cpy((dst), (src), (n))
#define memmove(dst, src, n) mmgr_shim_move((dst), (src), (n))
#define memset(dst, c, n) mmgr_shim_set((dst), (c), (n))
#define memcmp(a, b, n) mmgr_shim_cmp((a), (b), (n))

#define memchr(p, c, n) mmgr_shim_chr((p), (c), (n))

#define strlen(s) mmgr_cellul_len((s), MMGR_STR_MAX)
#define strnlen(s, n) mmgr_cellul_len((s), (n))

#define strstr(hay, needle) ((char *)(size_t)mmgr_cellul_find((hay), MMGR_STR_MAX, (needle), MMGR_STR_MAX, MMGR_FALSE))
#define strcasestr(hay, needle)                                                                                        \
    ((char *)(size_t)mmgr_cellul_find((hay), MMGR_STR_MAX, (needle), MMGR_STR_MAX, MMGR_TRUE))

#define strcmp(a, b) (!mmgr_cellul_eq((a), (b), MMGR_STR_MAX, MMGR_FALSE))
#define strcasecmp(a, b) (!mmgr_cellul_eq((a), (b), MMGR_STR_MAX, MMGR_TRUE))

#define strncmp(a, b, n) (mmgr_cellul_diff((a), (b), (n), MMGR_FALSE) < (n))
#define strncasecmp(a, b, n) (mmgr_cellul_diff((a), (b), (n), MMGR_TRUE) < (n))

#define strlcpy(dst, src, cap) mmgr_cellul_copy((dst), (src), (cap))

#define strchr(s, c) ((char *)(size_t)mmgr_cellul_chr((s), MMGR_STR_MAX, (uint8_t)(c)))

MMGR_FINIS_DECLS

#endif
