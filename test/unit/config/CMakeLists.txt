# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The headers every module is compiled against, and nothing a module does.
# mmgr_oracle_libc.h has no suite: it is what the suites measure against.
#
# The widths, the compiler directives and the dispatch layout are embedded_types', and that
# repository carries their suite, as it does for the lane and the clock selection. The string shim is
# this repository's, and it has its suite here.

add_subdirectory(mmgr_string_shim)

