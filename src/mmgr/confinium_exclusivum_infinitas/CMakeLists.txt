# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# No .c: every entry is inline in the header. It still needs a target per
# environment so a consumer links it without crossing width settings.
mmgr_add_module(confinium_exclusivum_infinitas
  HEADER_ONLY
  DEPS
    proximus_operor
    spatium
)
