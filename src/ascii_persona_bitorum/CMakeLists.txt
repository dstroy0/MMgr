# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# The masks are file local. A header full of static const tables is a copy of all of them in
# every translation unit that wanted to ask one question.
mmgr_add_module(ascii_persona_bitorum
  SOURCES ascii_persona_bitorum.c
)
