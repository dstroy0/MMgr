/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file suite_runner.c
 * @brief Pulls in the generated Unity runner, whose main is renamed to praet_suite_main.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note The runner is generated from the case names in the suite source, so a case added there is
 *       registered here without anything being written by hand. Regenerate it with harness.py build.
 * @warning A stale runner registers the cases that existed when it was written and the rest never
 *          run, while the suite still reports green. harness.py remote regenerates before it sends
 *          for exactly that reason, and a device build has to be given a current one.
 */
#include "../../../integration/test_praet_correctness/unity_runner.c"
