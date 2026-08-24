#ifndef MMGR_CONFIG_H
#define MMGR_CONFIG_H

#include <stdint.h>

#include "config/mmgr_compiler_directives.h"

#ifndef MMGR_WORD_BITS
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#define MMGR_WORD_BITS 64
#elif UINTPTR_MAX == 0xFFFFFFFFu
#define MMGR_WORD_BITS 32
#elif UINTPTR_MAX == 0xFFFFu
#define MMGR_WORD_BITS 16
#else
#error "cannot derive MMGR_WORD_BITS from UINTPTR_MAX on this target - pass -DMMGR_WORD_BITS=16|32|64"
#endif
#endif

#ifndef MMGR_INDEX_BITS
#if MMGR_WORD_BITS < 32
#define MMGR_INDEX_BITS MMGR_WORD_BITS
#else
#define MMGR_INDEX_BITS 32
#endif
#endif

#ifndef MMGR_ALIGN_BYTES
#define MMGR_ALIGN_BYTES 16u
#endif

#include "config/mmgr_types.h"

#ifdef MMGR_SWAR_BITS
#undef MMGR_SWAR_BITS
#endif
#define MMGR_SWAR_BITS MMGR_WORD_BITS

#ifndef MMGR_ASSERT
#define MMGR_ASSERT(cond, msg) ((void)sizeof((cond) ? 1 : 0), (void)0)
#endif

#ifndef MMGR_DEBUG_CHECKS
#define MMGR_DEBUG_CHECKS 0
#endif

#ifndef MMGR_PLAINTEXT_CONFIN_SIZE
#define MMGR_PLAINTEXT_CONFIN_SIZE 4096u
#endif
#ifndef MMGR_SECURE_CONFIN_SIZE
#define MMGR_SECURE_CONFIN_SIZE 4096u
#endif

#ifndef MMGR_CARCER_MAX
#if MMGR_PLAINTEXT_CONFIN_SIZE >= MMGR_SECURE_CONFIN_SIZE
#define MMGR_CARCER_MAX ((size_t)MMGR_PLAINTEXT_CONFIN_SIZE)
#else
#define MMGR_CARCER_MAX ((size_t)MMGR_SECURE_CONFIN_SIZE)
#endif
#endif

#ifndef MMGR_CARCER_MAX_REGIONS
#define MMGR_CARCER_MAX_REGIONS 2u
#endif

#define MMGR_CARCER_MACHINERY                                                                                          \
    const CarcerInit init;                                                                                             \
    CarcerCtx pool[MMGR_CARCER_MAX_REGIONS];                                                                           \
    size_t mark[MMGR_CARCER_MAX_REGIONS]

#define MMGR_POOL(name_, n_) name_, n_

