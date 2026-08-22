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
 *
 * The table is the whole surface. There are no free functions to call.
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
 * @brief What a render is given.
 *
 * Public, and in the header, because the caller is what builds it. Every member is const: nothing
 * writes to a config once the caller has built it, and the compound literal is gone before there is
 * code that could.
 *
 * One shape for all four entries. A spec driven render and a spec free one differ by whether spec is
 * set, not by what they are handed, so they are four entries over one config rather than two shapes.
 *
 * Not the module's context. NumerCtx is what the bodies work with - the builder, the cursor, and
 * which value is next - and it is file local in the .c.
 */
typedef struct
{
    char *const out;              /**< Destination. For an append, already holding a string. */
    const size_t cap;             /**< Its size, including the terminator. */
    const mmgr_field *const spec; /**< Field list, ending in MMGR_END. Read by build and append. */
    const mmgr_fval *const v;     /**< The values. */
    const size_t nv;              /**< How many. */
} NumerosCfg;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    size_t (*build)(const NumerosCfg *c);
    size_t (*append)(const NumerosCfg *c);
    size_t (*emit)(const NumerosCfg *c);
    size_t (*emit_append)(const NumerosCfg *c);
} NumerosScriboNs;
MMGR_NS_LAYOUT(NumerosScriboNs, build, append, emit, emit_append);

/** @name The entries the table points at.
 *  @brief Nameable so a static const table can name them, and for no other reason. The table is
 *         still the whole surface: call through it.
 *  @{ */
size_t mmgr_numer_build(const NumerosCfg *c);
size_t mmgr_numer_append(const NumerosCfg *c);
size_t mmgr_numer_emit(const NumerosCfg *c);
size_t mmgr_numer_emit_append(const NumerosCfg *c);
/** @} */

/** @name The types a render takes, settled where the call is written.
 *  @{ */
#define MMGR_NUMER_IS_WSTR(x_) ((void)_Generic((x_), char *: 0))
#define MMGR_NUMER_IS_SIZE(x_) ((void)_Generic((x_), size_t: 0, int: 0, unsigned: 0, long: 0, unsigned long: 0))
#define MMGR_NUMER_IS_SPEC(x_) ((void)_Generic((x_), const mmgr_field *: 0, mmgr_field *: 0))
#define MMGR_NUMER_IS_VALS(x_) ((void)_Generic((x_), const mmgr_fval *: 0, mmgr_fval *: 0))
/** @} */

/**
 * @brief Render @p v_ against @p spec_ into @p out_, replacing what was there.
 *
 * Positional in, so the struct and the designators never reach a call site, and the type of every
 * argument is settled where the call is written.
 */
#define mmgr_numer_build(out_, cap_, spec_, v_, nv_)                                                                   \
    (MMGR_NUMER_IS_WSTR(out_), MMGR_NUMER_IS_SIZE(cap_), MMGR_NUMER_IS_SPEC(spec_), MMGR_NUMER_IS_VALS(v_),            \
     MMGR_NUMER_IS_SIZE(nv_),                                                                                          \
     numer.build(&(NumerosCfg){.out = (out_), .cap = (cap_), .spec = (spec_), .v = (v_), .nv = (nv_)}))

/** @brief Render @p v_ against @p spec_ onto the end of @p out_. */
#define mmgr_numer_append(out_, cap_, spec_, v_, nv_)                                                                  \
    (MMGR_NUMER_IS_WSTR(out_), MMGR_NUMER_IS_SIZE(cap_), MMGR_NUMER_IS_SPEC(spec_), MMGR_NUMER_IS_VALS(v_),            \
     MMGR_NUMER_IS_SIZE(nv_),                                                                                          \
     numer.append(&(NumerosCfg){.out = (out_), .cap = (cap_), .spec = (spec_), .v = (v_), .nv = (nv_)}))

/**
 * @brief Render @p v_ into @p out_ with no spec, replacing what was there.
 *
 * No spec. Each value already says what it is, so the spec array the other two entries take is only
 * telling them something they could have read off the value.
 */
#define mmgr_numer_emit(out_, cap_, v_, nv_)                                                                           \
    (MMGR_NUMER_IS_WSTR(out_), MMGR_NUMER_IS_SIZE(cap_), MMGR_NUMER_IS_VALS(v_), MMGR_NUMER_IS_SIZE(nv_),              \
     numer.emit(&(NumerosCfg){.out = (out_), .cap = (cap_), .v = (v_), .nv = (nv_)}))

/** @brief Render @p v_ with no spec onto the end of @p out_. */
#define mmgr_numer_emit_append(out_, cap_, v_, nv_)                                                                    \
    (MMGR_NUMER_IS_WSTR(out_), MMGR_NUMER_IS_SIZE(cap_), MMGR_NUMER_IS_VALS(v_), MMGR_NUMER_IS_SIZE(nv_),              \
     numer.emit_append(&(NumerosCfg){.out = (out_), .cap = (cap_), .v = (v_), .nv = (nv_)}))

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
 * The array is parenthesised. The value shorthands are braced initializers, and a brace does not
 * hide a comma from the preprocessor: without the parentheses the entry below would be handed one
 * argument per value.
 *
 * The cost is one array of values on the stack. Not a va_list, not a walk over a format string, and
 * nothing variadic in the C sense - the callee takes a pointer and a count like any other function.
 */
#define mmgr_write(out_, cap_, ...)                                                                                    \
    mmgr_numer_emit((out_), (cap_), ((const mmgr_fval[]){__VA_ARGS__}),                                                \
                    sizeof((const mmgr_fval[]){__VA_ARGS__}) / sizeof(mmgr_fval))

/**
 * @brief Append a formatted line. Same shape as mmgr_write.
 */
#define mmgr_write_append(out_, cap_, ...)                                                                             \
    mmgr_numer_emit_append((out_), (cap_), ((const mmgr_fval[]){__VA_ARGS__}),                                         \
                           sizeof((const mmgr_fval[]){__VA_ARGS__}) / sizeof(mmgr_fval))

/**
 * @brief Module namespace.
 *
 * static const, like every other module's. gcc devirtualizes a call through one down to the
 * inlined body and cannot do that through an extern one, where the table is in another
 * translation unit and every call is a load and an indirect jump.
 */
MMGR_NS NumerosScriboNs numer MMGR_UNUSED = {
    .build = mmgr_numer_build,
    .append = mmgr_numer_append,
    .emit = mmgr_numer_emit,
    .emit_append = mmgr_numer_emit_append,
};

#endif
