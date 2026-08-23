#ifndef MMGR_STRING_SHIM_H
#define MMGR_STRING_SHIM_H

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "config/mmgr_config.h"


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
    mmgr_memor_cpy(dst, src, n);
    return dst;
}

MMGR_INLINE void *mmgr_shim_move(void *dst, const void *src, size_t n)
{
    if ((const unsigned char *)dst <= (const unsigned char *)src)
    {
        mmgr_memor_move_down(dst, src, n);
    }
    else
    {
        mmgr_memor_move_up(dst, src, n);
    }
    return dst;
}

MMGR_INLINE void *mmgr_shim_set(void *dst, int c, size_t n)
{
    mmgr_memor_set(dst, (uint8_t)c, n);
    return dst;
}

MMGR_INLINE int mmgr_shim_cmp(const void *a, const void *b, size_t n)
{
    return mmgr_memor_cmp(a, b, n);
}

MMGR_INLINE void *mmgr_shim_chr(const void *p, int c, size_t n)
{
    return (void *)(size_t)mmgr_memor_chr(p, n, (uint8_t)c);
}

#define memcpy(dst, src, n) mmgr_shim_cpy((dst), (src), (n))
#define memmove(dst, src, n) mmgr_shim_move((dst), (src), (n))
#define memset(dst, c, n) mmgr_shim_set((dst), (c), (n))
#define memcmp(a, b, n) mmgr_shim_cmp((a), (b), (n))

#define memchr(p, c, n) mmgr_shim_chr((p), (c), (n))

#define strlen(s) mmgr_cellul_len((s), MMGR_STR_MAX)
#define strnlen(s, n) mmgr_cellul_len((s), (n))

#define strstr(hay, needle) ((char *)(size_t)mmgr_cellul_find((hay), MMGR_STR_MAX, (needle), MMGR_STR_MAX, MMGR_FALSE))
#define strcasestr(hay, needle) ((char *)(size_t)mmgr_cellul_find((hay), MMGR_STR_MAX, (needle), MMGR_STR_MAX, MMGR_TRUE))

#define strcmp(a, b) (!mmgr_cellul_eq((a), (b), MMGR_STR_MAX, MMGR_FALSE))
#define strcasecmp(a, b) (!mmgr_cellul_eq((a), (b), MMGR_STR_MAX, MMGR_TRUE))

#define strncmp(a, b, n) (mmgr_cellul_diff((a), (b), (n), MMGR_FALSE) < (n))
#define strncasecmp(a, b, n) (mmgr_cellul_diff((a), (b), (n), MMGR_TRUE) < (n))

#define strlcpy(dst, src, cap) mmgr_cellul_copy((dst), (src), (cap))

#define strchr(s, c) ((char *)(size_t)mmgr_cellul_chr((s), MMGR_STR_MAX, (uint8_t)(c)))

MMGR_FINIS_DECLS

#endif
