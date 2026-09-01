#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""MMgr test harness: suite discovery and Unity runner generation.

  harness.py build [--tree T] [--fresh]       configure if needed, then build
  harness.py test [--tree T] [--filter RE]    build, then run the suites
  harness.py ab                               both sides of the A/B, one after the other
  harness.py coverage [--worst N] [--gaps]    build, run and report what src/ the suites reached
  harness.py trees                            which build trees exist and which one each name uses
  harness.py device list                      which on-device benches exist and what is built
  harness.py device build B --target T        configure and build one bench for one part
  harness.py device flash B --target T --port P   flash the image that build produced
  harness.py suites                          every suite, its cases, and the capabilities it needs
  harness.py runners gen <dir> --unity <rb>  write <dir>/unity_runner.c
  harness.py cases <dir>                     what Unity will register, and what it will walk past
  harness.py generated                       are the generated headers what their generators emit

This is the only entry point. Everything else here is a module, and a build reached any other way is
a build whose flags nobody wrote down.

Every tree lives under one container, build/, named for what it is and stamped with when it was
made: build/build-20260831-181500. Two builds can then never share objects, and a tree can be kept
and compared against rather than overwritten. A tree is reused between invocations, because a fresh
one costs a full configure and a full compile every time; --fresh forces a new one, which is what a
changed target or a changed toolchain needs. The newest three per name are kept and older ones are
removed on the way in.

There are three host trees and each one is a different question, so each carries its own flags here
rather than in somebody's shell history:

  build         the library, as it ships
  build-oracle  MMGR_TEST_ORACLE on, so every suite that includes oracle_divergence.h calls libc
  build-cov     instrumented, always_inline off, link time optimisation off

The device benches are ESP-IDF projects, which is a second build system with its own toolchain and
environment. Reaching it needs a shell, so the shell script is written out from here, run, and
removed when it succeeds. A script kept in the tree is a second place the paths live and it drifts
from what this file computes; one generated from the paths this file just resolved cannot. A failing
script is left behind with its path printed, because a build that failed is exactly when the command
that ran is worth reading.

The last two matter. always_inline is honoured at -O0, so without turning it off every call site of
a header entry gets its own copy of that entry's branch records and the report counts optimiser
copies instead of source branches. Link time optimisation rewrites the code across translation
units before the counters are read, which measures something that is not what anyone wrote.

Two environment variables, because a first build has nothing to infer them from:

  MMGR_BUILD_ROOT   where the trees are made, ROOT by default. Windows caps a full object path at
                    250 characters and this tree's deepest object sits ~180 below its build dir, so
                    a checkout more than ~60 characters down cannot build in place at all.
  MMGR_CMAKE_ARGS   extra configure arguments, split like a shell would. The generator and the
                    compiler are otherwise read off a tree that already built, which answers
                    nothing in a fresh clone or worktree whose first build is this one.

`ab` runs the two sides one after the other, never at once. Two full builds at the same time is
what makes this machine unusable, and the comparison does not need them concurrent.

The mechanisms here are generic; the names, the paths and the two patterns that key on a per-project
idiom are this tree's.

A capability is a set of translation units the config selects, so a suite whose capabilities are off
is not built at all:

  cmake -S . -B build -DMMGR_ENABLE_DMA=OFF

The capabilities each suite needs are test/CMakeLists.txt's map, which is what `suites` reads: a
suite naming a capability it never drives, or driving one it does not name, is how a reduced build
fails while the full one passes.

A case Unity's generator does not collect is not an error to the generator: it is simply never
registered, so the suite passes while the case never ran. `cases` and `runners gen` both break that
silence by naming the near misses.

