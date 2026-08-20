// ProtoCore v1.0.16 - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mmgr/protoframe/protoframe.h"
#include "mmgr/protostr/protostr.h"
#include "shared/speed_opt/speed_opt.h"

#ifndef MMGR_FRAME_SCAN_LITERALS
#define MMGR_FRAME_SCAN_LITERALS 0
#endif

MMGR_OPTIMIZE_O2

static const char *str_or_empty(const char *s)
{
    if (s == NULL)
    {
        return "";
    }
    return s;
}

size_t mmgr_frame_build(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv)
{
    if (!out || cap == 0 || !spec)
    {
        return 0;
    }
    mmgr_sb b = {out, cap, 0, MMGR_TRUE};
    size_t k = 0;
    for (const mmgr_field *f = spec; f->kind != MMGR_FK_END; f++)
    {
        if (f->kind == MMGR_FK_LIT)
        {
#if MMGR_FRAME_SCAN_LITERALS
            Sb.put(&b, f->lit);
#else

            Sb.put_n(&b, f->lit, f->len);
#endif
            continue;
        }

        if (k >= nv || !v || v[k].kind != f->kind)
        {
            out[0] = '\0';
            return 0;
        }
        const mmgr_fval *a = &v[k];
        k++;
        switch (f->kind)
        {
        case MMGR_FK_STR:
            Sb.put(&b, str_or_empty(a->as.s));
            break;
        case MMGR_FK_U32:
            Sb.u32(&b, a->as.u32);
            break;
        case MMGR_FK_U64:
            Sb.u64(&b, a->as.u64);
            break;
        case MMGR_FK_I64:
            Sb.i64(&b, a->as.i64);
            break;
        case MMGR_FK_DEC:
            Sb.u32w(&b, a->as.u32, f->width);
            break;
        case MMGR_FK_HEX:
            Sb.hex(&b, a->as.u64, f->width ? f->width : 1);
            break;
        case MMGR_FK_OCT:
            Sb.uint(&b, a->as.u64, 8, f->width ? f->width : 1);
            break;
        case MMGR_FK_G:
            Sb.g(&b, a->as.d, f->width ? f->width : 6);
            break;
        case MMGR_FK_FIX:
            Sb.fixed(&b, a->as.d, f->width);
            break;
        case MMGR_FK_CH:
            Sb.ch(&b, a->as.c);
            break;
        case MMGR_FK_JSON:
            Sb.json(&b, str_or_empty(a->as.s));
            break;
        case MMGR_FK_XML:
            Sb.xml(&b, str_or_empty(a->as.s));
            break;
        default:

            out[0] = '\0';
            return 0;
        }
    }
    if (k != nv)
    {
        out[0] = '\0';
        return 0;
    }
    size_t n = Sb.finish(&b);
    if (n == 0)
    {

        out[0] = '\0';
    }
    return n;
}

size_t mmgr_frame_append(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv)
{
    if (!out || cap == 0 || !spec)
    {
        return 0;
    }
    size_t used = str.len(out, cap);
    if (used >= cap)
    {
        return 0;
    }
    size_t n = mmgr_frame_build(out + used, cap - used, spec, v, nv);
    if (n == 0)
    {
        out[used] = '\0';
        return 0;
    }
    return used + n;
}
