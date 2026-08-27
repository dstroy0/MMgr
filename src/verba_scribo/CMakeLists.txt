# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# -O3 adds 2272 bytes here, 21% on top of the 10812 this module takes at -O2, and buys
# about 1.5% on a render. The digit loops are short and already shaped; there is nothing
# for it to unroll that pays for itself. Sizes measured per translation unit with no LTO;
# see docs/quality/optimization.md for the table and the command.
mmgr_add_module(verba_scribo
  SOURCES verba_scribo.c
  DEPS
    cellularum_laboro
    clz
    fractio
    transformo
    proximus_operor
  OPTIMIZE -O2
)
