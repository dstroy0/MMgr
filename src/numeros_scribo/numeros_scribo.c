/* memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
/**
 * @brief Formatted output assembled from a field spec and a list of values.
 *
 * @note s_kind gives every field kind a verba call, a union arm, a numeric base and a default width.
 */
#include "numeros_scribo/numeros_scribo.h"
#include "cellularum_laboro/cellularum_laboro.h"

/**
 * @brief Arguments for the numer backends.
 *
 * @note out is read by every backend; cap by all but abandon; at by emit_one and finish.
 * @note spec by build; vals and nvals by build and emit; width by emit_one; one by emit_one and numer_str.
 */
typedef struct
{
    char *out;             /**< Destination buffer [BORROWS]. */
    size_t cap;            /**< Bytes available in out. */
    size_t at;             /**< Offset the next field is written at. */
    const mmgr_field *spec; /**< Field list, ending at MMGR_FK_END [BORROWS]. */
    const mmgr_fval *vals; /**< Values to place into the fields [BORROWS]. */
    size_t nvals;          /**< Values in vals. */
    const mmgr_fval *one;  /**< The single value emit_one formats [BORROWS]. */
    uint8_t width;         /**< Width override for that value, or 0 to take the kind's default. */
} NumerCtx;

/**
 * @brief Returns the string in c->one, substituting an empty one for NULL.
 *
 * @param[in] c The value to read [BORROWS].
 * @return      c->one->as.s, or a literal empty string [BORROWS].
 * @note Returns a literal empty string in place of NULL, so the verba call always receives a valid pointer.
 */
MMGR_INLINE const char *numer_str(const NumerCtx *c)
{
    if (c->one->as.s == NULL)
    {
        return "";
    }
    return c->one->as.s;
}

/**
 * @brief Which member of an mmgr_fval union a field kind reads.
 *
 * @note numer_emit_one tests this to pick the union member; NumerKind::fn decides the format.
 */
typedef enum MMGR_ENUM_PACKED
{
    NUMER_ARM_NONE = 0, /**< Reads no value at all. */
    NUMER_ARM_STR,      /**< Reads as.s. */
    NUMER_ARM_U32,      /**< Reads as.u32. */
    NUMER_ARM_U64,      /**< Reads as.u64. */
    NUMER_ARM_I64,      /**< Reads as.i64. */
    NUMER_ARM_D,        /**< Reads as.d. */
    NUMER_ARM_CH,       /**< Reads as.c. */
} NumerArm;

/**
 * @brief One field kind's verba call, union arm, numeric base and default width.
 */
typedef struct
{
    size_t (*fn)(const VerbaCfg *c); /**< The verba call that formats this kind. */
    NumerArm arm;                    /**< Which union member holds the value. */
    uint8_t base;                    /**< Numeric base, or 0 where the call fixes its own. */
    uint8_t width;                   /**< Default width, used when the field and value both leave it 0. */
} NumerKind;

/**
 * @brief Stands in for a kind that formats nothing, leaving the output untouched.
 *
 * @param[in] c The verba arguments, of which only cap is read [BORROWS].
 * @return      c->cap unchanged.
 * @note Bound to MMGR_FK_END and MMGR_FK_LIT in s_kind; numer_build handles both before reaching emit_one.
 * @note numer_emit passes every value to emit_one, so a value of either kind reaches this.
 */
static size_t numer_refuse(const VerbaCfg *c)
{
    return c->cap;
}

/**
 * @brief One entry per field kind, indexed by the mmgr_fk value itself.
 *
 * @note Sized to MMGR_FK_XML plus one, so every enumerator has a row.
 * @note MMGR_FK_DEC and MMGR_FK_U32 share the U32 arm but differ in call and default width.
 * @note MMGR_FK_G defaults to six significant digits; every other kind defaults to 1 or 0.
 */
