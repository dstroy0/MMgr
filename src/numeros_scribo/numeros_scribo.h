/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Formatted output: the field kinds, the value union, the macros that build both, and the numer table.
 *
 * @note A spec is an mmgr_field array ending at MMGR_FK_END; every field but MMGR_FK_LIT takes one mmgr_fval.
 * @note Includes verba_scribo.h, which is where mmgr_config.h and the verba table come from.
 */
#ifndef MMGR_NUMEROS_SCRIBO_H
#define MMGR_NUMEROS_SCRIBO_H

#include "verba_scribo/verba_scribo.h"

MMGR_INCIPE_DECLS

/**
 * @brief What one field formats, and which mmgr_fval arm it reads.
 *
 * @note numeros_scribo.c indexes s_kind by this value, so MMGR_FK_XML is the largest one it accepts.
 * @note MMGR_ENUM_PACKED asks for the narrowest representation; mmgr_types.h asserts that it took effect.
 */
typedef enum MMGR_ENUM_PACKED
{
    MMGR_FK_END = 0, /**< Ends a spec; numer_build stops at this field. */
    MMGR_FK_LIT,     /**< Literal text, taken from mmgr_field::lit and mmgr_field::len. */
    MMGR_FK_STR,     /**< String from as.s, through mmgr_verba_put. */
    MMGR_FK_U32,     /**< Unsigned 32-bit from as.u32, base 10, through mmgr_verba_u32. */
    MMGR_FK_U64,     /**< Unsigned 64-bit from as.u64, base 10, through mmgr_verba_u64. */
    MMGR_FK_I64,     /**< Signed 64-bit from as.i64, base 10, through mmgr_verba_i64. */
    MMGR_FK_DEC,     /**< Unsigned 32-bit from as.u32, base 10, through mmgr_verba_u32w. */
    MMGR_FK_HEX,     /**< Unsigned 64-bit from as.u64, base 16, through mmgr_verba_hex. */
    MMGR_FK_OCT,     /**< Unsigned 64-bit from as.u64, base 8, through mmgr_verba_uint. */
    MMGR_FK_G,       /**< Double from as.d, through mmgr_verba_g, defaulting to six significant digits. */
    MMGR_FK_FIX,     /**< Double from as.d, through mmgr_verba_fixed. */
    MMGR_FK_CH,      /**< Single character from as.c, through mmgr_verba_ch. */
    MMGR_FK_JSON,    /**< String from as.s, through mmgr_verba_json. */
    MMGR_FK_XML,     /**< String from as.s, through mmgr_verba_xml. */
} mmgr_fk;

/**
 * @brief One field of a spec.
 *
 * @note numer_build reads lit and len only for MMGR_FK_LIT, and width only for the other kinds.
 */
typedef struct
{
    mmgr_fk kind;    /**< What this field formats. */
    uint8_t width;   /**< Width to give the value, or 0 to take the kind's default from s_kind. */
    uint16_t len;    /**< Bytes of lit to write, for MMGR_FK_LIT. */
    const char *lit; /**< Literal text, for MMGR_FK_LIT [BORROWS]. */
} mmgr_field;

/** @brief Expands to an mmgr_field of kind MMGR_FK_STR, with width 0, len 0 and lit NULL. */
#define MMGR_STR {MMGR_FK_STR, 0, 0, NULL}

/** @brief Expands to an mmgr_field of kind MMGR_FK_U32, with width 0, len 0 and lit NULL. */
#define MMGR_U32 {MMGR_FK_U32, 0, 0, NULL}

/** @brief Expands to an mmgr_field of kind MMGR_FK_U64, with width 0, len 0 and lit NULL. */
#define MMGR_U64 {MMGR_FK_U64, 0, 0, NULL}

/** @brief Expands to an mmgr_field of kind MMGR_FK_I64, with width 0, len 0 and lit NULL. */
#define MMGR_I64 {MMGR_FK_I64, 0, 0, NULL}

/** @brief Expands to an mmgr_field of kind MMGR_FK_CH, with width 0, len 0 and lit NULL. */
#define MMGR_CH {MMGR_FK_CH, 0, 0, NULL}

/** @brief Expands to an mmgr_field of kind MMGR_FK_JSON, with width 0, len 0 and lit NULL. */
#define MMGR_JSON {MMGR_FK_JSON, 0, 0, NULL}

/** @brief Expands to an mmgr_field of kind MMGR_FK_XML, with width 0, len 0 and lit NULL. */
#define MMGR_XML {MMGR_FK_XML, 0, 0, NULL}

/**
 * @brief Expands to an mmgr_field of kind MMGR_FK_END, with width 0, len 0 and lit NULL.
 *
 * @note numer_build stops at this field, so every spec array needs one as its last entry.
 */
#define MMGR_END {MMGR_FK_END, 0, 0, NULL}

