# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py prove: each proof harness under test/prove proved on the C source and on the machine code compilers lower.

A harness is a C program of inputs, assumptions and assertions (prove.h) that states a theorem of
theory/locus_carcerum on the real functions. Two provers hold it:

  cbmc      the C source, with every source the library links, unwound to the harness's bound with unwinding
            assertions, and a bound too small for a loop fails and does not pass. A second run with PROVE_REACH
            must fail: it asserts the end of the harness unreachable, and a harness whose assumptions leave no path
            would pass every assertion and prove nothing.
  machine   the harness built by each toolchain here at each optimization level and run from main by the symbolic
            executor (symbolic.py) with every input a symbol, every path to its end; the property is proved on the
            instructions the compiler emitted.

A third prover holds the threads:

  genmc     each client under test/prove named *_genmc.c, two or three threads on the real translation unit, run
            by GenMC under every memory model in GENMC_MODELS. GenMC explores every execution the model allows and
            reports a failed assertion, a data race, or a thread blocked in a sleep no wake ends (genmc_host.h). A
            client that passes under one model and fails under another names the ordering the code needs.

All three run in WSL on a Windows host and directly on Linux. CBMC comes from the distribution; the executor from a
virtual environment at ~/.venvs/angr; GenMC from its source at ~/src/genmc, built and installed to ~/.local by the
first run that needs it. Everything a run builds lands in one folder under the build container.
"""

import glob
import os
import shlex
import subprocess

# Each harness's bound: the steps its sequence runs, and the loop unwinding CBMC holds it to. A bound with hosted
# False needs more memory than a hosted runner has, and prove --hosted names it and does not run it. The movable
# harness uses more than 27 GB under CBMC, and a hosted runner has 16 GB.
BOUNDS = {
    "locus_carcerum_fragmentation": {"defines": [], "unwind": 0, "hosted": True},
    "locus_carcerum_sequence": {"defines": ["PROVE_STEPS=2u"], "unwind": 12, "hosted": True},
    "locus_carcerum_movable": {"defines": ["PROVE_STEPS=2u"], "unwind": 12, "hosted": False},
}

# The toolchains of the machine arm and the levels each is proved at.
MACHINE = [("gcc", "-O0"), ("gcc", "-O2"), ("clang", "-O0"), ("clang", "-O2")]

DEFINES = ["MMGR_ENABLE_DMA=0", "MMGR_ENABLE_EXTRAM=0", "MMGR_CARCER_OWNERS=1024u"]
INCLUDES = ["src", "include", "deps/embedded_types/include", "test/prove", "test/support"]
EXECUTOR = "~/.venvs/angr/bin/python"

# The memory models of the genmc arm: sequential consistency, x86, C11, the hardware model under C11, and Armv8.
GENMC = "~/.local/bin/genmc"
GENMC_MODELS = ["--sc", "--tso", "--rc11", "--imm", "--arm8"]
# GenMC's load annotation, spin-assume and loop jump threading read each address in a loop's condition, and stop on
# a constant address other than NULL, the sink a branch-free pick names. With the three off, every loop runs to the
# unroll bound, which covers the clients' loops whole, and a spin's end is not proved.
GENMC_FLAGS = ["--disable-estimation", "--unroll=8", "--disable-load-annotation", "--disable-spin-assume",
               "--disable-loop-jump-threading"]
GENMC_DEFINES = ["MMGR_ENABLE_DMA=0", "MMGR_ENABLE_EXTRAM=0", "MMGR_CARCER_OWNERS=8u"]
GENMC_SOURCE = "https://github.com/MPI-SWS/genmc.git"
# The release the clients use. Each host builds this tag. Thus, each host runs the same checker.
GENMC_TAG = "v0.20.0"
# The packages GenMC builds against, LLVM 20 among those its source supports
GENMC_PACKAGES = "cmake llvm-20-dev libffi-dev zlib1g-dev libedit-dev bc g++-14 libzstd-dev libxml2-dev"


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


def linux_root(root, command):
    """A shell command run as root on Linux: through WSL's root user on a Windows host, through sudo on Linux."""
    if os.name == "nt":
        return subprocess.run(["wsl", "-u", "root", "-e", "bash", "-lc", command], cwd=root, capture_output=True,
                              text=True)
    return subprocess.run(["bash", "-lc", "sudo -n " + command], cwd=root, capture_output=True, text=True)


