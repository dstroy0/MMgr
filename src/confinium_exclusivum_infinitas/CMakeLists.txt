# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# The bodies live in the .c so that <stdatomic.h> is that file's problem. A consumer that names a
# ring should not inherit the header that makes the cursors work.
mmgr_add_module(confinium_exclusivum_infinitas
  SOURCES confinium_exclusivum_infinitas.c
  DEPS
    proximus_operor
)
