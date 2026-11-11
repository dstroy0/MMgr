#!/usr/bin/env sh
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Builds each research paper under theory/papers/ with XeLaTeX, into build/theory/<paper>/.
#
#   sh maint/texbuild/build_theory.sh [<paper> ...]
#
# theory_tex.py writes the chapters of each paper from its Markdown first. Two passes set each
# paper, because the first pass writes the table of contents and the second reads it. A paper that
# builds clean has its PDF copied beside its main.tex, with the name of its directory. A paper that
# fails, or that drops a glyph, leaves that PDF as it was, and this script exits 1.
#
# The engine reports a glyph that no font has as "Missing character", and the run still succeeds.
# This script counts those lines and fails the paper where the count is not zero.

set -e

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
MIKTEX="/c/Users/Douglas/AppData/Local/Programs/MiKTeX/miktex/bin/x64"
if [ -d "$MIKTEX" ]; then
    PATH="$MIKTEX:$PATH"
    export PATH
fi

if [ $# -gt 0 ]; then
    PAPERS=$*
else
    PAPERS=$(for one in "$ROOT"/theory/papers/*/main.tex; do
        [ -f "$one" ] || continue
        one_dir=$(dirname "$one")
        echo "${one_dir#"$ROOT"/theory/}"
    done)
fi
STATUS=0

if ! python "$ROOT/maint/texbuild/theory_tex.py"; then
    echo "  theory_tex.py failed, and no chapter is written from its Markdown"
    exit 1
fi

for paper in $PAPERS; do
    src="$ROOT/theory/$paper"
    out="$ROOT/build/theory/$paper"
    if [ ! -f "$src/main.tex" ]; then
        echo "  no such research paper: $paper"
        STATUS=1
        continue
    fi
    mkdir -p "$out/chapters" "$out/frontmatter"
    if ! rm -f "$out/main.pdf"; then
        echo "  $paper: $out/main.pdf is open in another program"
        STATUS=1
        continue
    fi
    clean=1
    cd "$src"
    # The engine reads an aux file from the source directory before the output directory. A stale one
    # there hides the citations of this run.
    rm -f main.aux main.log main.fls main.fdb_latexmk main.toc main.out
    for pass in 1 2; do
        if ! xelatex -interaction=nonstopmode -file-line-error \
                -output-directory="$out" main.tex > "$out/pass$pass.log" 2>&1; then
            echo "  $paper: xelatex failed on pass $pass, see $out/pass$pass.log"
            grep -m 5 -E "^[^ ]+\.tex:[0-9]+:" "$out/main.log" 2>/dev/null || true
            STATUS=1
            clean=0
        fi
    done

    if [ ! -f "$out/main.pdf" ]; then
        echo "  $paper: no PDF"
        STATUS=1
        continue
    fi

    dropped=$(grep -c "^Missing character" "$out/main.log" 2>/dev/null | head -n 1)
    dropped=${dropped:-0}
    bytes=$(wc -c < "$out/main.pdf")
    printf "  %-28s %8s bytes, %s dropped glyphs\n" "$paper" "$bytes" "$dropped"
    if [ "$dropped" != "0" ]; then
        grep "^Missing character" "$out/main.log" | sed "s/^/      /" | sort -u | head -n 12
        STATUS=1
        clean=0
    fi
    if [ "$clean" = "1" ]; then
        cp "$out/main.pdf" "$src/$(basename "$src").pdf"
    fi
done

exit $STATUS
