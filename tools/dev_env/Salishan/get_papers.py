#!/usr/bin/env python3
# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Fetch the ICSNL papers and turn them into the text the readers expect.
#
#   python tools/dev_env/Salishan/get_papers.py             the papers the readers need
#   python tools/dev_env/Salishan/get_papers.py --all       every paper in the archive
#   python tools/dev_env/Salishan/get_papers.py --convert   convert PDFs already on disk
#   python tools/dev_env/Salishan/get_papers.py --list      print what would be fetched
#
# Nothing else here works without build/papers/, and the instructions used to be "download the PDFs,
# convert each to text, and name the text file after the PDF". That is three chances to get it wrong
# before anything runs, and one of them is silent.
#
# THE ENCODING, WHICH IS THE ONE THAT MATTERS
#
# pdftotext writes Latin-1 unless told otherwise, and Latin-1 has no ʔ, no ə and no ɬ. A paper
# converted that way still opens, still looks like a paper, and has had the language taken out of
# it. Given -enc UTF-8 it keeps the characters and reorders the page: it lays text out by position,
# and a running header then prints before the title it sits above while a two-column table comes out
# interleaved.
#
# pypdf reads a page in the order the PDF stores it, which is the order the readers were written
# against. That was checked. Converting Mellesmoen_Kye_ICSNL61.pdf here gives 1462 lines identical,
# in order, to the copy the readers were built on, and all 146 PDFs on disk reproduce their held
# text exactly.
#
# WHERE THE ADDRESSES COME FROM
#
# One page lists every ICSNL paper ever published, 993 of them, each as a direct link. So there is
# no list of addresses to keep here and none to go stale: the page is read and the paper is found on
# it by name. The name is the PDF's own filename, which is also what build/papers/ calls it.

import argparse
import io
import os
import re
import sys
import time

ROOT = os.path.abspath(__file__)
while (ROOT != os.path.dirname(ROOT)) and not os.path.isdir(os.path.join(ROOT, "build")):
    ROOT = os.path.dirname(ROOT)
PAPERS = os.path.join(ROOT, "build", "papers")

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "corpus_script_extraction"))

INDEX = "https://lingpapers.sites.olt.ubc.ca/icsnl-volumes/"

# Identifies the tool, its purpose and who to complain to. A request carrying no user agent gets an
# interstitial HTML page back instead of the file. Naming the client is the courtesy every guide to
# harvesting an academic archive asks for. The site's robots.txt disallows only wp-admin, wp-login,
# the cache and trackbacks.
AGENT = ("Salishan-corpus-tools/1.0 (+https://github.com/dstroy0/MMgr; "
         "academic corpus extraction; dquigg123@gmail.com)")

# Seconds between requests. 993 files is a lot to ask of a university web server at once.
PAUSE = 1.0

LINK = re.compile(r'href="(https?://[^"]+\.pdf)"', re.IGNORECASE)


def wanted_stems():
    """The papers the readers need, taken from the coverage check so there is only one list."""
    from coverage_check import PAIRS
    return [one[0] for one in PAIRS]


def index_of(session):
    """Every paper in the archive, as its filename without extension against its address."""
    answer = session.get(INDEX, timeout=60)
    answer.raise_for_status()
    held = {}
    for address in LINK.findall(answer.text):
        stem = os.path.splitext(os.path.basename(address))[0]
        held.setdefault(stem, address)
    return held


def fetch(session, address, path):
    """One PDF onto disk, whole, or nothing.

    Written to a part file and moved into place, so an interrupted run leaves no half a paper for
    the next run to read as complete.
    """
    answer = session.get(address, timeout=120, stream=True)
    answer.raise_for_status()
    kind = answer.headers.get("content-type", "")
    if "pdf" not in kind.lower():
        raise ValueError("answered with %s, not a PDF" % (kind or "nothing"))
    part = path + ".part"
    with open(part, "wb") as handle:
        for block in answer.iter_content(65536):
            handle.write(block)
    if os.path.getsize(part) < 1024:
        os.remove(part)
        raise ValueError("answered with %d bytes" % os.path.getsize(part))
    os.replace(part, path)
    return os.path.getsize(path)


