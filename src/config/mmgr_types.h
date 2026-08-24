#ifndef MMGR_TYPES_H
#define MMGR_TYPES_H

#include <stddef.h>
#include <stdint.h>

#include "config/mmgr_compiler_directives.h"

typedef uint8_t mmgr_u8;
typedef uint16_t mmgr_u16;
typedef uint32_t mmgr_u32;
typedef uint64_t mmgr_u64;
typedef int8_t mmgr_i8;
typedef int16_t mmgr_i16;
typedef int32_t mmgr_i32;
typedef int64_t mmgr_i64;

#ifdef __cplusplus
typedef bool mmgr_bool;
#else
typedef _Bool mmgr_bool;
#endif
#define MMGR_TRUE ((mmgr_bool)1)
#define MMGR_FALSE ((mmgr_bool)0)

#if !defined(MMGR_WORD_BITS) || !defined(MMGR_INDEX_BITS)
#error "mmgr_types.h is not a standalone header - include mmgr_config.h, which sets the widths first"
#endif

#if MMGR_WORD_BITS == 64
typedef mmgr_u64 mmgr_word;
typedef mmgr_i64 mmgr_iword;
#elif MMGR_WORD_BITS == 32
typedef mmgr_u32 mmgr_word;
typedef mmgr_i32 mmgr_iword;
#elif MMGR_WORD_BITS == 16
typedef mmgr_u16 mmgr_word;
typedef mmgr_i16 mmgr_iword;
#else
#error "MMGR_WORD_BITS must be 16, 32 or 64 - see mmgr_config.h"
#endif

#if MMGR_INDEX_BITS == 32
typedef mmgr_u32 mmgr_idx;
#elif MMGR_INDEX_BITS == 16
typedef mmgr_u16 mmgr_idx;
#else
#error "MMGR_INDEX_BITS must be 16 or 32 - see mmgr_config.h"
#endif

MMGR_STATIC_ASSERT(sizeof(mmgr_u8) == 1, "mmgr_u8 must be exactly 8 bits: this target has no 8-bit type");
MMGR_STATIC_ASSERT(sizeof(mmgr_u16) * 8u == 16u, "mmgr_u16 must be exactly 16 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_u32) * 8u == 32u, "mmgr_u32 must be exactly 32 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_u64) * 8u == 64u, "mmgr_u64 must be exactly 64 bits");
MMGR_STATIC_ASSERT(sizeof(mmgr_word) * 8u == MMGR_WORD_BITS, "mmgr_word must be exactly MMGR_WORD_BITS wide");
MMGR_STATIC_ASSERT(sizeof(mmgr_iword) == sizeof(mmgr_word), "the signed word must be the same register as the word");
MMGR_STATIC_ASSERT(sizeof(mmgr_idx) * 8u == MMGR_INDEX_BITS, "mmgr_idx must be exactly MMGR_INDEX_BITS wide");
MMGR_STATIC_ASSERT(sizeof(mmgr_idx) <= sizeof(mmgr_word), "an index must fit the register it is carried in");

typedef enum MMGR_ENUM_PACKED
{
    MMGR_ENUM_PROBE_MIN = 0,
    MMGR_ENUM_PROBE_MAX = 255,
} MmgrEnumProbe;
MMGR_STATIC_ASSERT(sizeof(MmgrEnumProbe) == 1,
                   "MMGR_ENUM_PACKED is not honored here, so no enum keeps its declared width and every "
                   "borrow offset computed from a struct containing one is wrong (TI: pass --small_enum)");

#endif
