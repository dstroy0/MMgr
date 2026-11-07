# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py prove: each proof harness under test/prove held on the C source and on the machine code compilers lower.

A harness is a C program of inputs, assumptions and assertions (prove.h) that states a theorem of
theory/locus_carcerum on the real functions. Two provers hold it:

  cbmc      the C source, with every source the library links, unwound to the harness's bound with unwinding
            assertions, so a bound too small for a loop fails rather than passes. A second run with PROVE_REACH
            must fail: it asserts the end of the harness unreachable, and a harness whose assumptions leave no path
            would pass every assertion and prove nothing.
  machine   the harness built by each toolchain here at each optimization level and run from main by the symbolic
            executor (symbolic.py) with every input a symbol, every path to its end; the property is held on the
            instructions the compiler emitted.

Both run in WSL on a Windows host and directly on Linux. CBMC comes from the distribution; the executor from a
virtual environment at ~/.venvs/angr. Everything a run builds lands in one folder under the build container.
"""

import glob
import os
import shlex
import subprocess

# Each harness's bound: the steps its sequence runs, and the loop unwinding CBMC holds it to.
BOUNDS = {
    "locus_carcerum_fragmentation": {"defines": [], "unwind": 0},
    "locus_carcerum_sequence": {"defines": ["PROVE_STEPS=2u"], "unwind": 12},
    "locus_carcerum_movable": {"defines": ["PROVE_STEPS=2u"], "unwind": 12},
}

# The toolchains of the machine arm and the levels each is held at.
MACHINE = [("gcc", "-O0"), ("gcc", "-O2"), ("clang", "-O0"), ("clang", "-O2")]

DEFINES = ["MMGR_ENABLE_DMA=0", "MMGR_ENABLE_EXTRAM=0", "MMGR_CARCER_OWNERS=1024u"]
INCLUDES = ["src", "include", "deps/embedded_types/include", "test/prove", "test/support"]
EXECUTOR = "~/.venvs/angr/bin/python"


def library_sources(root):
    """The sources the library links, as verify links them, relative to the root."""
    sources = []
    for path in sorted(glob.glob(os.path.join(root, "src", "*", "*.c"))):
        rel = os.path.relpath(path, root).replace("\\", "/")
        module = rel.split("/")[1]
        if module in ("memoria_externa", "memoriam_praetereo", "confinium_exclusivum_infinitas", "confinium_externum"):
            continue
        if module == "impensa_ancorae_acus" and not rel.endswith("impensa_ancorae_acus_generic.c"):
            continue
        sources.append(rel)
    sources.append("test/support/host_wait.c")
    return sources


def linux(root, command):
    """A shell command run in the root on Linux, through WSL on a Windows host."""
    if os.name == "nt":
        return subprocess.run(["wsl", "-e", "bash", "-lc", command], cwd=root, capture_output=True, text=True)
    return subprocess.run(["bash", "-lc", command], cwd=root, capture_output=True, text=True)


def cbmc(root, harness, bound, reach):
    # The library's cellblocks, owner table, turnstiles and statics pass CBMC's default of 256 addressed objects
    flags = ["--object-bits 12", "-D PROVE_CBMC", "-D MMGR_OWNERS_IN_THIS_UNIT", "-D MMGR_MISNAMED_IN_THIS_UNIT"]
    flags += ["-D " + d for d in DEFINES + bound["defines"]] + ["-I " + i for i in INCLUDES]
    if reach:
        flags.append("-D PROVE_REACH")
    if bound["unwind"]:
        flags += ["--unwind %d" % bound["unwind"], "--unwinding-assertions"]
    command = "cbmc %s %s %s" % (" ".join(flags), shlex.quote(harness), " ".join(library_sources(root)))
    r = linux(root, command)
    text = r.stdout + r.stderr
    failed = [line.strip() for line in text.splitlines() if line.rstrip().endswith("FAILURE")]
    verdict = "VERIFICATION SUCCESSFUL" in text
    return verdict, failed, text


def machine(root, harness, bound, cc, level, out):
    program = "%s/%s_%s%s" % (out, os.path.splitext(os.path.basename(harness))[0], cc, level.replace("-", "_"))
    flags = ["-std=c11", level, "-g"] + ["-I " + i for i in INCLUDES] + ["-D " + d for d in DEFINES + bound["defines"]]
    build = "%s %s %s test/prove/prove_hooks.c %s -lpthread -lm -o %s" % (
        cc, " ".join(flags), shlex.quote(harness), " ".join(library_sources(root)), program)
    r = linux(root, build)
    if r.returncode != 0:
        return "build failed: " + (r.stdout + r.stderr).strip().splitlines()[-1]
    r = linux(root, "%s test/prove/symbolic.py %s" % (EXECUTOR, program))
    lines = [line for line in (r.stdout + r.stderr).splitlines()
             if line.startswith(("proved", "counterexample", "incomplete"))]
    return lines[-1] if lines else "no answer: " + (r.stdout + r.stderr).strip()[-200:]


def prove(root, run_root, names, arms):
    harnesses = sorted(glob.glob(os.path.join(root, "test", "prove", "*_prove.c")))
    chosen = [h for h in harnesses if not names or os.path.basename(h)[:-len("_prove.c")] in names]
    os.makedirs(run_root, exist_ok=True)
    out = os.path.relpath(run_root, root).replace("\\", "/")
    failures = []
    for path in chosen:
        name = os.path.basename(path)[:-len("_prove.c")]
        harness = os.path.relpath(path, root).replace("\\", "/")
        bound = BOUNDS.get(name)
        if bound is None:
            failures.append("%s: no bound in test/prove/prove.py" % name)
            continue
        print("== %s" % name)
        if "cbmc" in arms:
            held, failed, text = cbmc(root, harness, bound, reach=False)
            reached, _, _ = cbmc(root, harness, bound, reach=True)
            with open(os.path.join(run_root, name + "_cbmc.txt"), "w", encoding="utf-8") as log:
                log.write(text)
            print("  cbmc      %s%s" % ("proved" if held and not failed else "FAILED",
                                       "" if not reached else "  (VACUOUS: the end is unreachable)"))
            for line in failed[:6]:
                print("      " + line)
            if not held or failed:
                failures.append("%s cbmc: %s" % (name, "; ".join(failed[:3]) or "not proved"))
            if reached:
                failures.append("%s cbmc: the end of the harness is unreachable" % name)
        if "machine" in arms:
            for cc, level in MACHINE:
                answer = machine(root, harness, bound, cc, level, out)
                print("  %-5s %-3s %s" % (cc, level, answer))
                if not answer.startswith("proved"):
                    failures.append("%s %s %s: %s" % (name, cc, level, answer))
    print("prove: run in %s" % run_root)
    if failures:
        print("%d failures:" % len(failures))
        for failure in failures:
            print("  " + failure)
        return 1
    return 0