Some of src/ is generated rather than written, because it is tables nobody can check by eye - the
ASCII class bitmaps, the anchor cost profiles and the powers of five. Editing any of it by hand is
silently undone the next time anyone runs the generator, so `generated` compares what is on disk
against what the generators emit and says which is which. It covers whole modules, not just
headers: the ASCII module's .c and its CMakeLists.txt come out of the same generator as its header,
and a profile is a .c.
"""

import argparse
import csv
import datetime
import json
import os
import re
import shlex
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Where the build trees live. ROOT by default, because a tree beside the source is what anyone
# expects to find. MMGR_BUILD_ROOT moves them, which is not a convenience: Windows caps a full
# object path at 250 characters, and this tree's deepest object sits ~180 below its build dir, so a
# checkout more than ~60 characters down - a git worktree under .claude/worktrees/ is already past
# it - cannot build in place at all. The cap is the compiler's, so the only fix is a shorter prefix.
BUILD_ROOT = os.path.abspath(os.environ.get("MMGR_BUILD_ROOT", ROOT))

# Extra configure arguments, split like a shell would. borrowed_toolchain() reads the generator and
# the compiler off a tree that already built, which answers the question everywhere there is such a
# tree - and nowhere there is not, which is any fresh clone or worktree whose first build is this
# one. cmake's default generator is not always one that works on a given machine, so a first build
# with nothing to borrow from needs somewhere to be told.
CMAKE_ARGS = os.environ.get("MMGR_CMAKE_ARGS", "")


# Every build tree lives under one container and nowhere else. A tree written beside the source, or
# into whatever directory a command happened to run from, is how two builds come to share objects.
BUILD_CONTAINER = os.path.join(BUILD_ROOT, "build")

# Which directory each tree is currently using. Written here rather than inferred from the newest
# timestamp on disk, so a run that was interrupted does not silently hand the next one a half
# configured tree.
BUILD_POINTER = os.path.join(BUILD_CONTAINER, "current.json")

# Trees kept per name before the oldest is removed. Small on purpose: an instrumented tree is
# hundreds of megabytes, and what anyone wants is the last few rather than the last month.
BUILD_KEEP = 3


def build_stamp():
    """A directory-safe stamp for a new tree, to the second."""
    return datetime.datetime.now().strftime("%Y%m%d-%H%M%S")


def read_pointer():
    """Which directory each tree is currently using, as the pointer records it."""
    try:
        with open(BUILD_POINTER, encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return {}


def write_pointer(current):
    """Record which directory each tree is using."""
    os.makedirs(BUILD_CONTAINER, exist_ok=True)
    with open(BUILD_POINTER, "w", encoding="utf-8") as fh:
        json.dump(current, fh, indent=2, sort_keys=True)


# Where the last working toolchain is kept in the pointer. Underscored so the tree lookups can tell
# it from a tree name without a list of exceptions.
REMEMBERED_TOOLCHAIN = "_toolchain"

# What a cache has to say for a tree to be worth borrowing from, and the flag each answer becomes.
TOOLCHAIN_KEYS = {
    "CMAKE_GENERATOR": "-G",
    "CMAKE_C_COMPILER": "-DCMAKE_C_COMPILER=",
    "CMAKE_MAKE_PROGRAM": "-DCMAKE_MAKE_PROGRAM=",
}


def toolchain_from_cache(cache):
    """The generator and compiler flags a configured tree's cache records, or an empty list."""
    if not os.path.isfile(cache):
        return []
    out = []
    with open(cache, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            name, sep, value = line.partition(":")
            if not sep or name not in TOOLCHAIN_KEYS:
                continue
            value = value.partition("=")[2].strip()
            flag = TOOLCHAIN_KEYS[name]
            out += [flag + value] if flag.endswith("=") else [flag, value]
    return out


def remember_toolchain(path):
    """Record the toolchain a tree configured with, so it outlives that tree.

    Trees rotate. Without this, a machine that has built a hundred times arrives at the same place a
    fresh clone does the moment the last tree is removed, and the generator cmake picks by default is
    not one that works here.
    """
    args = toolchain_from_cache(os.path.join(path, "CMakeCache.txt"))
    if not args:
        return
    current = read_pointer()
    if current.get(REMEMBERED_TOOLCHAIN) != args:
        current[REMEMBERED_TOOLCHAIN] = args
        write_pointer(current)


def tree_dirs(tree):
    """Directories under the container belonging to @p tree, oldest first.

    Matched against the whole name and not its prefix. "build" is a prefix of "build-oracle", so a
    prefix test would let a rotation of one name remove another's trees. The stamp is part of the
    pattern for the same reason: anything under the container that is not a tree this made is left
    alone.
    """
    if not os.path.isdir(BUILD_CONTAINER):
        return []
    pattern = re.compile(r"^" + re.escape(tree) + r"-\d{8}-\d{6}$")
    return sorted(
        name
        for name in os.listdir(BUILD_CONTAINER)
        if pattern.match(name) and os.path.isdir(os.path.join(BUILD_CONTAINER, name))
    )


def rotate_trees(tree, keep=BUILD_KEEP):
    """Remove all but the newest @p keep directories belonging to @p tree.

    The name carries the stamp, so sorting the names sorts them by age without stat-ing anything.
    The one the pointer names is kept whatever its position, since a rotation that removes the tree
    a build is about to use has removed the wrong thing.
    """
    live = read_pointer().get(tree)
    mine = tree_dirs(tree)
    for name in mine[: max(0, len(mine) - keep)]:
        if name == live:
            continue
        shutil.rmtree(os.path.join(BUILD_CONTAINER, name), ignore_errors=True)


def tree_path(tree, fresh=False):
    """Absolute path of build tree @p tree, making a new one where @p fresh or none is current.

    A tree is reused between invocations, because a fresh one means a full configure and a full
    compile every time and the common command is run constantly. What fresh buys is the case where
    reuse would be wrong: a different target, a different toolchain, anything that leaves a cache
    describing a build nobody asked for.
    """
    current = read_pointer()
    name = current.get(tree)
    if not fresh and name and os.path.isdir(os.path.join(BUILD_CONTAINER, name)):
        return os.path.join(BUILD_CONTAINER, name)

    name = "%s-%s" % (tree, build_stamp())
    current[tree] = name
    write_pointer(current)
    rotate_trees(tree)
    return os.path.join(BUILD_CONTAINER, name)

# A five-way split. A suite is a directory holding exactly one .c
# with cases, and its generated runner sits beside it.
#
#   unit/<module>/test_<name>/     one per translation unit, mirroring src/<module>/
#   accuracy/test_<name>/          the value a conversion produces, against a reference that is
#                                  right by construction rather than by agreement with this library
#   environment/test_<env>/        one per entry in MMGR_ENVIRONMENTS, asserting that the widths
#                                  the build claims are the widths the code actually got
#   integration/test_<name>/       more than one module together
#   interop/test_<name>/           this library's output against another implementation's
#
# accuracy is separate from unit on purpose. A unit suite asks whether an entry keeps the contract
# its header states, which a table-driven module can satisfy with a table that is internally
# consistent and numerically wrong. An accuracy suite asks what the number actually is.
UNIT = os.path.join(ROOT, "test", "unit")
ACCURACY = os.path.join(ROOT, "test", "accuracy")
ENVIRONMENT = os.path.join(ROOT, "test", "environment")
INTEGRATION = os.path.join(ROOT, "test", "integration")
INTEROP = os.path.join(ROOT, "test", "interop")
GENERATED_RUNNER = "unity_runner.c"

# What Unity's generate_test_runner.rb collects, and the shape a case has to have to be collected.
UNITY_CASE = re.compile(r"^[ \t]*void[ \t]+(test_\w+)[ \t]*\([ \t]*(?:void)?[ \t]*\)", re.M)
NEAR_MISS = re.compile(r"^[ \t]*void[ \t]+(\w+)[ \t]*\([ \t]*(?:void)?[ \t]*\)[ \t]*\r?\n[ \t]*\{", re.M)
NOT_A_CASE = ("setUp", "tearDown", "main", "suiteSetUp", "suiteTearDown")

# test/CMakeLists.txt's map: which capabilities a suite needs before the build carries it.
SUITE_CAP = re.compile(r"^set\(MMGR_SUITE_CAP_(\w+)\s+([^)]*)\)", re.M)

# A conditional over a capability, and the two directives that end or invert one.
CAP_IF = re.compile(r"^[ \t]*#[ \t]*if[ \t]+MMGR_ENABLE_(\w+)[ \t]*$", re.M)
ANY_IF = re.compile(r"^[ \t]*#[ \t]*if")
ANY_ELSE = re.compile(r"^[ \t]*#[ \t]*el(se|if)")
ANY_ENDIF = re.compile(r"^[ \t]*#[ \t]*endif")


def guarded_cases(path):
    """Each registered case in @p path, with the capabilities whose #if it sits inside.

    Unity's generator reads the case names out of the source text and does not see a preprocessor
    conditional, so a case inside a capability's #if is still declared and called by the runner. With
    that capability off the definition is gone and the suite fails to LINK, which is what makes this
    the same finding as a capability the map does not name.
    """
    with open(path, encoding="utf-8") as fh:
        lines = fh.read().splitlines()
    out = {}
    stack = []  # one entry per open #if: the capability it tests, or None
    for line in lines:
        if ANY_ENDIF.match(line):
            if stack:
                stack.pop()
            continue
        if ANY_ELSE.match(line):
            if stack:
                stack[-1] = None  # the other arm is not the capability's
            continue
        if ANY_IF.match(line):
            m = CAP_IF.match(line)
            stack.append(m.group(1) if m else None)
            continue
        m = UNITY_CASE.match(line)
        if m:
            out[m.group(1)] = sorted({c for c in stack if c})
    return out


def suite_caps():
    """The capabilities each suite needs, as test/CMakeLists.txt's map states them."""
    path = os.path.join(ROOT, "test", "CMakeLists.txt")
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    return {name: caps.split() for name, caps in SUITE_CAP.findall(text)}


def runner_cases(path):
    """The cases Unity's generator will register in @p path, and the ones it will walk past."""
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    found = UNITY_CASE.findall(text)
    missed = [n for n in NEAR_MISS.findall(text) if n not in found and n not in NOT_A_CASE]
    return found, missed


def suite_source(suite_dir):
    """The one .c in a suite that holds its cases, or None."""
    if not os.path.isdir(suite_dir):
        return None
    for name in sorted(os.listdir(suite_dir)):
        path = os.path.join(suite_dir, name)
        if name.endswith(".c") and name != GENERATED_RUNNER and runner_cases(path)[0]:
            return path
    return None


def discover():
    """Every suite directory, which is any dir holding a .c with a collectable case."""
    out = []
    for base in (UNIT, ACCURACY, ENVIRONMENT, INTEGRATION, INTEROP):
        if not os.path.isdir(base):
            continue
        for dirpath, _dirnames, filenames in os.walk(base):
            if any(f.endswith(".c") and f != GENERATED_RUNNER for f in filenames):
                if suite_source(dirpath):
                    out.append(dirpath)
    return sorted(out)


def find_ruby():
    """Ruby runs Unity's generator. A missing one is an error, never a silent skip."""
    return shutil.which("ruby")


def generate_runner(suite_dir, unity_rb):
    """Emit suite_dir/unity_runner.c from the one source that holds the cases."""
    candidates = [f for f in sorted(os.listdir(suite_dir)) if f.endswith(".c") and f != GENERATED_RUNNER]
    sources = [f for f in candidates if runner_cases(os.path.join(suite_dir, f))[0]]
    if not sources:
        for f in candidates:
            _, missed = runner_cases(os.path.join(suite_dir, f))
            if missed:
                raise SystemExit(
                    "runners: %s holds no case Unity's generator will register.\n"
                    "  It collects file-scope `void test_<name>(void)` and nothing else, so these\n"
                    "  are walked past and never run: %s\n"
                    "  Rename each to test_<name> - a case the generator skips costs coverage in\n"
                    "  silence, because the suite still passes."
                    % (os.path.relpath(os.path.join(suite_dir, f), ROOT), ", ".join(missed))
                )
        raise SystemExit("runners: no test case found in %s" % os.path.relpath(suite_dir, ROOT))
    # The generator takes one input file and emits one main(), so cases spread across several
    # sources cannot be registered from any single one of them.
    if len(sources) > 1:
        raise SystemExit(
            "runners: %s holds test cases in %d sources (%s).\n"
            "  Unity's generator registers one source per runner, so the rest would never run.\n"
            "  Put the cases in one file, or give each file its own suite directory."
            % (os.path.relpath(suite_dir, ROOT), len(sources), ", ".join(sources))
        )
    ruby = find_ruby()
    if not ruby:
        raise SystemExit("runners: ruby not found on PATH - install it (choco install ruby)")
    if not os.path.isfile(unity_rb):
        raise SystemExit("runners: Unity's generate_test_runner.rb not found at %s" % unity_rb)
    src = os.path.join(suite_dir, sources[0])
    out = os.path.join(suite_dir, GENERATED_RUNNER)
    subprocess.run([ruby, unity_rb, src, out], check=True)
    # Unity's generator opens its output in text mode, so on Windows every line lands CRLF while
    # .gitattributes holds this tree at "LF in the repository and LF in the working copy". The runner
    # is tracked, so that difference shows as a modified file after every build that regenerates one.
    # git normalizes the content it stores either way, so nothing was ever committed wrong; rewriting
    # the bytes here is what leaves the working copy as the build found it, on every platform. A
    # no-op where the generator already wrote LF.
    with open(out, "rb") as fh:
        written = fh.read()
    lf = written.replace(b"\r\n", b"\n")
    if lf != written:
        with open(out, "wb") as fh:
            fh.write(lf)
    # Report the near misses even on success: the runner is written, and these still never ran.
    _, missed = runner_cases(src)
    if missed:
        print(
            "runners: %s registered %d cases, and walked past %s - rename each to test_<name>"
            % (os.path.relpath(src, ROOT), len(runner_cases(src)[0]), ", ".join(missed)),
            file=sys.stderr,
        )
    return out


# A suite that borrows a region and never drives it looks covered and is not.
#
# This tree's shape is the golden one: operands go into <Mod>V.<entry>_args and the entry is then
# called with the borrow. So the pattern matched is the assignment into an Args record, and "driven"
# means the borrow is named somewhere that is not its declaration, its assignment, or a clear.
BIND_DEP = re.compile(r"(\w+)V\.\w*_?args\.(\w+)\s*=\s*(\w+_work)\b")


def _is_setup_use(line, mem):
    """True when this line only declares, binds, or clears the borrow.

    Those three are how a dependency is made ready, not how it is exercised, and a suite that does
    only them has bound a unit it never asks anything of. Where the setup lives does not matter, so
    this keys on the line rather than on the enclosing function.
    """
    esc = re.escape(mem)
    if re.search(r"\b(static|uint8_t)\b.*\b" + esc + r"\b\s*\[", line):
        return True  # the declaration
    if re.search(r"\w+V\.\w*_?args\.\w+\s*=\s*" + esc + r"\b", line):
        return True  # the bind
    if re.search(r"\w+\.(clear|reset|zero)\s*\(\s*" + esc + r"\s*\)", line):
        return True  # made ready, not asked anything
    return False


def undriven_deps(path):
    """Dependencies a suite binds that no line exercises beyond declaring, binding and clearing."""
    with open(path, encoding="utf-8") as fh:
        lines = fh.read().split("\n")
    text = "\n".join(lines)
    out = []
    for _ns, dep, mem in set(BIND_DEP.findall(text)):
        driven = any(mem in ln and not _is_setup_use(ln, mem) for ln in lines)
        if not driven:
            out.append((dep, mem))
    return sorted(out)


# ------------------------------------------------------------------------------------------------
# Build trees
# ------------------------------------------------------------------------------------------------
# Each tree is a question, and the flags are the question. Written down here so that running one is
# a command rather than a remembered incantation.
TREES = {
    "build": {
        "what": "the library as it ships",
        "args": [],
    },
    "build-oracle": {
        "what": "every entry with a libc equivalent replaced by that equivalent",
        # Test side. The library has no oracle in it and no option to turn one on - the define
        # reaches test/support/oracle_divergence.h, which pulls in the substitution for the suites
        # that include it. Nothing in src is compiled differently.
        "args": ["-DCMAKE_C_FLAGS=-DMMGR_TEST_ORACLE=1"],
    },
    "build-cov": {
        "what": "instrumented, with always_inline and link time optimisation off",
        "args": [
            "-DMMGR_LTO=OFF",
            "-DCMAKE_C_FLAGS=--coverage -O0 -g -include " + os.path.join(ROOT, "test", "support", "coverage_inline.h"),
            "-DCMAKE_EXE_LINKER_FLAGS=--coverage",
        ],
    },
}


def run(cmd, quiet=True):
    """Run a command from the repository root and hand back its completed process."""
    if quiet:
        return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    return subprocess.run(cmd, cwd=ROOT, text=True)


def borrowed_toolchain(skip_path):
    """Generator and compiler taken from whichever tree is already configured.

    cmake's default generator is not always the one that works on a given machine, and a tree that
    already built is proof of one that does. Beats a second place to keep a toolchain path.

    @p skip_path is the directory being configured, and it is the only one passed over. Skipping the
    whole tree NAME instead is what this did when each name had one directory, and under stamped
    trees that reads as "never borrow from a tree of the kind you are building", which is the kind
    most likely to have one.
    """
    # Read off the directories rather than through tree_path. That function makes a directory and
    # rotates when a tree has none, and probing for a toolchain must not create anything.
    #
    # Every tree on disk is considered, newest first, not only the ones the pointer names. A tree
    # that configured successfully still proves a toolchain that works even after another build took
    # the name, and skipping it would send a machine that has built many times back to the answer a
    # fresh clone gets.
    # The remembered one first. It is written only after a configure succeeded, where a tree on disk
    # may be the wreck of one that did not.
    current = read_pointer()
    remembered = current.get(REMEMBERED_TOOLCHAIN, [])
    if remembered:
        return remembered

    for tree in TREES:
        for name in reversed(tree_dirs(tree)):
            path = os.path.join(BUILD_CONTAINER, name)
            if os.path.abspath(path) == os.path.abspath(skip_path):
                continue
            out = toolchain_from_cache(os.path.join(path, "CMakeCache.txt"))
            if out:
                return out
    return []


def configure(tree, fresh=False):
    """Configure @p tree if it is not there yet."""
    # Taken before the path, because tree_path writes the pointer when it makes a new tree and a
    # failed configure has to put back what was there rather than leaving the name unset.
    was = read_pointer().get(tree)

    path = tree_path(tree, fresh)
    if os.path.isfile(os.path.join(path, "CMakeCache.txt")):
        return 0
    cmd = ["cmake", "-S", ".", "-B", path, "-DCMAKE_BUILD_TYPE=Debug", "-DMMGR_BUILD_TESTS=ON"]
    cmd += TREES[tree]["args"] + borrowed_toolchain(tree) + shlex.split(CMAKE_ARGS)
    r = run(cmd)
    if r.returncode != 0:
        sys.stderr.write(r.stdout[-3000:] + r.stderr[-3000:])
        print("configure of %s failed" % tree)
        # A configure that failed still leaves a cache, and that cache holds whatever cmake settled
        # on before it stopped - a generator nobody asked for, or a compiler it could not find. The
        # next run sees a cache, treats the tree as configured, and builds against it. Removing it
        # is what makes a failed configure fail again rather than half-succeed.
        shutil.rmtree(path, ignore_errors=True)
        current = read_pointer()
        # Put back whatever the name pointed at before, so a failed --fresh leaves the working tree
        # in place instead of unsetting the name and stranding a tree that is still on disk.
        if was and os.path.isdir(os.path.join(BUILD_CONTAINER, was)):
            current[tree] = was
        else:
            current.pop(tree, None)
        write_pointer(current)
        return 1
    remember_toolchain(path)
    print("configured %s - %s" % (os.path.relpath(path, BUILD_ROOT), TREES[tree]["what"]))
    return 0


def build(tree, jobs, fresh=False):
    """Build @p tree, reporting only what went wrong."""
    if configure(tree, fresh) != 0:
        return 1
    r = run(["cmake", "--build", tree_path(tree), "-j", str(jobs)])
    if r.returncode != 0:
        sys.stdout.write(r.stdout[-4000:])
        sys.stderr.write(r.stderr[-4000:])
        print("build of %s failed" % tree)
        return 1
    print("%s built" % os.path.relpath(tree_path(tree), BUILD_ROOT))
    return 0


def ctest(tree, pattern):
    """Run @p tree's suites, and name the ones that failed."""
    cmd = ["ctest", "--test-dir", tree_path(tree), "--output-on-failure"]
    if pattern:
        cmd += ["-R", pattern]
    r = run(cmd)

    for line in r.stdout.splitlines():
        if "tests passed" in line or "tests failed" in line:
            print("  %s: %s" % (tree, line.strip()))
    if r.returncode != 0:
        for line in r.stdout.splitlines():
            if "(Failed)" in line or ":FAIL" in line:
                print("    " + line.strip())
    return r.returncode


# ------------------------------------------------------------------------------------------------
# Device benches
# ------------------------------------------------------------------------------------------------
# The on-device benches are ESP-IDF projects, which is a second build system with its own toolchain
# and its own environment. Reaching it needs a shell, and a shell script kept in the tree is a second
# place the paths live: it drifts from what this file computes and is wrong exactly when nobody
# checks. So the script is written out from here, run, and removed on success.
#
# Left behind on failure, with its path printed. A build that failed is when the exact command that
# ran is worth reading, and reconstructing it from a script that deleted itself is guesswork.
BENCH_ROOT = os.path.join(ROOT, "test", "performance_benching")

# Where an emitted script goes. Under the container with the build trees, so one directory holds
# everything a build makes and nothing lands beside the source.
SANDBOX = os.path.join(BUILD_CONTAINER, "sandbox")

# The IDF install this machine carries. Read from the environment where it is set, so a different
# install needs no edit here.
IDF_PATH = os.environ.get("IDF_PATH", r"C:\Espressif\frameworks\esp-idf-v5.5.5")
IDF_TOOLS_PATH = os.environ.get("IDF_TOOLS_PATH", r"C:\Espressif")
IDF_PYTHON = os.environ.get("MMGR_IDF_PYTHON", r"C:\Espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe")


def emit_script(name, body):
    """Write @p body to a script under the sandbox and hand back its path."""
    os.makedirs(SANDBOX, exist_ok=True)
    path = os.path.join(SANDBOX, "%s-%s.ps1" % (name, build_stamp()))
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(body)
    return path


def run_script(path, quiet=False):
    """Run an emitted script, removing it on success and leaving it on failure."""
    r = subprocess.run(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", path],
        cwd=ROOT,
        text=True,
        capture_output=quiet,
    )
    if r.returncode == 0:
        os.unlink(path)
    else:
        print("script kept for reading: %s" % os.path.relpath(path, ROOT).replace("\\", "/"))
    return r


# idf.py invoked by name goes through the Windows .py association on this machine, which prints
# nothing and exits 0 - so a failure reads as a success and the build directory is never made. Every
# invocation goes through the IDF python explicitly for that reason.
IDF_PREAMBLE = """\
$ErrorActionPreference = "Stop"
$env:IDF_PATH = "{idf_path}"
$env:IDF_TOOLS_PATH = "{idf_tools}"
$idfPython = "{idf_python}"

$pairs = & $idfPython "$env:IDF_PATH\\tools\\idf_tools.py" export --format key-value
foreach ($line in $pairs) {{
    if ($line -match '^\\s*([A-Za-z_][A-Za-z0-9_]*)=(.*)$') {{
        Set-Item -Path ("env:" + $matches[1]) -Value $matches[2].Replace('%PATH%', $env:PATH)
    }}
}}
$env:PATH = "$env:IDF_PATH\\tools;" + $env:PATH
$idfPy = "$env:IDF_PATH\\tools\\idf.py"
"""


def bench_names():
    """Every bench project under test/performance_benching that carries a CMakeLists."""
    if not os.path.isdir(BENCH_ROOT):
        return []
    return sorted(
        name
        for name in os.listdir(BENCH_ROOT)
        if os.path.isfile(os.path.join(BENCH_ROOT, name, "CMakeLists.txt"))
    )


def cmd_device_build(a):
    """Configure and build one device bench into its own tree under the container."""
    if a.bench not in bench_names():
        print("no bench called %s. There is: %s" % (a.bench, ", ".join(bench_names())))
        return 1

    proj = os.path.join(BENCH_ROOT, a.bench)
    # Always a new tree. An IDF tree carries the target it was configured for, so reusing one across
    # targets is the case that reports success for a part the image was not built for.
    tree = tree_path("%s-%s" % (a.bench, a.target), fresh=True)

    body = IDF_PREAMBLE.format(idf_path=IDF_PATH, idf_tools=IDF_TOOLS_PATH, idf_python=IDF_PYTHON)
    body += '\nSet-Location "{root}"\n'.format(root=ROOT)
    body += '& $idfPython $idfPy -C "{proj}" -B "{tree}" set-target {target}\n'.format(
        proj=proj, tree=tree, target=a.target
    )
    body += 'if ($LASTEXITCODE -ne 0) {{ throw "set-target failed" }}\n'
    # Job count capped: ninja defaults to cores + 2, and a full IDF tree at that width has taken this
    # machine down.
    body += 'ninja -C "{tree}" -j {jobs}\n'.format(tree=tree, jobs=a.jobs)
    body += 'if ($LASTEXITCODE -ne 0) {{ throw "build failed" }}\n'

    path = emit_script("device-build-%s-%s" % (a.bench, a.target), body)
    r = run_script(path)
    if r.returncode != 0:
        print("device build of %s for %s failed" % (a.bench, a.target))
        return 1
    print("built %s for %s in %s" % (a.bench, a.target, os.path.relpath(tree, ROOT).replace("\\", "/")))
    return 0


def cmd_device_flash(a):
    """Flash the image a previous device build produced."""
    tree = tree_path("%s-%s" % (a.bench, a.target))
    if not os.path.isfile(os.path.join(tree, "flash_args")):
        print("no built image for %s at %s - run: harness.py device build %s --target %s"
              % (a.bench, os.path.relpath(tree, ROOT).replace("\\", "/"), a.bench, a.target))
        return 1

    body = IDF_PREAMBLE.format(idf_path=IDF_PATH, idf_tools=IDF_TOOLS_PATH, idf_python=IDF_PYTHON)
    body += '\nSet-Location "{tree}"\n'.format(tree=tree)
    body += '& $idfPython -m esptool --chip {chip} --port {port} --baud {baud} write_flash "@flash_args"\n'.format(
        chip=a.target, port=a.port, baud=a.baud
    )
    body += 'if ($LASTEXITCODE -ne 0) {{ throw "flash failed" }}\n'

    path = emit_script("device-flash-%s-%s" % (a.bench, a.target), body)
    r = run_script(path)
    if r.returncode != 0:
        print("flash of %s to %s failed" % (a.bench, a.port))
        return 1
    print("flashed %s to %s on %s" % (a.bench, a.target, a.port))
    return 0


def cmd_bench(a):
    """Forward to the matrix module, which does not run on its own.

    bench.py owns bench_matrix.json and every mutation of it. Those commands are still worth
    reaching, so they are reached through here rather than being reimplemented in a second place.
    """
    sys.path.insert(0, BENCH_ROOT)
    try:
        import bench  # noqa: PLC0415  the module is only importable once BENCH_ROOT is on the path
    except ImportError as exc:
        print("cannot reach the matrix module: %s" % exc)
        return 1
    # bench.py resolves the matrix beside itself, so it is run from its own directory.
    here = os.getcwd()
    os.chdir(BENCH_ROOT)
    try:
        return bench.main(a.rest)
    finally:
        os.chdir(here)


def cmd_device_list(a):
    """Which bench projects exist, and which trees have been built for them."""
    del a
    names = bench_names()
    if not names:
        print("no bench projects under %s" % os.path.relpath(BENCH_ROOT, ROOT).replace("\\", "/"))
        return 0
    current = read_pointer()
    for name in names:
        built = sorted(key for key in current if key.startswith(name + "-"))
        print("  %-14s %s" % (name, ", ".join(built) if built else "not built"))
    return 0


def cmd_trees(a):
    """What is under the container, and which directory each name is currently using."""
    del a
    current = read_pointer()
    if not os.path.isdir(BUILD_CONTAINER):
        print("no build container at %s" % os.path.relpath(BUILD_CONTAINER, ROOT).replace("\\", "/"))
        return 0

    names = [name for tree in sorted(TREES) for name in tree_dirs(tree)]
    if not names:
        print("no build trees yet")
        return 0

    print("%s  (keeping %d per name)" % (os.path.relpath(BUILD_CONTAINER, ROOT).replace("\\", "/"), BUILD_KEEP))
    # Only the tree entries. The pointer also carries the remembered toolchain, whose value is a list
    # and would not go into a set at all.
    live = {name for key, name in current.items() if not key.startswith("_")}
    for name in names:
        configured = os.path.isfile(os.path.join(BUILD_CONTAINER, name, "CMakeCache.txt"))
        print("  %-34s %-12s %s" % (name, "current" if name in live else "", "" if configured else "not configured"))
    return 0


def cmd_build(a):
    return build(a.tree, a.jobs, a.fresh)


def cmd_test(a):
    if build(a.tree, a.jobs, a.fresh) != 0:
        return 1
    return 1 if ctest(a.tree, a.filter) != 0 else 0


def cmd_ab(a):
    """Both sides, one after the other.

    Concurrently would halve the wall clock and make the machine unusable while it ran, and the
    comparison does not need them at the same time.
    """
    bad = 0
    for tree in ("build", "build-oracle"):
        print("%s - %s" % (tree, TREES[tree]["what"]))
        if build(tree, a.jobs, a.fresh) != 0:
            return 1
        if ctest(tree, a.filter) != 0:
            bad = 1
    print("\nthe A/B agrees" if bad == 0 else "\nthe A/B does not agree")
    return bad


def cmd_coverage(a):
    """Build, run and report what of src/ the suites reached.

    The counters carry a stamp from the .gcno they were built beside, so last run's are cleared
    after the build and before the run: a report always describes the binaries that produced it.
    Every environment runs, not just host, because which arm of a width conditional is compiled at
    all is MMGR_ENVIRONMENTS' business and only the whole set describes the library.
    """
    tree = "build-cov"
    if not a.no_build and build(tree, a.jobs, a.fresh) != 0:
        return 1

    if not a.no_run:
        stale = []
        for base, _dirs, files in os.walk(tree_path(tree)):
            stale += [os.path.join(base, f) for f in files if f.endswith(".gcda")]
        for f in stale:
            os.unlink(f)
        print("cleared %d counter files" % len(stale))
        ctest(tree, None)

    out = os.path.join(tree_path(tree), "coverage.csv")
    js = os.path.join(tree_path(tree), "coverage.json")
    # No --exclude-unreachable-branches. It drops branches gcov attributes to a line gcovr believes
    # cannot be reached, which is the tool deciding what does not have to be covered - and it decides
    # it from the compiler's records, not from the source. A branch this report does not print is a
    # branch nobody looked at. Anything genuinely unreachable is established by reading it and
    # written down, not silently dropped from the denominator.
    r = run(
        [
            sys.executable, "-m", "gcovr", "--root", ".", "--filter", "src/",
            "--print-summary", "--txt-metric", "branch",
            "--csv", "-o", out, "--json", js, tree_path(tree),
        ]
    )
    if r.returncode != 0:
        sys.stderr.write(r.stderr[-4000:])
        print("gcovr failed")
        return 1

    for line in (r.stdout + r.stderr).splitlines():
        if line.startswith(("lines:", "functions:", "branches:")):
            print("  " + line)

    if a.worst:
        with open(out, encoding="utf-8") as fh:
            rows = list(csv.DictReader(fh))

        def gap(row):
            """How many uncovered lines and branches a file still holds."""
            return (int(row["line_total"]) - int(row["line_covered"])) + (
                int(row["branch_total"]) - int(row["branch_covered"])
            )

        print("\n  %-46s %13s %15s" % ("file", "lines", "branches"))
        for row in sorted(rows, key=gap, reverse=True)[: a.worst]:
            lt, lc = int(row["line_total"]), int(row["line_covered"])
            bt, bc = int(row["branch_total"]), int(row["branch_covered"])
            if lt == lc and bt == bc:
                continue
            print(
                "  %-46s %5d/%-5d%3d%% %5d/%-5d%3d%%"
                % (row["filename"], lc, lt, 100 * lc // max(lt, 1), bc, bt, 100 * bc // max(bt, 1))
            )

    if a.gaps:
        report_gaps(js)
    return 0


def line_runs(nums):
    """Collapse sorted line numbers into (first, last) runs.

    A function no case ever calls is a run of uncovered lines, so printed one per line it is forty
    entries that say one thing. Printed as a range it says the one thing.
    """
    out = []
    for n in nums:
        if out and n == out[-1][1] + 1:
            out[-1] = (out[-1][0], n)
        else:
            out.append((n, n))
    return out


def src_line(path, n):
    """Line @p n of @p path, for a report that names a gap and shows it."""
    try:
        with open(os.path.join(ROOT, path), encoding="utf-8", errors="replace") as fh:
            for i, line in enumerate(fh, 1):
                if i == n:
                    return line.strip()
    except OSError:
        pass
    return "<unavailable>"


def report_gaps(js):
    """Name every uncovered line and branch, with the source that is not being reached.

    gcovr's JSON carries one entry per line per translation unit that compiled it, and this library
    compiles every source once per environment - so the same physical line arrives five times. They
    are merged before anything is counted: a line is covered if any environment reached it, and a
    branch if any environment took it. Summing them instead counts an untested line once per
    environment and reports a gap five times its real size.

    Lines and branches fail in different places and both are printed. A function no case calls has
    no uncovered branch at all - it has no *covered* branch either, and a branch-only report calls
    it clean.
    """
    with open(js, encoding="utf-8") as fh:
        doc = json.load(fh)

    files = []
    for f in doc["files"]:
        lines, branches = {}, {}
        for ln in f["lines"]:
            if ln.get("gcovr/noncode"):
                continue
            n = ln["line_number"]
            lines[n] = lines.get(n, 0) + ln["count"]
            for i, b in enumerate(ln.get("branches", [])):
                branches[(n, i)] = branches.get((n, i), 0) + b["count"]
        ul = sorted(n for n, c in lines.items() if c == 0)
        ub = sorted({n for (n, _i), c in branches.items() if c == 0})
        if ul or ub:
            files.append((len(ul) + len(ub), f["file"].replace("\\", "/"), lines, branches, ul, ub))

    files.sort(reverse=True)
    for _gap, path, lines, branches, ul, ub in files:
        lc = sum(1 for c in lines.values() if c > 0)
        bc = sum(1 for c in branches.values() if c > 0)
        print(
            "\n=== %s  %d/%d lines, %d/%d branches"
            % (path, lc, len(lines), bc, len(branches))
        )
        for lo, hi in line_runs(ul):
            if lo == hi:
                print("  line   %s:%d | %s" % (path, lo, src_line(path, lo)))
            else:
                print("  lines  %s:%d-%d (%d) | %s" % (path, lo, hi, hi - lo + 1, src_line(path, lo)))
        for n in ub:
            taken = sum(1 for (ln, _i), c in branches.items() if ln == n and c > 0)
            total = sum(1 for (ln, _i) in branches if ln == n)
            print("  branch %s:%d %d/%d | %s" % (path, n, taken, total, src_line(path, n)))

    tl = sum(len(r[4]) for r in files)
    tb = sum(len(r[5]) for r in files)
    print("\n%d files with gaps: %d uncovered lines, %d uncovered branches" % (len(files), tl, tb))


def cmd_deps(a):
    bad = 0
    for d in discover():
        src = suite_source(d)
        if not src:
            continue
        un = undriven_deps(src)
        rel = os.path.relpath(d, ROOT).replace("\\", "/")
        if un:
            bad += len(un)
            print("%s" % rel)
            for dep, mem in un:
                print("   UNASSERTED  %-14s (%s)" % (dep, mem))
    if not bad:
        print("every bound dependency is asserted on by at least one case")
        return 0
    print(
        "\n%d dependencies are bound and then never named again outside setup.\n"
        "\n"
        "  This is a smell, not a proof. A unit the suite reaches only THROUGH the unit under\n"
        "  test is still exercised, and this cannot see that. What it does prove is that no case\n"
        "  ASSERTS anything about the dependency's own state, so a defect in the interaction\n"
        "  between the two is invisible either way.\n"
        "\n"
        "  Fix one of two ways: drive it and assert its state, or stop binding it so the suite\n"
        "  says what it tests." % bad
    )
    return 1 if a.strict else 0


def cmd_suites(a):
    caps = suite_caps()
    unnamed = []
    for d in discover():
        src = suite_source(d)
        found, missed = runner_cases(src)
        name = os.path.basename(d)
        need = caps.get(name, [])
        note = "" if not missed else "   NOT REGISTERED: " + ", ".join(missed)
        for case, guards in guarded_cases(src).items():
            for cap in guards:
                if cap not in need:
                    unnamed.append((name, cap, case))
        print(
            "%-52s %2d cases  %-18s%s"
            % (os.path.relpath(d, ROOT).replace("\\", "/"), len(found), " ".join(need) or "-", note)
        )

    if not unnamed:
        print("\nevery case a capability guards is in a suite that names that capability")
        return 0

    print("\n%d cases sit inside a capability their suite does not name:\n" % len(unnamed))
    for suite, cap, case in unnamed:
        print("   %-22s needs %-10s %s" % (suite, cap, case))
    print("""
  Unity's generator reads the case names out of the source text and does not see a
  preprocessor conditional, so the runner declares and calls each of these however the
  capability is set. With it off the definition is gone and the suite fails to LINK, so
  the reduced build breaks while the full one stays green.

  Fix one of two ways: add the capability to the suite's MMGR_SUITE_CAP_ line in
  test/CMakeLists.txt, which gates the whole suite on it, or move the cases to a suite
  that already names it.""")
    return 1 if a.strict else 0


def cmd_cases(a):
    for d in a.suite:
        src = suite_source(d)
        if not src:
            print("%s: no collectable case" % d)
            continue
        found, missed = runner_cases(src)
        print("%s" % os.path.relpath(src, ROOT).replace("\\", "/"))
        for name in found:
            print("   registered   %s" % name)
        for name in missed:
            print("   WALKED PAST  %s" % name)
    return 0


def cmd_runners_gen(a):
    for d in a.suite:
        out = generate_runner(d, a.unity)
        print("wrote %s" % os.path.relpath(out, ROOT).replace("\\", "/"))
    return 0


# ------------------------------------------------------------------------------------------------
# Generated headers
# ------------------------------------------------------------------------------------------------
# A generated header that someone edited by hand looks fine until the next regeneration throws the
# edit away. These pair each output with the tool that owns it, so the check is mechanical.
GENERATED = (
    (
        "tools/dev_env/gen_ascii_persona_bitorum.py",
        (
            "src/ascii_persona_bitorum/ascii_persona_bitorum.h",
            "src/ascii_persona_bitorum/ascii_persona_bitorum.c",
            "src/ascii_persona_bitorum/CMakeLists.txt",
        ),
    ),
    ("tools/dev_env/gen_pow5.py", ("src/pow5/pow5.h",)),
    (
        "tools/dev_env/gen_ancorae_formae.py",
        (
            "src/impensa_ancorae_acus/impensa_ancorae_acus_generic.c",
            "src/impensa_ancorae_acus/impensa_ancorae_acus_english.c",
            "src/impensa_ancorae_acus/impensa_ancorae_acus_uri.c",
            "src/impensa_ancorae_acus/impensa_ancorae_acus_inet.c",
            "src/impensa_ancorae_acus/impensa_ancorae_acus_route.c",
        ),
    ),
)


def cmd_generated(a):
    """Regenerate into a scratch copy and diff, so a check never writes."""
    stale, missing = [], []

    for tool, outs in GENERATED:
        tool_path = os.path.join(ROOT, tool)
        if not os.path.exists(tool_path):
            missing.append(tool)
            continue

        before = {}
        for rel in outs:
            p = os.path.join(ROOT, rel)
            before[rel] = open(p, "rb").read() if os.path.exists(p) else None

        r = subprocess.run([sys.executable, tool_path], capture_output=True, text=True)
        if r.returncode != 0:
            print("%s failed:" % tool)
            print(r.stderr.strip())
            return 1

        for rel in outs:
            p = os.path.join(ROOT, rel)
            after = open(p, "rb").read() if os.path.exists(p) else None
            if after is None:
                # The generator no longer emits something this table says it owns. Reporting that
                # as "created" is how a dropped output passes a green check: the file is absent
                # before and after, and nothing compares absent to absent.
                missing.append("%s (no output from %s)" % (rel, tool))
                print("  ABSENT   %s" % rel)
            elif before[rel] is None:
                print("  created  %s" % rel)
            elif before[rel] != after:
                stale.append(rel)
                if not a.write:
                    open(p, "wb").write(before[rel])  # a check does not write
                print("  STALE    %s" % rel)
            else:
                print("  ok       %s" % rel)

    for tool in missing:
        print("  MISSING  %s" % tool)

    if missing:
        print("\n%d generator(s) missing" % len(missing))
    if stale:
        verb = "regenerated" if a.write else "left as they were"
        print("\n%d header(s) differ from their generator, %s" % (len(stale), verb))
        print("fix with: python test/harness.py generated --write")
    if not stale and not missing:
        print("\nevery generated header matches its generator")

    if a.strict and (stale or missing):
        return 1
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    fresh_help = "make a new tree instead of reusing the current one"

    p = sub.add_parser("build", help="configure if needed, then build")
    p.add_argument("--tree", default="build", choices=sorted(TREES), help="which build tree")
    p.add_argument("--jobs", type=int, default=2, help="build parallelism, kept low on purpose")
    p.add_argument("--fresh", action="store_true", help=fresh_help)
    p.set_defaults(fn=cmd_build)

    p = sub.add_parser("test", help="build, then run the suites")
    p.add_argument("--tree", default="build", choices=sorted(TREES), help="which build tree")
    p.add_argument("--filter", help="only suites matching this regex")
    p.add_argument("--jobs", type=int, default=2, help="build parallelism, kept low on purpose")
    p.add_argument("--fresh", action="store_true", help=fresh_help)
    p.set_defaults(fn=cmd_test)

    p = sub.add_parser("ab", help="both sides of the A/B, one after the other")
    p.add_argument("--filter", help="only suites matching this regex")
    p.add_argument("--jobs", type=int, default=2, help="build parallelism, kept low on purpose")
    p.add_argument("--fresh", action="store_true", help=fresh_help)
    p.set_defaults(fn=cmd_ab)

    p = sub.add_parser("coverage", help="what of src/ the suites reached")
    p.add_argument("--worst", type=int, default=0, help="list the N thinnest files")
    p.add_argument("--gaps", action="store_true", help="name every uncovered line and branch")
    p.add_argument("--no-build", action="store_true", help="report on what is already built")
    p.add_argument("--no-run", action="store_true", help="report on the counters already there")
    p.add_argument("--jobs", type=int, default=2, help="build parallelism, kept low on purpose")
    p.add_argument("--fresh", action="store_true", help=fresh_help)
    p.set_defaults(fn=cmd_coverage)

    p = sub.add_parser("trees", help="which build trees exist, and which one each name is using")
    p.set_defaults(fn=cmd_trees)

    p = sub.add_parser("device", help="the on-device benches under test/performance_benching")
    dsub = p.add_subparsers(dest="sub", required=True)

    d = dsub.add_parser("list", help="which bench projects exist and what has been built")
    d.set_defaults(fn=cmd_device_list)

    d = dsub.add_parser("build", help="configure and build one bench for one part")
    d.add_argument("bench")
    d.add_argument("--target", required=True, help="esp32s3, esp32c6")
    d.add_argument("--jobs", type=int, default=2, help="build parallelism, kept low on purpose")
    d.set_defaults(fn=cmd_device_build)

    d = dsub.add_parser("flash", help="flash the image a build produced")
    d.add_argument("bench")
    d.add_argument("--target", required=True, help="esp32s3, esp32c6")
    d.add_argument("--port", required=True, help="serial port, COM3 and the like")
    d.add_argument("--baud", type=int, default=921600)
    d.set_defaults(fn=cmd_device_flash)

    p = sub.add_parser("bench", help="the bench matrix: list, add, update, deps, gen")
    p.add_argument("rest", nargs=argparse.REMAINDER, help="passed to the matrix module unchanged")
    p.set_defaults(fn=cmd_bench)

    p = sub.add_parser("suites", help="every suite, its cases, and the capabilities it needs")
    p.add_argument("--strict", action="store_true", help="exit non-zero on a finding, for a CI gate")
    p.set_defaults(fn=cmd_suites)

    p = sub.add_parser("deps", help="dependencies a suite binds but no case asserts on")
    p.add_argument("--strict", action="store_true", help="exit non-zero on a finding, for a CI gate")
    p.set_defaults(fn=cmd_deps)

    p = sub.add_parser("cases", help="what Unity will register, and what it will walk past")
    p.add_argument("suite", nargs="+")
    p.set_defaults(fn=cmd_cases)

    p = sub.add_parser("runners", help="Unity runner generation")
    psub = p.add_subparsers(dest="sub", required=True)
    g = psub.add_parser("gen", help="write a suite's unity_runner.c")
    g.add_argument("suite", nargs="+")
    g.add_argument("--unity", required=True, help="path to Unity's auto/generate_test_runner.rb")
    g.set_defaults(fn=cmd_runners_gen)

    p = sub.add_parser("generated", help="are the generated headers what their generators emit")
    p.add_argument("--write", action="store_true", help="keep the regenerated output instead of restoring")
    p.add_argument("--strict", action="store_true", help="exit non-zero on a finding, for a CI gate")
    p.set_defaults(fn=cmd_generated)

    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
