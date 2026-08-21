# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# -O3 adds 3648 bytes here, 68% more than the whole module at -O2, and buys 1.5% on a
# render. The digit loops are short and already shaped; there is nothing for it to unroll
# that pays for itself. See docs/quality/optimisation.md.
mmgr_add_module(verba_scribo
  SOURCES verba_scribo.c
  DEPS
    cellularum_laboro
    fractio
    transformo
    proximus_operor
  OPTIMIZE -O2
)
