# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# src/ is the include root: every header is reached as <mmgr/<module>/<module>.h> and
# mmgr_config.h resolves with the same single -I, so a consumer adds one directory.
set(MMGR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "include root")

add_subdirectory(mmgr)

# One aggregate per environment, so a test or a consumer links `mmgr_<env>` rather than
# naming seventeen modules and getting the list wrong.
foreach(entry IN LISTS MMGR_ENVIRONMENTS)
  mmgr_env_name("${entry}" env defs)
  get_property(_mods GLOBAL PROPERTY MMGR_TARGETS_${env})
  add_library(mmgr_${env} INTERFACE)
  target_link_libraries(mmgr_${env} INTERFACE ${_mods})
endforeach()
