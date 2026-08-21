# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# -O3 more than doubles this one, 336 bytes to 816, and moves nothing measurable: the
# entries are single loads and stores that are already one instruction each. See
# docs/quality/optimisation.md.
mmgr_add_module(proximus_operor
  SOURCES proximus_operor.c
  OPTIMIZE -O2
)
