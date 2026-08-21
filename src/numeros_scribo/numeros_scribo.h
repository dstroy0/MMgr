// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_NUMEROS_SCRIBO_H
#define MMGR_NUMEROS_SCRIBO_H

#include "verba_scribo/verba_scribo.h"

/**
 * @file numeros_scribo.h
 * @brief Build a formatted line from a field spec and a value list.
 *
 * The spec is data, not a format string. Nothing is parsed at run time and nothing is variadic, so
 * a wrong value type is a compile error rather than a crash.
 */

/** @brief What a field holds. */
typedef enum
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

/**
 * @brief One field of a spec.
 *
 * @c lit is the literal text for MMGR_FK_LIT and NULL otherwise.
 */
typedef struct mmgr_field
{
    uint8_t kind;
    uint8_t width;
    uint16_t len;
    const char *lit;
} mmgr_field;

/** @brief Field spec shorthands. */
#define MMGR_STR {MMGR_FK_STR, 0, 0, NULL}
#define MMGR_U32 {MMGR_FK_U32, 0, 0, NULL}
#define MMGR_U64 {MMGR_FK_U64, 0, 0, NULL}
#define MMGR_I64 {MMGR_FK_I64, 0, 0, NULL}
#define MMGR_CH {MMGR_FK_CH, 0, 0, NULL}
#define MMGR_JSON {MMGR_FK_JSON, 0, 0, NULL}
#define MMGR_XML {MMGR_FK_XML, 0, 0, NULL}
#define MMGR_END {MMGR_FK_END, 0, 0, NULL}

/**
 * @brief One value, tagged with the kind it satisfies.
 *
 * @c width is only read by the kinds that take one - DEC, HEX, OCT, G, FIX - and zero means the
 * default for that kind. It sits after the union so every initializer written before it existed
 * still compiles and leaves it zero.
 */
typedef struct mmgr_fval
{
    uint8_t kind;
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

/** @brief Value shorthands. The tag has to match the kind the value satisfies. */
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

/**
 * @brief Width-carrying value shorthands, for the kinds that take one.
 *
 * @c MMGR_VHEXW(x, 8) is hex zero padded to eight digits. The unwidthed forms above give each kind
 * its default.
 */
#define MMGR_VDECW(x, w) {MMGR_FK_DEC, {.u32 = (x)}, (w)}
#define MMGR_VHEXW(x, w) {MMGR_FK_HEX, {.u64 = (x)}, (w)}
#define MMGR_VOCTW(x, w) {MMGR_FK_OCT, {.u64 = (x)}, (w)}
#define MMGR_VGW(x, w) {MMGR_FK_G, {.d = (x)}, (w)}
#define MMGR_VFIXW(x, w) {MMGR_FK_FIX, {.d = (x)}, (w)}

/**
 * @brief Render values into @p out, replacing what was there.
 * @param out Destination.
 * @param cap Its size including the terminator.
 * @param v Values.
 * @param nv How many.
 * @return Length written, or 0 if it did not fit or a value had no kind.
 *
 * No spec. Each value already says what it is, so the spec array the older entries take is only
 * telling them something they could have read off the value.
 */
size_t mmgr_numer_emit(char *out, size_t cap, const mmgr_fval *v, size_t nv);

/**
 * @brief Render values onto the end of @p out.
 * @param out Destination, already holding a string.
 * @param cap Its size including the terminator.
 * @param v Values.
 * @param nv How many.
 * @return New total length, or 0 if it did not fit.
 */
size_t mmgr_numer_emit_append(char *out, size_t cap, const mmgr_fval *v, size_t nv);

/**
 * @brief Write a formatted line. Reads like snprintf, costs nothing like snprintf.
 *
 *     mmgr_write(buf, sizeof buf, MMGR_VSTR("addr="), MMGR_VHEXW(addr, 8),
 *                                 MMGR_VSTR(" len="), MMGR_VU32(len));
 *
 * There is no format string, so there is nothing to parse at run time and nothing to get out of
 * step with the arguments. A wrong type is a compile error at the value macro rather than a wrong
 * read at the call. The count is sizeof over sizeof, so it is a constant the compiler folds, and
 * the array is a compound literal that lives until the end of the enclosing block.
 *
 * The cost is one array of values on the stack. Not a va_list, not a walk over a format string, and
 * nothing variadic in the C sense - the callee takes a pointer and a count like any other function.
 */
#define mmgr_write(out, cap, ...)                                                                                      \
    mmgr_numer_emit((out), (cap), (const mmgr_fval[]){__VA_ARGS__},                                                    \
                    sizeof((const mmgr_fval[]){__VA_ARGS__}) / sizeof(mmgr_fval))

/**
 * @brief Append a formatted line. Same shape as mmgr_write.
 */
#define mmgr_write_append(out, cap, ...)                                                                               \
    mmgr_numer_emit_append((out), (cap), (const mmgr_fval[]){__VA_ARGS__},                                             \
                           sizeof((const mmgr_fval[]){__VA_ARGS__}) / sizeof(mmgr_fval))

/**
 * @brief Render a spec into @p out, replacing what was there.
 * @param out Destination.
 * @param cap Its size including the terminator.
 * @param spec Field list, ending in MMGR_END.
 * @param v Values.
 * @param nv How many.
 * @return Length written.
 */
size_t mmgr_numer_build(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv);

/**
 * @brief Render a spec onto the end of @p out.
 * @param out Destination, already holding a string.
 * @param cap Its size including the terminator.
 * @param spec Field list, ending in MMGR_END.
 * @param v Values.
 * @param nv How many.
 * @return New total length.
 */
size_t mmgr_numer_append(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv);

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    size_t (*build)(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv);
    size_t (*append)(char *out, size_t cap, const mmgr_field *spec, const mmgr_fval *v, size_t nv);
    size_t (*emit)(char *out, size_t cap, const mmgr_fval *v, size_t nv);
    size_t (*emit_append)(char *out, size_t cap, const mmgr_fval *v, size_t nv);
} NumerosScriboNs;
MMGR_NS_LAYOUT(NumerosScriboNs, build, append, emit, emit_append);

/** @brief Module namespace. */
MMGR_NS NumerosScriboNs numer MMGR_UNUSED = {
    .build = mmgr_numer_build,
    .append = mmgr_numer_append,
    .emit = mmgr_numer_emit,
    .emit_append = mmgr_numer_emit_append,
};

#endif
