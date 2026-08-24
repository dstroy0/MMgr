#ifndef MMGR_NUMEROS_SCRIBO_H
#define MMGR_NUMEROS_SCRIBO_H

#include "verba_scribo/verba_scribo.h"

MMGR_INCIPE_DECLS

typedef enum MMGR_ENUM_PACKED
{
    MMGR_FK_END = 0,
    MMGR_FK_LIT,
    MMGR_FK_STR,
    MMGR_FK_U32,
    MMGR_FK_U64,
    MMGR_FK_I64,
    MMGR_FK_DEC,
    MMGR_FK_HEX,
    MMGR_FK_OCT,
    MMGR_FK_G,
    MMGR_FK_FIX,
    MMGR_FK_CH,
    MMGR_FK_JSON,
    MMGR_FK_XML,
} mmgr_fk;

typedef struct
{
    mmgr_fk kind;
    uint8_t width;
    uint16_t len;
    const char *lit;
} mmgr_field;

#define MMGR_STR {MMGR_FK_STR, 0, 0, NULL}
#define MMGR_U32 {MMGR_FK_U32, 0, 0, NULL}
#define MMGR_U64 {MMGR_FK_U64, 0, 0, NULL}
#define MMGR_I64 {MMGR_FK_I64, 0, 0, NULL}
#define MMGR_CH {MMGR_FK_CH, 0, 0, NULL}
#define MMGR_JSON {MMGR_FK_JSON, 0, 0, NULL}
#define MMGR_XML {MMGR_FK_XML, 0, 0, NULL}
#define MMGR_END {MMGR_FK_END, 0, 0, NULL}

typedef struct
{
    mmgr_fk kind;
    union {
        const char *s;
        uint32_t u32;
        uint64_t u64;
        int64_t i64;
        double d;
        char c;
    } as;
    uint8_t width;
} mmgr_fval;

#define MMGR_VSTR(x) {MMGR_FK_STR, {.s = (x)}, 0}
#define MMGR_VU32(x) {MMGR_FK_U32, {.u32 = (x)}, 0}
#define MMGR_VU64(x) {MMGR_FK_U64, {.u64 = (x)}, 0}
#define MMGR_VI64(x) {MMGR_FK_I64, {.i64 = (x)}, 0}
#define MMGR_VDEC(x) {MMGR_FK_DEC, {.u32 = (x)}, 0}
#define MMGR_VHEX(x) {MMGR_FK_HEX, {.u64 = (x)}, 0}
#define MMGR_VOCT(x) {MMGR_FK_OCT, {.u64 = (x)}, 0}
#define MMGR_VG(x) {MMGR_FK_G, {.d = (x)}, 0}
#define MMGR_VFIX(x) {MMGR_FK_FIX, {.d = (x)}, 0}
#define MMGR_VCH(x) {MMGR_FK_CH, {.c = (x)}, 0}
#define MMGR_VJSON(x) {MMGR_FK_JSON, {.s = (x)}, 0}
#define MMGR_VXML(x) {MMGR_FK_XML, {.s = (x)}, 0}

#define MMGR_VDECW(x, w) {MMGR_FK_DEC, {.u32 = (x)}, (w)}
#define MMGR_VHEXW(x, w) {MMGR_FK_HEX, {.u64 = (x)}, (w)}
#define MMGR_VOCTW(x, w) {MMGR_FK_OCT, {.u64 = (x)}, (w)}
#define MMGR_VGW(x, w) {MMGR_FK_G, {.d = (x)}, (w)}
#define MMGR_VFIXW(x, w) {MMGR_FK_FIX, {.d = (x)}, (w)}

typedef struct
{
    char *const out;
    const size_t cap;
    const mmgr_field *const spec;
    const mmgr_fval *const vals;
    const size_t nvals;
} NumerosCfg;

typedef struct
{
    size_t (*build)(const NumerosCfg *c);
    size_t (*append)(const NumerosCfg *c);
    size_t (*emit)(const NumerosCfg *c);
    size_t (*emit_append)(const NumerosCfg *c);
} NumerosScriboNs;
MMGR_NS_LAYOUT(NumerosScriboNs, build, append, emit, emit_append);

size_t mmgr_numer_build(const NumerosCfg *c);
size_t mmgr_numer_append(const NumerosCfg *c);
size_t mmgr_numer_emit(const NumerosCfg *c);
size_t mmgr_numer_emit_append(const NumerosCfg *c);

MMGR_NS NumerosScriboNs numer MMGR_UNUSED = {
    .build = mmgr_numer_build,
    .append = mmgr_numer_append,
    .emit = mmgr_numer_emit,
    .emit_append = mmgr_numer_emit_append,
};

MMGR_FINIS_DECLS

#endif