static const NumerKind s_kind[MMGR_FK_XML + 1u] = {
    [MMGR_FK_END] = {numer_refuse, NUMER_ARM_NONE, 0u, 0u},
    [MMGR_FK_LIT] = {numer_refuse, NUMER_ARM_NONE, 0u, 0u},
    [MMGR_FK_STR] = {mmgr_verba_put, NUMER_ARM_STR, 0u, 0u},
    [MMGR_FK_U32] = {mmgr_verba_u32, NUMER_ARM_U32, 10u, 1u},
    [MMGR_FK_U64] = {mmgr_verba_u64, NUMER_ARM_U64, 10u, 1u},
    [MMGR_FK_I64] = {mmgr_verba_i64, NUMER_ARM_I64, 10u, 1u},
    [MMGR_FK_DEC] = {mmgr_verba_u32w, NUMER_ARM_U32, 10u, 0u},
    [MMGR_FK_HEX] = {mmgr_verba_hex, NUMER_ARM_U64, 16u, 1u},
    [MMGR_FK_OCT] = {mmgr_verba_uint, NUMER_ARM_U64, 8u, 1u},
    [MMGR_FK_G] = {mmgr_verba_g, NUMER_ARM_D, 0u, 6u},
    [MMGR_FK_FIX] = {mmgr_verba_fixed, NUMER_ARM_D, 0u, 0u},
    [MMGR_FK_CH] = {mmgr_verba_ch, NUMER_ARM_CH, 0u, 0u},
    [MMGR_FK_JSON] = {mmgr_verba_json, NUMER_ARM_STR, 0u, 0u},
    [MMGR_FK_XML] = {mmgr_verba_xml, NUMER_ARM_STR, 0u, 0u},
};

/**
 * @brief Formats the single value in c->one at c->at, through that kind's verba call.
 *
 * @param[in] c Buffer, offset, the value and a width override [BORROWS].
 * @return      The offset past what was written, or c->cap when the kind is out of range.
 * @note The width override wins when non-zero; otherwise the kind's own default applies.
 * @note Fills text, ch, val, sval and real on every call, whichever arm the kind names.
 * @warning c->one->kind above MMGR_FK_XML returns c->cap without a lookup, since s_kind ends at MMGR_FK_XML.
 */
MMGR_INLINE size_t numer_emit_one(const NumerCtx *c)
{

    if (c->one->kind > MMGR_FK_XML)
    {
        return c->cap;
    }

    const NumerKind k = s_kind[c->one->kind];
    const uint8_t width = (c->width != 0u) ? c->width : k.width;
    const VerbaCfg cfg = {.out = c->out,
                          .cap = c->cap,
                          .at = c->at,
                          .text = (k.arm == NUMER_ARM_STR) ? numer_str(c) : NULL,
                          .ch = (k.arm == NUMER_ARM_CH) ? c->one->as.c : 0,
                          // Explicit cast widens the u32 arm to the uint64_t VerbaCfg::val is declared with
                          .val = (k.arm == NUMER_ARM_U32) ? (uint64_t)c->one->as.u32 : c->one->as.u64,
                          .sval = c->one->as.i64,
                          .real = c->one->as.d,
                          .base = k.base,
                          .min = width,
                          .sig = width,
                          .decimals = width};

    return k.fn(&cfg);
}

/**
 * @brief Terminates c->out where this write began and reports nothing written.
 *
 * @param[in] c Destination buffer and the starting cursor [BORROWS].
 * @return      0 always.
 * @note Terminates at c->at, which is where this write began, so text already in the buffer survives
 *       an abandoned append. c->at is the starting cursor here, not the offset the walk reached.
 * @note Reads c->out and c->at; cap and the value members take no part.
 * @warning c->out must be writable at c->at.
 */
MMGR_INLINE size_t numer_abandon(const NumerCtx *c)
{
    c->out[c->at] = '\0';
    return 0;
}