/**
 * @brief One value, tagged with the kind that says which arm of as holds it.
 *
 * @note numer_build requires kind to equal the field's kind, and abandons the whole write when it does not.
 * @note numer_emit takes width from here, where numer_build takes it from the field.
 * @warning numer_emit_one reads as.i64 and as.d for every kind, not only for the kinds that name them.
 */
typedef struct
{
    mmgr_fk kind; /**< Which arm of as holds the value. */
    union {
        const char *s; /**< Read for MMGR_FK_STR, MMGR_FK_JSON and MMGR_FK_XML [BORROWS]. */
        uint32_t u32;  /**< Read for MMGR_FK_U32 and MMGR_FK_DEC. */
        uint64_t u64;  /**< Read for MMGR_FK_U64, MMGR_FK_HEX and MMGR_FK_OCT. */
        int64_t i64;   /**< Read for MMGR_FK_I64. */
        double d;      /**< Read for MMGR_FK_G and MMGR_FK_FIX. */
        char c;        /**< Read for MMGR_FK_CH. */
    } as;              /**< The value, under the arm kind names. */
    uint8_t width;     /**< Width numer_emit gives this value, or 0 to take the kind's default from s_kind. */
} mmgr_fval;

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_STR, with x in as.s and width 0.
 *
 * @param[in] x String placed in as.s [BORROWS].
 */
#define MMGR_VSTR(x) {MMGR_FK_STR, {.s = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_U32, with x in as.u32 and width 0.
 *
 * @param[in] x Value placed in as.u32.
 */
#define MMGR_VU32(x) {MMGR_FK_U32, {.u32 = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_U64, with x in as.u64 and width 0.
 *
 * @param[in] x Value placed in as.u64.
 */
#define MMGR_VU64(x) {MMGR_FK_U64, {.u64 = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_I64, with x in as.i64 and width 0.
 *
 * @param[in] x Value placed in as.i64.
 */
#define MMGR_VI64(x) {MMGR_FK_I64, {.i64 = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_DEC, with x in as.u32 and width 0.
 *
 * @param[in] x Value placed in as.u32.
 */
#define MMGR_VDEC(x) {MMGR_FK_DEC, {.u32 = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_HEX, with x in as.u64 and width 0.
 *
 * @param[in] x Value placed in as.u64.
 */
#define MMGR_VHEX(x) {MMGR_FK_HEX, {.u64 = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_OCT, with x in as.u64 and width 0.
 *
 * @param[in] x Value placed in as.u64.
 */
#define MMGR_VOCT(x) {MMGR_FK_OCT, {.u64 = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_G, with x in as.d and width 0.
 *
 * @param[in] x Value placed in as.d.
 */
#define MMGR_VG(x) {MMGR_FK_G, {.d = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_FIX, with x in as.d and width 0.
 *
 * @param[in] x Value placed in as.d.
 */
#define MMGR_VFIX(x) {MMGR_FK_FIX, {.d = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_CH, with x in as.c and width 0.
 *
 * @param[in] x Character placed in as.c.
 */
#define MMGR_VCH(x) {MMGR_FK_CH, {.c = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_JSON, with x in as.s and width 0.
 *
 * @param[in] x String placed in as.s [BORROWS].
 */
#define MMGR_VJSON(x) {MMGR_FK_JSON, {.s = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_XML, with x in as.s and width 0.
 *
 * @param[in] x String placed in as.s [BORROWS].
 */
#define MMGR_VXML(x) {MMGR_FK_XML, {.s = (x)}, 0}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_DEC, with x in as.u32 and w in width.
 *
 * @param[in] x Value placed in as.u32.
 * @param[in] w Width placed in the width member.
 * @note numer_emit uses that width; numer_build takes the width from the field instead.
 */
#define MMGR_VDECW(x, w) {MMGR_FK_DEC, {.u32 = (x)}, (w)}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_HEX, with x in as.u64 and w in width.
 *
 * @param[in] x Value placed in as.u64.
 * @param[in] w Width placed in the width member.
 * @note numer_emit uses that width; numer_build takes the width from the field instead.
 */
#define MMGR_VHEXW(x, w) {MMGR_FK_HEX, {.u64 = (x)}, (w)}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_OCT, with x in as.u64 and w in width.
 *
 * @param[in] x Value placed in as.u64.
 * @param[in] w Width placed in the width member.
 * @note numer_emit uses that width; numer_build takes the width from the field instead.
 */
#define MMGR_VOCTW(x, w) {MMGR_FK_OCT, {.u64 = (x)}, (w)}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_G, with x in as.d and w in width.
 *
 * @param[in] x Value placed in as.d.
 * @param[in] w Width placed in the width member.
 * @note numer_emit_one puts that width into VerbaCfg::min, VerbaCfg::sig and VerbaCfg::decimals together.
 * @note numer_emit uses it; numer_build takes the width from the field instead.
 */
#define MMGR_VGW(x, w) {MMGR_FK_G, {.d = (x)}, (w)}