def genmc_ready(root):
    """None where GenMC runs, after a build where it is missing: its packages, its source, its install. The reason
    where a step failed."""
    if linux(root, "test -x %s" % GENMC).returncode == 0:
        return None
    print("  genmc     building at ~/src/genmc, installing to ~/.local")
    r = linux_root(root, "env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq %s" % GENMC_PACKAGES)
    if r.returncode != 0:
        return "its packages did not install: " + (r.stdout + r.stderr).strip()[-200:]
    steps = [
        "mkdir -p ~/src",
        "test -d ~/src/genmc/.git || git clone --depth 1 --branch %s %s ~/src/genmc" % (GENMC_TAG, GENMC_SOURCE),
        "cd ~/src/genmc && CC=gcc-14 CXX=g++-14 cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo "
        "-DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm -DCMAKE_INSTALL_PREFIX=$HOME/.local -B RelWithDebInfo -S .",
        "cd ~/src/genmc && cmake --build RelWithDebInfo -j 8 && cmake --install RelWithDebInfo",
    ]
    for step in steps:
        r = linux(root, step)
        if r.returncode != 0:
            return "its build stopped at `%s`: %s" % (step.split("&&")[-1].strip(), (r.stdout + r.stderr).strip()[-200:])
    return None


def genmc(root, client, model, run_root, name):
    """One client under one memory model: proved with its count of executions, or the first fault GenMC names."""
    flags = GENMC_FLAGS + ["--"] + ["-I " + i for i in INCLUDES] + ["-D" + d for d in GENMC_DEFINES]
    r = linux(root, "%s %s %s %s" % (GENMC, model, " ".join(flags), shlex.quote(client)))
    text = r.stdout + r.stderr
    with open(os.path.join(run_root, "%s_genmc_%s.txt" % (name, model.lstrip("-"))), "w", encoding="utf-8") as log:
        log.write(text)
    explored = [line for line in text.splitlines() if line.startswith("Number of complete executions explored")]
    blocked = [line for line in text.splitlines() if "blocked" in line.lower() and any(c.isdigit() and c != "0"
                                                                                     for c in line.split(":")[-1])]
    if "No errors were detected" in text and not blocked:
        return "proved, %s executions" % (explored[-1].split(":")[-1].strip() if explored else "?")
    faults = [line.strip() for line in text.splitlines()
              if line.startswith(("Error", "Assertion violation", "INTERNAL", "LLVM ERROR"))] + blocked
    return "FAILED: " + (faults[0] if faults else text.strip()[-200:])


def prove(root, run_root, names, arms, hosted=False):
    harnesses = sorted(glob.glob(os.path.join(root, "test", "prove", "*_prove.c")))
    chosen = [h for h in harnesses if not names or os.path.basename(h)[:-len("_prove.c")] in names]
    if not ({"cbmc", "machine"} & set(arms)):
        chosen = []
    os.makedirs(run_root, exist_ok=True)
    out = os.path.relpath(run_root, root).replace("\\", "/")
    failures = []
    if "genmc" in arms:
        clients = sorted(glob.glob(os.path.join(root, "test", "prove", "*_genmc.c")))
        clients = [c for c in clients if not names or os.path.basename(c)[:-len("_genmc.c")] in names]
        reason = genmc_ready(root) if clients else None
        if reason:
            failures.append("genmc: " + reason)
            clients = []
        for path in clients:
            name = os.path.basename(path)[:-len("_genmc.c")]
            client = os.path.relpath(path, root).replace("\\", "/")
            print("== %s" % name)
            for model in GENMC_MODELS:
                answer = genmc(root, client, model, run_root, name)
                print("  genmc %-6s %s" % (model.lstrip("-"), answer))
                if not answer.startswith("proved"):
                    failures.append("%s genmc %s: %s" % (name, model.lstrip("-"), answer))
    for path in chosen:
        name = os.path.basename(path)[:-len("_prove.c")]
        harness = os.path.relpath(path, root).replace("\\", "/")
        bound = BOUNDS.get(name)
        if bound is None:
            failures.append("%s: no bound in test/prove/prove.py" % name)
            continue
        print("== %s" % name)
        if hosted and not bound["hosted"]:
            print("  not run: its bound needs more memory than a hosted runner has")
            continue
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
