# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

# The one suite that links the string shim's symbols. Every other suite keeps the C library's memset, memcpy, memmove
# and memcmp, which some of them check MMgr against.
mmgr_add_suite(test_mmgr_string_shim_symbols LINK string_shim)
