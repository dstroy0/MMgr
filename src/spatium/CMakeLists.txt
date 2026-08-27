# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# A span is a value type with pure walks over it and no storage of its own, so this reaches nothing
# but config. It sits below carceribus, byteio and anything else that hands a region to a caller.
mmgr_add_module(spatium
  SOURCES spatium.c
  DEPS
)