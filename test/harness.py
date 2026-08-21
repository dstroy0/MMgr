#!/usr/bin/env python3
# memmanager - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
"""MMgr test harness: suite discovery and Unity runner generation.

  harness.py build [--tree T]                 configure if needed, then build
  harness.py test [--tree T] [--filter RE]    build, then run the suites
  harness.py ab                               both sides of the A/B, one after the other
  harness.py coverage [--worst N]             build, run and report what src/ the suites reached
  harness.py suites                          every suite, its cases, and the capabilities it needs
  harness.py runners gen <dir> --unity <rb>  write <dir>/unity_runner.c
  harness.py cases <dir>                     what Unity will register, and what it will walk past
  harness.py generated                       are the generated headers what their generators emit

There are three build trees and each one is a different question, so each carries its own flags
here rather than in somebody's shell history:

  build         the library, as it ships
  build-oracle  MMGR_ORACLE_LIBC on, so every entry with a libc equivalent becomes that equivalent
  build-cov     instrumented, always_inline off, link time optimisation off

The last two matter. always_inline is honoured at -O0, so without turning it off every call site of
a header entry gets its own copy of that entry's branch records and the report counts optimiser
copies instead of source branches. Link time optimisation rewrites the code across translation
units before the counters are read, which measures something that is not what anyone wrote.

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

Two headers in src/ are generated rather than written, because they are tables nobody can check by
eye - the ASCII class bitmaps and the anchor cost profiles. Editing them by hand is silently undone
the next time anyone runs the generator, so `generated` compares what is on disk against what the
generators emit and says which is which.
"""

import argparse
import csv
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# A four-way split. A suite is a directory holding exactly one .c
# with cases, and its generated runner sits beside it.
#
#   unit/<module>/test_<name>/     one per translation unit, mirroring src/<module>/
#   environment/test_<env>/        one per entry in MMGR_ENVIRONMENTS, asserting that the widths
#                                  the build claims are the widths the code actually got
#   integration/test_<name>/       more than one module together
#   interop/test_<name>/           this library's output against another implementation's
UNIT = os.path.join(ROOT, "test", "unit")
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
    for base in (UNIT, ENVIRONMENT, INTEGRATION, INTEROP):
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
        "args": ["-DMMGR_ORACLE_LIBC=ON"],
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


def borrowed_toolchain(skip):
    """Generator and compiler taken from whichever tree is already configured.

    cmake's default generator is not always the one that works on a given machine, and a tree that
    already built is proof of one that does. Beats a second place to keep a toolchain path.
    """
    keys = {
        "CMAKE_GENERATOR": "-G",
        "CMAKE_C_COMPILER": "-DCMAKE_C_COMPILER=",
        "CMAKE_MAKE_PROGRAM": "-DCMAKE_MAKE_PROGRAM=",
    }
    for tree in TREES:
        if tree == skip:
            continue
        cache = os.path.join(ROOT, tree, "CMakeCache.txt")
        if not os.path.isfile(cache):
            continue
        out = []
        with open(cache, encoding="utf-8", errors="replace") as fh:
            for line in fh:
                name, sep, value = line.partition(":")
                if not sep or name not in keys:
                    continue
                value = value.partition("=")[2].strip()
                out += [keys[name] + value] if keys[name].endswith("=") else [keys[name], value]
        if out:
            return out
    return []


def configure(tree):
    """Configure @p tree if it is not there yet."""
    if os.path.isfile(os.path.join(ROOT, tree, "CMakeCache.txt")):
        return 0
    cmd = ["cmake", "-S", ".", "-B", tree, "-DCMAKE_BUILD_TYPE=Debug", "-DMMGR_BUILD_TESTS=ON"]
    cmd += TREES[tree]["args"] + borrowed_toolchain(tree)
    r = run(cmd)
    if r.returncode != 0:
        sys.stderr.write(r.stdout[-3000:] + r.stderr[-3000:])
        print("configure of %s failed" % tree)
        return 1
    print("configured %s - %s" % (tree, TREES[tree]["what"]))
    return 0


