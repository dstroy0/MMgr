// memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef MMGR_BITIO_H
#define MMGR_BITIO_H

#include "mmgr_config.h"

MMGR_BEGIN_DECLS

/**
 * @file bitio.h
 * @brief Write bits into a byte buffer, most significant first.
 */

/**
 * @brief Bit writer state.
 *
 * Bits accumulate in @c acc until a whole byte is ready. @c overflow latches once the buffer is
 * full and is never cleared, so one check at the end covers the whole run.
 */
typedef struct
{
    uint8_t *out;
    size_t cap;
    size_t cnt;
    uint32_t acc;
    int nbits;
    mmgr_bool overflow;
} mmgr_bitio_writer;

/** @brief Dispatch table. Addressed by offset, so the layout is asserted below. */
typedef struct
{
    void (*put)(mmgr_bitio_writer *w, uint32_t bits, int n);
    void (*align)(mmgr_bitio_writer *w);
} BitioNs;
MMGR_NS_LAYOUT(BitioNs, put, align);

/**
 * @brief Append @p n bits.
 * @param w Writer.
 * @param bits Value, in the low @p n bits.
 * @param n Bit count.
 */
void mmgr_bitio_put(mmgr_bitio_writer *w, uint32_t bits, int n);
/**
 * @brief Pad with zero bits to the next byte boundary.
 * @param w Writer.
 */
void mmgr_bitio_align(mmgr_bitio_writer *w);

/** @brief Module namespace. */
MMGR_NS BitioNs bitio MMGR_UNUSED = {.put = mmgr_bitio_put, .align = mmgr_bitio_align};

MMGR_END_DECLS

#endif