#define MMGR_CARCER_EXISTS(region_, n_)                                                                                \
    MMGR_STATIC_ASSERT(sizeof(((region_##_layout *)0)->bytes) != 0u, #region_ " has no address");                      \
    MMGR_STATIC_ASSERT((n_) != 0u, #region_ " has no extent")

#define MMGR_CARCER_CHECK(region_, name_, off_, n_)                                                                    \
    MMGR_STATIC_ASSERT((n_) >= (2u * MMGR_ALIGN_BYTES), #name_ " is too small to hold a block");                       \
    MMGR_STATIC_ASSERT(((n_) & (MMGR_ALIGN_BYTES - 1u)) == 0u, #name_ " is not a whole number of aligned units");      \
    MMGR_STATIC_ASSERT(((off_) + (n_)) <= sizeof(((region_##_layout *)0)->bytes),                                      \
                       #name_ " does not fit inside " #region_)

#define MMGR_CARCER_R4(region_, n_, a_, an_, b_, bn_)                                                                  \
    typedef struct                                                                                                     \
    {                                                                                                                  \
        MMGR_CARCER_MACHINERY;                                                                                         \
        MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t bytes[(n_)];                                                              \
    } region_##_layout;                                                                                                \
    enum                                                                                                               \
    {                                                                                                                  \
        a_ = 0,                                                                                                        \
        b_ = 1,                                                                                                        \
        region_##_count = 2                                                                                            \
    };                                                                                                                 \
    MMGR_STATIC_ASSERT(region_##_count <= MMGR_CARCER_MAX_REGIONS, #region_ " carves past its limit");                 \
    MMGR_CARCER_EXISTS(region_, n_);                                                                                   \
    MMGR_CARCER_CHECK(region_, a_, 0, an_);                                                                            \
    MMGR_CARCER_CHECK(region_, b_, an_, bn_);                                                                          \
    region_##_layout region_ = {.init = {.at = region_.bytes, .size = (n_)},                                           \
                                .pool = {{.base = region_.bytes + 0, .size = (an_), .interim_top = (an_)},             \
                                         {.base = region_.bytes + (an_), .size = (bn_), .interim_top = (bn_)}}}

#define MMGR_CARCER_R2(region_, n_, a_, an_)                                                                           \
    typedef struct                                                                                                     \
    {                                                                                                                  \
        MMGR_CARCER_MACHINERY;                                                                                         \
        MMGR_ALIGN(MMGR_ALIGN_BYTES) uint8_t bytes[(n_)];                                                              \
    } region_##_layout;                                                                                                \
    enum                                                                                                               \
    {                                                                                                                  \
        a_ = 0,                                                                                                        \
        region_##_count = 1                                                                                            \
    };                                                                                                                 \
    MMGR_STATIC_ASSERT(region_##_count <= MMGR_CARCER_MAX_REGIONS, #region_ " carves past its limit");                 \
    MMGR_CARCER_EXISTS(region_, n_);                                                                                   \
    MMGR_CARCER_CHECK(region_, a_, 0, an_);                                                                            \
    region_##_layout region_ = {.init = {.at = region_.bytes, .size = (n_)},                                           \
                                .pool = {{.base = region_.bytes + 0, .size = (an_), .interim_top = (an_)}}}

#define mmgr_carcer_init(region_, n_, ...) MMGR_CAT(MMGR_CARCER_R, MMGR_NARG(__VA_ARGS__))(region_, n_, __VA_ARGS__)

#ifndef MMGR_ENABLE_DMA
#define MMGR_ENABLE_DMA 0
#endif
#ifndef MMGR_ENABLE_EXTRAM
#define MMGR_ENABLE_EXTRAM 0
#endif
#ifndef MMGR_ENABLE_KEEPOUT
#define MMGR_ENABLE_KEEPOUT 0
#endif
#ifndef MMGR_ENABLE_HW_MEM_CAPACITY_CB
#define MMGR_ENABLE_HW_MEM_CAPACITY_CB 0
#endif
#ifndef MMGR_RING_DRAINS
#define MMGR_RING_DRAINS 4u
#endif

#ifndef MMGR_ENABLE_CLOCK
#define MMGR_ENABLE_CLOCK 0
#endif

#if MMGR_ENABLE_CLOCK
#ifndef MMGR_RING_ATTACH_US
#define MMGR_RING_ATTACH_US 100u
#endif
#endif

#if MMGR_ENABLE_DMA
#ifndef MMGR_PRAET_CHANNELS
#define MMGR_PRAET_CHANNELS 2
#endif
#ifndef MMGR_PRAET_BUF_SIZE
#define MMGR_PRAET_BUF_SIZE 256
#endif
#endif

#define MMGR_MEMOR_IS_SIZE(x_) ((void)_Generic((x_), size_t: 0))
#define MMGR_MEMOR_IS_BYTE(x_) ((void)_Generic((x_), uint8_t: 0))

#define MMGR_CARCER_POOL(region_, pool_) (&(region_).pool[pool_])

#define MMGR_CARCER(region_, pool_, ...) (&(CarcerCfg){.pool = MMGR_CARCER_POOL(region_, pool_), __VA_ARGS__})

#endif
