# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
# negotiated commercial licensing contract or an educator's license issued to you personally.

# memset, memcpy, memmove and memcmp as symbols, for a program with no C library. OPT_IN keeps the target out of
# mmgr_<env>: a program links mmgr_string_shim_<env> itself where it wants them, and a suite that checks MMgr
# against the C library keeps the C library's.
mmgr_add_module(string_shim
  SOURCES mmgr_string_shim.c
  DEPS memoria_operor
  OPT_IN
)

# No LTO for this target. The code generator writes its calls to these names after LTO chose what to keep, and an
# LTO that saw no call to them drops them from the image.
foreach(entry IN LISTS MMGR_ENVIRONMENTS)
  mmgr_env_name("${entry}" env defs)
  set_target_properties(mmgr_string_shim_${env} PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
endforeach()