/**
 * @brief Expands to an mmgr_fval of kind MMGR_FK_FIX, with x in as.d and w in width.
 *
 * @param[in] x Value placed in as.d.
 * @param[in] w Width placed in the width member.
 * @note numer_emit_one puts that width into VerbaCfg::min, VerbaCfg::sig and VerbaCfg::decimals together.
 * @note numer_emit uses it; numer_build takes the width from the field instead.
 */
#define MMGR_VFIXW(x, w) {MMGR_FK_FIX, {.d = (x)}, (w)}

/**
 * @brief Arguments for the four numer calls.
 *
 * @note build and append read all six members; emit and emit_append leave spec alone.
 * @note at is the cursor, as in verba: build and emit begin there and return where they finished, so
 *       a run of writes threads the cursor rather than measuring the text again between each. Leave
 *       it unset and a call starts at the first byte, which is what a single write wants.
 */
typedef struct
{
    char *const out;              /**< Destination buffer [BORROWS]. */
    const size_t cap;             /**< Bytes available in out. */
    const size_t at;              /**< Offset to begin writing at; 0 for the first write. */
    const mmgr_field *const spec; /**< Field list, ending at MMGR_FK_END [BORROWS]. */
    const mmgr_fval *const vals;  /**< Values to place into the fields [BORROWS]. */
    const size_t nvals;           /**< Values in vals. */
} NumerosCfg;

/**
 * @brief Type of the numer dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the four members sit at consecutive MMGR_FP_SIZE offsets, with nothing else.
 * @note build and emit start at out's first byte; the two append members start past the text already there.
 */
typedef struct
{
    size_t (*build)(const NumerosCfg *args);       /**< Writes a spec and its values. */
    size_t (*append)(const NumerosCfg *args);      /**< Writes a spec and its values after the text in out. */
    size_t (*emit)(const NumerosCfg *args);        /**< Writes values, with no field list. */
    size_t (*emit_append)(const NumerosCfg *args); /**< Writes values after the text in out, with no field list. */
} NumerosScriboNs;
MMGR_NS_LAYOUT(NumerosScriboNs, build, append, emit, emit_append);

/**
 * @brief Writes args->spec and args->vals into args->out, starting at its first byte.
 *
 * @param[in] args Buffer, capacity, the field list and the values [BORROWS].
 * @return      Length of the string written, not counting its terminator, or 0 when nothing was written.
 * @note An MMGR_FK_LIT field writes its own text; every other field consumes the next value in args->vals.
 * @note Returns 0 and empties args->out when a value is missing, when a kind differs, or when values are left over.
 * @warning args->spec must reach an MMGR_FK_END field, and args->vals must hold args->nvals values.
 */
size_t mmgr_numer_build(const NumerosCfg *args);

/**
 * @brief Writes args->spec and args->vals into args->out after the string already there.
 *
 * @param[in] args Buffer, capacity, the field list and the values [BORROWS].
 * @return      Length of the whole string in args->out, or 0 when nothing was added.
 * @note Measures the existing string with cellul.len, then builds from there with the capacity that is left.
 * @note Puts the terminator back at the existing length and returns 0 when the build writes nothing.
 * @warning args->out must already hold a terminated string, and args->spec must reach an MMGR_FK_END field.
 */
size_t mmgr_numer_append(const NumerosCfg *args);

/**
 * @brief Writes args->vals into args->out, starting at its first byte and reading no field list.
 *
 * @param[in] args Buffer, capacity and the values [BORROWS].
 * @return      Length of the string written, not counting its terminator, or 0 when nothing was written.
 * @note Each value carries its own width, and no kind is matched, since there are no fields to match against.
 * @warning args->vals must hold args->nvals values.
 */
size_t mmgr_numer_emit(const NumerosCfg *args);

/**
 * @brief Writes args->vals into args->out after the string already there, reading no field list.
 *
 * @param[in] args Buffer, capacity and the values [BORROWS].
 * @return      Length of the whole string in args->out, or 0 when nothing was added.
 * @note Measures the existing string with cellul.len, then emits from there with the capacity that is left.
 * @note Puts the terminator back at the existing length and returns 0 when the emit writes nothing.
 * @warning args->out must already hold a terminated string, and args->vals must hold args->nvals values.
 */
size_t mmgr_numer_emit_append(const NumerosCfg *args);

/**
 * @brief Dispatch table instance named numer; each member calls the matching mmgr_numer_ function.
 *
 * @note mmgr_numer_append reaches build through this table, and mmgr_numer_emit_append reaches emit.
 */
MMGR_NS NumerosScriboNs numer MMGR_UNUSED = {
    .build = mmgr_numer_build,
    .append = mmgr_numer_append,
    .emit = mmgr_numer_emit,
    .emit_append = mmgr_numer_emit_append,
};

MMGR_FINIS_DECLS

#endif