/**
 * @brief Closes the output through verba.finish.
 *
 * @param[in] c Buffer, capacity and the offset reached [BORROWS].
 * @return      The length verba.finish reported, which is c->at, or 0 when c->at reached c->cap.
 * @note Writes no terminator of its own. A 0 return is the caller's to act on, and the caller is the
 *       one that knows where the write began.
 */
MMGR_INLINE size_t numer_finish(const NumerCtx *c)
{
    // Reports and terminates nothing on failure. c->at here is the offset the walk reached, not where
    // it began, so this cannot restore the buffer; the caller holds the starting cursor and abandons.
    return MMGR_CALL(verba.finish, VerbaCfg, .out = c->out, .cap = c->cap, .at = c->at);
}

/**
 * @brief Walks c->spec, writing each literal and pairing every other field with the next value in c->vals.
 *
 * @param[in] c Buffer, capacity, the field list and the values [BORROWS].
 * @return      What numer_finish returned, or 0 when c->cap is 0 or the spec and the values do not match.
 * @note An MMGR_FK_LIT field takes cursor->lit and cursor->len; every other field takes cursor->width.
 * @note Calls numer_abandon when a value is missing, when its kind differs, and when values are left over.
 * @warning c->spec must reach an MMGR_FK_END field, which is what ends the walk.
 */
MMGR_INLINE size_t numer_build(const NumerCtx *c)
{
    // Begins at the caller's cursor rather than the first byte, so a run of writes never re-measures
    // what the last one left. An unset at is 0, which is where a single write starts anyway.
    size_t at = c->at;
    size_t k = 0;

    if (c->cap == 0u)
    {
        return 0;
    }

    for (const mmgr_field *cursor = c->spec; cursor->kind != MMGR_FK_END; cursor++)
    {
        if (cursor->kind == MMGR_FK_LIT)
        {
            at = MMGR_CALL(verba.put_n, VerbaCfg, .out = c->out, .cap = c->cap, .at = at, .text = cursor->lit,
                           .text_len = cursor->len);
            continue;
        }

        if ((k >= c->nvals) || (c->vals[k].kind != cursor->kind))
        {
            return numer_abandon(c);
        }

        at = MMGR_CALL(numer_emit_one, NumerCtx, .out = c->out, .cap = c->cap, .at = at, .one = &c->vals[k],
                       .width = cursor->width);
        k++;
    }
    if (k != c->nvals)
    {
        return numer_abandon(c);
    }
    const size_t done = MMGR_CALL(numer_finish, NumerCtx, .out = c->out, .cap = c->cap, .at = at);

    // The finish reports 0 when it ran out of room. c->at is where this write began, so abandoning
    // here puts the terminator back there and leaves earlier text whole.
    return (done == 0u) ? numer_abandon(c) : done;
}

/**
 * @brief Formats every value in c->vals in order, with no field list.
 *
 * @param[in] c Buffer, capacity and the values [BORROWS].
 * @return      What numer_finish returned, or 0 when c->cap is 0.
 * @note Each value carries its own width in c->vals[k].width, where build takes the width from the field.
 * @note Passes every value to numer_emit_one whatever its kind, so MMGR_FK_END and MMGR_FK_LIT reach numer_refuse.
 * @warning c->vals must hold c->nvals values.
 */
MMGR_INLINE size_t numer_emit(const NumerCtx *c)
{
    // Begins at the caller's cursor, as numer_build does
    size_t at = c->at;

    if (c->cap == 0u)
    {
        return 0;
    }

    for (size_t k = 0; k < c->nvals; k++)
    {
        at = MMGR_CALL(numer_emit_one, NumerCtx, .out = c->out, .cap = c->cap, .at = at, .one = &c->vals[k],
                       .width = c->vals[k].width);
    }
    const size_t done = MMGR_CALL(numer_finish, NumerCtx, .out = c->out, .cap = c->cap, .at = at);

    // The finish reports 0 when it ran out of room. c->at is where this write began, so abandoning
    // here puts the terminator back there and leaves earlier text whole.
    return (done == 0u) ? numer_abandon(c) : done;
}

