# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# The headers every module is compiled against, rather than anything a module does.
# mmgr_oracle_libc.h has no suite: it is what the suites measure against.

add_subdirectory(mmgr_compiler_directives)
add_subdirectory(mmgr_config)
add_subdirectory(mmgr_string_shim)
add_subdirectory(mmgr_types)
