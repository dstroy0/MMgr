# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# The engine is inline inside this translation unit and a call from outside it. Header-only was
# right when one module converted numbers; at two, always_inline put a copy of the whole thing
# in each. See transformo.c.
mmgr_add_module(transformo
  SOURCES transformo.c
  DEPS
    fractio
    pow5
)
