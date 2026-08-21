# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# No .c. The engine is inline: every entry is a handful of shifts and adds, and a call frame
# would cost more than the body.
mmgr_add_module(transformo
  HEADER_ONLY
  DEPS
    fractio
    pow5
)