def converted(source, target):
    """One PDF as the text the readers read, with a marker between pages.

    The marker is what the coverage check reports positions against and what every reader uses to
    know where it is in the paper.
    """
    import pypdf
    reader = pypdf.PdfReader(source)
    # The blank line the held texts open with. Every reader counts lines from it, and a text that
    # starts one line higher moves every position the coverage check reports.
    out = [""]
    for number, page in enumerate(reader.pages, 1):
        out.append("===== page %d =====" % number)
        out.append(page.extract_text() or "")
    with open(target, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(out))
        handle.write("\n")
    return len(reader.pages)


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--all", action="store_true",
                        help="every paper in the archive, not only the ones with readers")
    parser.add_argument("--convert", action="store_true",
                        help="convert the PDFs already in build/papers and fetch nothing")
    parser.add_argument("--list", action="store_true",
                        help="print what would be fetched and stop")
    parser.add_argument("--stem", action="append", default=[],
                        help="one paper by its filename without the extension, repeatable")
    given = parser.parse_args()

    os.makedirs(PAPERS, exist_ok=True)

    try:
        import pypdf  # noqa: F401
    except ImportError:
        out.write("  pypdf is not installed, and it is what turns a PDF into the text the\n")
        out.write("  readers read. Install it with:  python -m pip install pypdf\n")
        out.flush()
        return 1

    if given.convert:
        done = 0
        for name in sorted(os.listdir(PAPERS)):
            if not name.lower().endswith(".pdf"):
                continue
            source = os.path.join(PAPERS, name)
            target = source[:-4] + ".txt"
            if os.path.isfile(target):
                continue
            pages = converted(source, target)
            done += 1
            out.write("  %-46s %d pages\n" % (name[:46], pages))
        out.write("\n  %d converted, %d already had text beside them\n"
                  % (done, len([one for one in os.listdir(PAPERS)
                                if one.lower().endswith(".pdf")]) - done))
        out.flush()
        return 0

    try:
        import requests
    except ImportError:
        out.write("  requests is not installed, and it is what fetches the papers. Install it\n")
        out.write("  with:  python -m pip install requests\n")
        out.flush()
        return 1

    session = requests.Session()
    session.headers["User-Agent"] = AGENT

    out.write("  reading the archive index\n")
    every = index_of(session)
    out.write("  %d papers listed at %s\n\n" % (len(every), INDEX))

    if given.stem:
        stems = given.stem
    elif given.all:
        stems = sorted(every)
    else:
        stems = wanted_stems()

    if given.list:
        for stem in stems:
            out.write("  %-46s %s\n" % (stem[:46], every.get(stem, "not in the index")))
        out.flush()
        return 0

    fetched = 0
    already = 0
    unlisted = []
    for stem in stems:
        target = os.path.join(PAPERS, "%s.txt" % stem)
        source = os.path.join(PAPERS, "%s.pdf" % stem)
        if os.path.isfile(target):
            already += 1
            continue
        if not os.path.isfile(source):
            address = every.get(stem)
            if not address:
                unlisted.append(stem)
                continue
            try:
                size = fetch(session, address, source)
            except Exception as trouble:
                out.write("  %-46s %s\n" % (stem[:46], trouble))
                continue
            out.write("  %-46s %d KB\n" % (stem[:46], size // 1024))
            time.sleep(PAUSE)
        pages = converted(source, target)
        fetched += 1
        out.write("  %-46s %d pages of text\n" % ("", pages))

    out.write("\n  %d papers now have text, %d already did\n" % (fetched, already))
    if unlisted:
        out.write("  %d not found in the index by that name:\n" % len(unlisted))
        for stem in unlisted:
            out.write("    %s\n" % stem)
    out.write("\n  build/papers/ holds %d texts\n"
              % len([one for one in os.listdir(PAPERS) if one.lower().endswith(".txt")]))
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
