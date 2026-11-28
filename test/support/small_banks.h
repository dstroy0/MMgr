/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file small_banks.h
 * @brief The banks of the build-banks tree: 64 bytes where a field of the closed word has 20 bits, so that a pool
 *        of every suite spans many banks and each writer that stores a boundary moves a frame.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-27
 *
 * @note The tree forces this header into every unit ahead of its own includes. The value expands where the cellblock
 *       reads it, after locus_carcerum.h defines the field bits.
 * @note A closed word of 32 or 16 bits has fields of 9 or 4 bits. Banks of 64 bytes there cap a pool below the pools
 *       of some suites, and so those words keep the default bank.
 */
#ifndef MMGR_SMALL_BANKS_H
#define MMGR_SMALL_BANKS_H

#define MMGR_CARCER_BANK_SHIFT ((MMGR_CARCER_BANK_FIELD_BITS >= 20u) ? 6u : (MMGR_CARCER_HALF_BITS - 2u))

#endif
