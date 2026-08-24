/**
 * @brief Spans over a caller's buffer: the span type, the argument, and the spat dispatch table.
 */
#ifndef MMGR_SPATIUM_H
#define MMGR_SPATIUM_H

#include "config/mmgr_config.h"

MMGR_INCIPE_DECLS

/**
 * @brief A buffer, the bytes in it, and a position within it.
 *
 * @note Returned by value from mmgr_spat_init; this module keeps none of its own.
 * @warning buf points at the caller's storage, which must outlive every use of the span [BORROWS].
 */
typedef struct
{
    uint8_t *buf; /**< The buffer the span covers [BORROWS]. */
    size_t cap;   /**< Bytes in buf. */
    size_t pos;   /**< Offset into buf, which mmgr_spat_init sets to 0. */
} mmgr_spat;

/**
 * @brief Arguments for the spat call.
 */
typedef struct
{
    uint8_t *const buf; /**< Buffer the span will cover [BORROWS]. */
    const size_t cap;   /**< Bytes in buf. */
} SpatCfg;

/**
 * @brief Type of the spat dispatch table.
 *
 * @note MMGR_NS_LAYOUT asserts the init member is at offset 0 and that the struct holds nothing else.
 */
typedef struct
{
    mmgr_spat (*init)(const SpatCfg *c); /**< Builds a span over a buffer. */
} SpatiumNs;
MMGR_NS_LAYOUT(SpatiumNs, init);

/**
 * @brief Builds a span over c->buf, taking cap from c->cap and setting pos to 0.
 *
 * @param[in] c Buffer and its capacity [BORROWS].
 * @return      The span, by value.
 * @note Asserts c->buf is not NULL and c->cap is not 0; the default MMGR_ASSERT leaves both unevaluated.
 * @warning The span carries c->buf away, so that buffer must stay valid for as long as the span is used [BORROWS].
 */
mmgr_spat mmgr_spat_init(const SpatCfg *c);

/**
 * @brief Dispatch table instance named spat; its init member calls mmgr_spat_init.
 */
MMGR_NS SpatiumNs spat MMGR_UNUSED = {
    .init = mmgr_spat_init,
};

MMGR_FINIS_DECLS

#endif
