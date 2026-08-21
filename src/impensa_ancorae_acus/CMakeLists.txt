# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# No .c. The module is one const table selected at compile time from a set of profile headers, so
# there is nothing to compile - but it still needs a target per environment so a consumer picks it
# up without crossing width settings, and so the profile headers are visible to the include root.
mmgr_add_module(impensa_ancorae_acus
  HEADER_ONLY
)
