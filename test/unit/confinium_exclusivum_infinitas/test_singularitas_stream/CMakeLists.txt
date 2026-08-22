# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later

# The translation unit is compiled in, not linked, so a case can see the reservation word and the
# ingestion path's own record.
#
# CAPABILITY KEEPOUT because the point of the suite is the stream running while drains are worked,
# and without keepouts there are no drains to work it against.
mmgr_add_suite(test_singularitas_stream CAPABILITY KEEPOUT)