def build(tree, jobs):
    """Build @p tree, reporting only what went wrong."""
    if configure(tree) != 0:
        return 1
    r = run(["cmake", "--build", tree, "-j", str(jobs)])
    if r.returncode != 0:
        sys.stdout.write(r.stdout[-4000:])
        sys.stderr.write(r.stderr[-4000:])
        print("build of %s failed" % tree)
        return 1
    print("%s built" % tree)
    return 0


def ctest(tree, pattern):
    """Run @p tree's suites, and name the ones that failed."""
    cmd = ["ctest", "--test-dir", tree, "--output-on-failure"]
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


def cmd_build(a):
    return build(a.tree, a.jobs)


def cmd_test(a):
    if build(a.tree, a.jobs) != 0:
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
        if build(tree, a.jobs) != 0:
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
    if not a.no_build and build(tree, a.jobs) != 0:
        return 1

    if not a.no_run:
        stale = []
        for base, _dirs, files in os.walk(os.path.join(ROOT, tree)):
            stale += [os.path.join(base, f) for f in files if f.endswith(".gcda")]
        for f in stale:
            os.unlink(f)
        print("cleared %d counter files" % len(stale))
        ctest(tree, None)

    out = os.path.join(ROOT, tree, "coverage.csv")
    r = run(
        [
            sys.executable, "-m", "gcovr", "--root", ".", "--filter", "src/",
            "--exclude-unreachable-branches", "--print-summary", "--txt-metric", "branch",
            "--csv", "-o", out, tree,
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
    return 0


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
    ("tools/dev_env/gen_ascii_persona_bitorum.py", ("src/ascii_persona_bitorum/ascii_persona_bitorum.h",)),
    ("tools/dev_env/gen_pow5.py", ("src/pow5/pow5.h",)),
    (
        "tools/dev_env/gen_ancorae_formae.py",
        (
            "src/impensa_ancorae_acus/impensa_ancorae_acus_generic.h",
            "src/impensa_ancorae_acus/impensa_ancorae_acus_english.h",
            "src/impensa_ancorae_acus/impensa_ancorae_acus_uri.h",
            "src/impensa_ancorae_acus/impensa_ancorae_acus_inet.h",
            "src/impensa_ancorae_acus/impensa_ancorae_acus_route.h",
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
            if before[rel] is None:
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

    p = sub.add_parser("build", help="configure if needed, then build")
    p.add_argument("--tree", default="build", choices=sorted(TREES), help="which build tree")
    p.add_argument("--jobs", type=int, default=2, help="build parallelism, kept low on purpose")
    p.set_defaults(fn=cmd_build)

    p = sub.add_parser("test", help="build, then run the suites")
    p.add_argument("--tree", default="build", choices=sorted(TREES), help="which build tree")
    p.add_argument("--filter", help="only suites matching this regex")
    p.add_argument("--jobs", type=int, default=2, help="build parallelism, kept low on purpose")
    p.set_defaults(fn=cmd_test)

    p = sub.add_parser("ab", help="both sides of the A/B, one after the other")
    p.add_argument("--filter", help="only suites matching this regex")
    p.add_argument("--jobs", type=int, default=2, help="build parallelism, kept low on purpose")
    p.set_defaults(fn=cmd_ab)

    p = sub.add_parser("coverage", help="what of src/ the suites reached")
    p.add_argument("--worst", type=int, default=0, help="list the N thinnest files")
    p.add_argument("--no-build", action="store_true", help="report on what is already built")
    p.add_argument("--no-run", action="store_true", help="report on the counters already there")
    p.add_argument("--jobs", type=int, default=2, help="build parallelism, kept low on purpose")
    p.set_defaults(fn=cmd_coverage)

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
