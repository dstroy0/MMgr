#ifndef MMGR_VERBUM_SCRUTOR_H
#define MMGR_VERBUM_SCRUTOR_H

#include "proximus_operor/proximus_operor.h"

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

#define MMGR_SWAR_BYTES (sizeof(mmgr_word))
#define MMGR_SWAR_LANE_BITS (MMGR_WORD_BITS)

MMGR_STATIC_ASSERT(sizeof(mmgr_word) == sizeof(mmgr_migro_word),
                   "the lane carrier and the word proximus moves are the same width, so a load needs no conversion");

#define MMGR_SWAR_ONES (((mmgr_word) ~(mmgr_word)0) / 0xFFu)
#define MMGR_VERBUM_SCRUTOR_HIGH (MMGR_SWAR_ONES * 0x80u)
#define MMGR_SWAR_LOW7 (MMGR_SWAR_ONES * 0x7Fu)

#define MMGR_SWAR_GO 0
#define MMGR_SWAR_YES 1
#define MMGR_SWAR_NO 2

#define MMGR_FAM_CS 0x60u
#define MMGR_FAM_CI 0x40u

#define MMGR_SCAN_MAX_WORDS ((MMGR_CARCER_MAX + (MMGR_SWAR_BYTES - 1u)) / MMGR_SWAR_BYTES)

MMGR_STATIC_ASSERT(MMGR_SCAN_MAX_WORDS *MMGR_SWAR_BYTES >= MMGR_CARCER_MAX,
                   "the worst-case scan does not cover the largest tenant");
MMGR_STATIC_ASSERT((MMGR_SCAN_MAX_WORDS - 1u) * MMGR_SWAR_BYTES < MMGR_CARCER_MAX,
                   "the worst-case scan is padded by a whole word - the word count is not tight");

typedef struct
{
    const mmgr_word w;
    const mmgr_word v;
    const mmgr_word m;
    const uint8_t byte;
    const uint8_t fam;
    const mmgr_bool ci;
} ScrutLaneCfg;

typedef struct
{
    const mmgr_word m;
    const size_t n;
    const size_t wi;
} ScrutMaskCfg;

typedef struct
{
    const mmgr_word w;
    const void *const at;
    const size_t n;
} ScrutWordCfg;

typedef struct
{
    mmgr_word (*ge)(const ScrutLaneCfg *c);
    mmgr_word (*le)(const ScrutLaneCfg *c);
    mmgr_word (*sub7)(const ScrutLaneCfg *c);
    mmgr_word (*has_zero)(const ScrutLaneCfg *c);
    mmgr_word (*eq)(const ScrutLaneCfg *c);
    mmgr_word (*xor_)(const ScrutLaneCfg *c);
    mmgr_word (*fam_eq)(const ScrutLaneCfg *c);
    mmgr_word (*any_upper)(const ScrutLaneCfg *c);
    mmgr_word (*any_digit)(const ScrutLaneCfg *c);
    mmgr_word (*alpha)(const ScrutLaneCfg *c);
    size_t (*count)(const ScrutLaneCfg *c);
    size_t (*first)(const ScrutLaneCfg *c);
    size_t (*last)(const ScrutLaneCfg *c);
} ScrutLaneNs;
MMGR_NS_LAYOUT(ScrutLaneNs, ge, le, sub7, has_zero, eq, xor_, fam_eq, any_upper, any_digit, alpha, count, first, last);

typedef struct
{
    mmgr_word (*spread)(const ScrutMaskCfg *c);
    mmgr_word (*drop_first)(const ScrutMaskCfg *c);
    mmgr_word (*drop_last)(const ScrutMaskCfg *c);
    mmgr_word (*bytes_below)(const ScrutMaskCfg *c);
    mmgr_word (*lanes_below)(const ScrutMaskCfg *c);
    mmgr_word (*before)(const ScrutMaskCfg *c);
    mmgr_word (*tail)(const ScrutMaskCfg *c);
    mmgr_word (*run)(const ScrutMaskCfg *c);
    mmgr_word (*run_edge)(const ScrutMaskCfg *c);
} ScrutMaskNs;
MMGR_NS_LAYOUT(ScrutMaskNs, spread, drop_first, drop_last, bytes_below, lanes_below, before, tail, run, run_edge);

