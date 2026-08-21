#!/usr/bin/env python3
"""Build the coverage tree, run its suites and report what src/ they reached.

The .gcda counters carry a stamp from the .gcno they were built beside, so a rebuild that leaves
last run's counters in place makes gcov refuse the pair with a stamp mismatch. They are wiped
before the run rather than after it, so a report always describes the binaries that produced it.

    python tools/dev_env/coverage.py                 build, run, summarize
    python tools/dev_env/coverage.py --no-build      run and summarize what is already built
    python tools/dev_env/coverage.py --worst 12      list the twelve thinnest files
"""

import argparse
import csv
import io
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD = ROOT / "build-cov"


def run(cmd, **kw):
    """Run a command from the repository root and hand back its completed process."""
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, **kw)


def borrowed_toolchain():
    """Generator and compiler arguments taken from whichever build tree is already configured."""
    keys = {
        "CMAKE_GENERATOR": "-G",
        "CMAKE_C_COMPILER": "-DCMAKE_C_COMPILER=",
        "CMAKE_MAKE_PROGRAM": "-DCMAKE_MAKE_PROGRAM=",
    }
    for tree in ("build", "build-oracle"):
        cache = ROOT / tree / "CMakeCache.txt"
        if not cache.is_file():
            continue
        out = []
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            name, sep, value = line.partition(":")
            if not sep or name not in keys:
                continue
            value = value.partition("=")[2]
            out += [keys[name] + value] if keys[name].endswith("=") else [keys[name], value]
        if out:
            return out
    return []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-build", action="store_true", help="skip the build step")
    ap.add_argument("--no-run", action="store_true", help="skip the test step")
    ap.add_argument("--worst", type=int, default=0, help="list the N thinnest files")
    ap.add_argument("--reconfigure", action="store_true", help="rewrite the build tree's flags first")
    ap.add_argument("--jobs", type=int, default=2, help="build parallelism")
    args = ap.parse_args()

    if not BUILD.is_dir() or args.reconfigure:
        # The same flags SonarCloud.yml configures with, so a number measured here is the number
        # that job reports.
        forced = ROOT / "test" / "support" / "coverage_inline.h"
        cmd = [
            "cmake",
            "-S",
            ".",
            "-B",
            str(BUILD),
            "-DCMAKE_BUILD_TYPE=Debug",
            "-DMMGR_BUILD_TESTS=ON",
            # Coverage counters describe the code as written. Link time optimization rewrites
            # it across translation units before the counters are read, and on this toolchain
            # the pair also crashes the assembler outright.
            "-DMMGR_LTO=OFF",
            "-DCMAKE_C_FLAGS=--coverage -O0 -g -include " + str(forced),
            "-DCMAKE_EXE_LINKER_FLAGS=--coverage",
        ]
        # On a machine where cmake's default generator is not the one that works, the ordinary
        # build tree already knows which one does. Borrowing from it beats a second place to keep
        # a toolchain path up to date.
        cmd += borrowed_toolchain()
        r = run(cmd)
        if r.returncode != 0:
            sys.stderr.write(r.stdout[-3000:] + r.stderr[-3000:])
            sys.exit("configure failed")
        print("configured")

    if not args.no_build:
        # The build has to come first: wiping counters under binaries that are about to be
        # relinked would only leave a fresh mismatch behind.
        r = run(["cmake", "--build", str(BUILD), "-j", str(args.jobs)])
        if r.returncode != 0:
            sys.stdout.write(r.stdout[-4000:])
            sys.stderr.write(r.stderr[-4000:])
            sys.exit("build failed")
        print("build ok")

    if not args.no_run:
        gcda = list(BUILD.rglob("*.gcda"))
        for f in gcda:
            f.unlink()
        print(f"cleared {len(gcda)} counter files")

        # Every environment, not just host. MMGR_ENVIRONMENTS is what decides which arm of a
        # width conditional is compiled at all, so a line that is dead at 64 bits is live at 16
        # and only the whole set describes the library. checks is where the debug assertions live,
        # and idx16 is where the narrow index paths do.
        r = run(["ctest", "--test-dir", str(BUILD), "--output-on-failure"])
        tail = [ln for ln in r.stdout.splitlines() if "tests passed" in ln or "tests failed" in ln]
        print("\n".join(tail) if tail else r.stdout[-1500:])
        if r.returncode != 0:
            for ln in r.stdout.splitlines():
                if "(Failed)" in ln:
                    print("  " + ln.strip())

    # Two reports out of one read: the summary on the terminal and the per file numbers on disk.
    # gcovr writes one report to stdout and the rest where -o points, so the csv is named and the
    # txt one is left to fall out on the terminal.
    out = BUILD / "coverage.csv"
    r = run(
        [
            sys.executable,
            "-m",
            "gcovr",
            "--root",
            ".",
            "--filter",
            "src/",
            "--exclude-unreachable-branches",
            "--print-summary",
            "--txt-metric",
            "branch",
            "--csv",
            "-o",
            str(out),
            str(BUILD),
        ]
    )
    if r.returncode != 0:
        sys.stderr.write(r.stderr[-4000:])
        sys.exit("gcovr failed")

    print(
        "\n".join(
            ln for ln in (r.stdout + r.stderr).splitlines() if ln.startswith(("lines:", "functions:", "branches:"))
        )
    )

    rows = list(csv.DictReader(io.StringIO(out.read_text(encoding="utf-8"))))

    if args.worst:

        def gap(row):
            """Rank by how many uncovered lines and branches a file still holds."""
            miss_l = int(row["line_total"]) - int(row["line_covered"])
            miss_b = int(row["branch_total"]) - int(row["branch_covered"])
            return miss_l + miss_b

        print(f"\n  {'file':<46} {'lines':>13} {'branches':>15}")
        for row in sorted(rows, key=gap, reverse=True)[: args.worst]:
            lt, lc = int(row["line_total"]), int(row["line_covered"])
            bt, bc = int(row["branch_total"]), int(row["branch_covered"])
            if lt == lc and bt == bc:
                continue
            print(
                f"  {row['filename']:<46} {lc:>5}/{lt:<5}{100*lc//max(lt,1):>3}% "
                f"{bc:>5}/{bt:<5}{100*bc//max(bt,1):>3}%"
            )


if __name__ == "__main__":
    main()