/**
 * @brief Returns the length cellul.len reports for the string already in c->out.
 *
 * @param[in] c Buffer and its capacity [BORROWS].
 * @return      What cellul.len returned, given c->out and c->cap.
 * @note Called by mmgr_numer_append and mmgr_numer_emit_append to find where to carry on writing.
 */
MMGR_INLINE size_t numer_used(const NumerCtx *c)
{
    return MMGR_CALL(cellul.len, CatenaFinitaCfg, .src = c->out, .cap = c->cap);
}

/**
 * @brief Binds this module's four fixed arguments to GENERIC_ENTRY.
 *
 * @param[in] ret  Return type of the entry point.
 * @param[in] name Name after the mmgr_numer_ and numer_ prefixes, which the two share.
 */
#define NUMER_ENTRY(ret, name, ...) GENERIC_ENTRY(mmgr_numer_, numer_, NumerCtx, NumerosCfg, ret, name, __VA_ARGS__)

/**
 * @brief The two entries that forward and nothing else.
 *
 * @note Each is documented at its declaration in numeros_scribo.h.
 * @note The two append entries below are not here. They read the text already in c->out and re-enter
 *       through the numer table, so they carry logic rather than an argument pack alone.
 */
NUMER_ENTRY(size_t, build, .out = c->out, .cap = c->cap, .at = c->at, .spec = c->spec, .vals = c->vals,
            .nvals = c->nvals)
NUMER_ENTRY(size_t, emit, .out = c->out, .cap = c->cap, .at = c->at, .vals = c->vals, .nvals = c->nvals)

/**
 * @brief Writes c->spec and c->vals into c->out after the string already there.
 *
 * @note Calls through the numer table, where mmgr_numer_build reaches numer_build directly.
 * @note Puts the terminator back at c->out[used] when the build reports 0, leaving the earlier text in place.
 * @note Documented at the declaration in numeros_scribo.h.
 */
size_t mmgr_numer_append(const NumerosCfg *c)
{
    if (c->cap == 0u)
    {
        return 0;
    }

    // c->at when the caller threaded it, and only otherwise a scan. A caller that keeps the cursor
    // pays nothing here; one that does not is measured once per call, which is what made a run of
    // appends cost more the longer the text got.
    const size_t used = (c->at != 0u) ? c->at : MMGR_CALL(numer_used, NumerCtx, .out = c->out, .cap = c->cap);

    if (used >= c->cap)
    {
        return 0;
    }

    const size_t n = MMGR_CALL(numer.build, NumerosCfg, .out = c->out, .cap = c->cap, .at = used, .spec = c->spec,
                               .vals = c->vals, .nvals = c->nvals);
    if (n == 0)
    {
        c->out[used] = '\0';
        return 0;
    }
    return n;
}

/**
 * @brief Writes c->vals into c->out after the string already there, reading no field list.
 *
 * @note Calls through the numer table, where mmgr_numer_emit reaches numer_emit directly.
 * @note Puts the terminator back at c->out[used] when the emit reports 0, leaving the earlier text in place.
 * @note Documented at the declaration in numeros_scribo.h.
 */
size_t mmgr_numer_emit_append(const NumerosCfg *c)
{
    if (c->cap == 0u)
    {
        return 0;
    }

    // c->at when the caller threaded it, and only otherwise a scan, as in mmgr_numer_append
    const size_t used = (c->at != 0u) ? c->at : MMGR_CALL(numer_used, NumerCtx, .out = c->out, .cap = c->cap);

    if (used >= c->cap)
    {
        return 0;
    }

    const size_t n = MMGR_CALL(numer.emit, NumerosCfg, .out = c->out, .cap = c->cap, .at = used, .vals = c->vals,
                               .nvals = c->nvals);
    if (n == 0)
    {
        c->out[used] = '\0';
        return 0;
    }
    return n;
}