typedef struct
{
    mmgr_word (*load)(const ScrutWordCfg *c);
    mmgr_word (*load_al)(const ScrutWordCfg *c);
    mmgr_word (*fold_lower)(const ScrutWordCfg *c);
    size_t (*count)(const ScrutWordCfg *c);
} ScrutWordNs;
MMGR_NS_LAYOUT(ScrutWordNs, load, load_al, fold_lower, count);

mmgr_word mmgr_scrut_ge(const ScrutLaneCfg *c);
mmgr_word mmgr_scrut_le(const ScrutLaneCfg *c);
mmgr_word mmgr_scrut_sub7(const ScrutLaneCfg *c);
mmgr_word mmgr_scrut_has_zero(const ScrutLaneCfg *c);
mmgr_word mmgr_scrut_eq(const ScrutLaneCfg *c);
mmgr_word mmgr_scrut_xor(const ScrutLaneCfg *c);
mmgr_word mmgr_scrut_fam_eq(const ScrutLaneCfg *c);
mmgr_word mmgr_scrut_any_upper(const ScrutLaneCfg *c);
mmgr_word mmgr_scrut_any_digit(const ScrutLaneCfg *c);
mmgr_word mmgr_scrut_alpha(const ScrutLaneCfg *c);
size_t mmgr_scrut_lane_count(const ScrutLaneCfg *c);
size_t mmgr_scrut_lane_lo(const ScrutLaneCfg *c);
size_t mmgr_scrut_lane_hi(const ScrutLaneCfg *c);

mmgr_word mmgr_scrut_spread(const ScrutMaskCfg *c);
mmgr_word mmgr_scrut_drop_lo(const ScrutMaskCfg *c);
mmgr_word mmgr_scrut_drop_hi(const ScrutMaskCfg *c);
mmgr_word mmgr_scrut_bytes_below(const ScrutMaskCfg *c);
mmgr_word mmgr_scrut_lanes_below(const ScrutMaskCfg *c);
mmgr_word mmgr_scrut_lanes_before(const ScrutMaskCfg *c);
mmgr_word mmgr_scrut_tail_mask(const ScrutMaskCfg *c);
mmgr_word mmgr_scrut_run(const ScrutMaskCfg *c);
mmgr_word mmgr_scrut_run_edge(const ScrutMaskCfg *c);

mmgr_word mmgr_scrut_load(const ScrutWordCfg *c);
mmgr_word mmgr_scrut_load_al(const ScrutWordCfg *c);
mmgr_word mmgr_scrut_fold_lower(const ScrutWordCfg *c);
size_t mmgr_scrut_words(const ScrutWordCfg *c);

MMGR_NS ScrutLaneNs lane MMGR_UNUSED = {
    .ge = mmgr_scrut_ge,
    .le = mmgr_scrut_le,
    .sub7 = mmgr_scrut_sub7,
    .has_zero = mmgr_scrut_has_zero,
    .eq = mmgr_scrut_eq,
    .xor_ = mmgr_scrut_xor,
    .fam_eq = mmgr_scrut_fam_eq,
    .any_upper = mmgr_scrut_any_upper,
    .any_digit = mmgr_scrut_any_digit,
    .alpha = mmgr_scrut_alpha,
    .count = mmgr_scrut_lane_count,
#if MMGR_HW_BIG_ENDIAN
    .first = mmgr_scrut_lane_hi,
    .last = mmgr_scrut_lane_lo,
#else
    .first = mmgr_scrut_lane_lo,
    .last = mmgr_scrut_lane_hi,
#endif
};

MMGR_NS ScrutMaskNs mask MMGR_UNUSED = {
    .spread = mmgr_scrut_spread,
#if MMGR_HW_BIG_ENDIAN
    .drop_first = mmgr_scrut_drop_hi,
    .drop_last = mmgr_scrut_drop_lo,
#else
    .drop_first = mmgr_scrut_drop_lo,
    .drop_last = mmgr_scrut_drop_hi,
#endif
    .bytes_below = mmgr_scrut_bytes_below,
    .lanes_below = mmgr_scrut_lanes_below,
    .before = mmgr_scrut_lanes_before,
    .tail = mmgr_scrut_tail_mask,
    .run = mmgr_scrut_run,
    .run_edge = mmgr_scrut_run_edge,
};

MMGR_NS ScrutWordNs word MMGR_UNUSED = {
    .load = mmgr_scrut_load,
    .load_al = mmgr_scrut_load_al,
    .fold_lower = mmgr_scrut_fold_lower,
    .count = mmgr_scrut_words,
};

MMGR_FINIS_DECLS

#endif
