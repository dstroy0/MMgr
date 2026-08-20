# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# No .c. Every entry is MMGR_INLINE in the header, because these are the innermost operations in the
# library - one SWAR word is about six instructions, and an out-of-line call costs more than the
# work it wraps. It still needs a target per environment so a consumer links it without crossing
# width settings.
mmgr_add_module(verbum_scrutor
  HEADER_ONLY
  DEPS
    proximus_operor
)
