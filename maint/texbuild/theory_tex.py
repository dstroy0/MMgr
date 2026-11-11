"""Write the chapters of each research paper from the Markdown beside its main.tex.

    python maint/texbuild/theory_tex.py [--hold name.md,...]

A research paper is a directory under theory/papers/ that holds a main.tex. Its Markdown is the source. Each
Markdown file becomes chapters/chapter_<stem>.tex in its own research paper. Each research paper also gets
chapters/order.tex, its list of chapters in the order of the file names. A PDF in the directory of a research paper
comes in whole with \\includepdf.

The tool only reads the Markdown. Run it again after each edit to the Markdown. It writes every generated file whole,
and it leaves a file whose text did not change alone. It removes a generated chapter whose Markdown is gone. --hold
leaves the named files out of the research papers.
"""

from pathlib import Path
import argparse
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
THEORY = ROOT / "theory"
SHELVES = ("papers",)

SPECIALS = {
    "&": r"\&",
    "%": r"\%",
    "$": r"\$",
    "#": r"\#",
    "_": r"\_",
    "{": r"\{",
    "}": r"\}",
    "~": r"\textasciitilde{}",
    "^": r"\textasciicircum{}",
    "\\": r"\textbackslash{}",
}

# A table with a cell longer than this becomes records, one row at a time. A longtable breaks only
# between rows. A cell this long in a column a third of the text wide fills most of a page.
RECORD_CELL = 600

# The table sizes in order, each with its point size and its \tabcolsep. A column stays wide enough
# for its longest word, at half an em a character, plus \tabcolsep on each side. The text block is
# 430pt wide (preamble.tex). A table whose columns cannot all fit steps down a size. The two smallest
# sizes also close the gap between columns.
TABLE_SIZES = (
    (r"\small", 10, 6),
    (r"\footnotesize", 9, 6),
    (r"\scriptsize", 8, 4),
    (r"\tiny", 6, 3),
)
TEXT_WIDTH = 430

HEADINGS = {2: "section", 3: "subsection", 4: "subsubsection", 5: "paragraph", 6: "subparagraph"}
BULLET = re.compile(r"^( *)([-*+]|\d+[.)])\s+(.*)$")


def escape_text(value):
    return "".join(SPECIALS.get(character, character) for character in value)


def label(stem):
    return "chap:" + stem.replace("_", "-").lower()


class Chapter:
    def __init__(self, research_paper_dir, source, chapters):
        self.research_paper_dir = research_paper_dir.resolve()
        self.source = source
        self.chapters = chapters


def link_target(url, chapter):
    if re.match(r"https?://", url):
        target = url.replace("%", r"\%").replace("#", r"\#")
        return r"\href{" + target + "}{", "}"
    path = url.split("#", 1)[0]
    if path:
        target = (chapter.source.parent / path).resolve()
        if (
            target.parent == chapter.research_paper_dir
            and target.suffix == ".md"
            and target.stem in chapter.chapters
        ):
            return r"\hyperref[" + label(target.stem) + "]{", "}"
    # A link out of the research paper, to code or to a file withheld, keeps its words and loses its target.
    return "", ""


def inline(value, chapter):
    protected = []

    def protect(content):
        protected.append(content)
        return f"\x00{len(protected) - 1}\x00"

    def format_code(match):
        code = escape_text(match.group(2).strip())
        code = code.replace("/", "/\\allowbreak{}")
        code = code.replace("-", "-\\allowbreak{}")
        code = code.replace(".", ".\\allowbreak{}")
        code = code.replace("_", "_\\allowbreak{}")
        code = re.sub(r"(?<=[a-z])(?=[A-Z])", r"\\allowbreak{}", code)
        return protect(r"\texttt{" + code + "}")

    def format_math(match):
        return protect(r"\(" + match.group(1).replace(r"\lt", "<").replace(r"\gt", ">") + r"\)")

    def format_link(match):
        # [a, a](0) in a table is an interval instead of a link. A target names a file or a page.
        if not re.search(r"[A-Za-z]", match.group(2)):
            return match.group(0)
        # [Chang 1959](#src:Chang-1959) cites a registry key: its words stay, and the label is
        # cited after them. The generated bibliography numbers it.
        if match.group(2).startswith("#src:"):
            return match.group(1) + protect(r"~\cite{" + match.group(2)[1:] + "}")
        opening, closing = link_target(match.group(2), chapter)
        return protect(opening) + match.group(1) + protect(closing)

    value = re.sub(r"(`+)(.+?)\1", format_code, value)
    value = re.sub(r"(?<!\\)\$\$(.+?)(?<!\\)\$\$", format_math, value)
    value = re.sub(r"(?<!\\)\$(.+?)(?<!\\)\$", format_math, value)
    value = re.sub(
        r"\\([\\`*_{}\[\]()#+\-.!|<>~$])",
        lambda match: protect(escape_text(match.group(1))),
        value,
    )
    value = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", format_link, value)
    value = escape_text(value)
    # Emphasis opens only where a word does not run into it, and closes only where a word does not
    # run out of it. A star inside a symbol, such as v*_b, is not an emphasis mark.
    value = re.sub(r"(?<![\w*])\*\*(?=\S)(.+?)(?<=\S)\*\*(?![\w*])", r"\\textbf{\1}", value)
    value = re.sub(r"(?<![\w*])\*(?=[^\s*])(.+?)(?<=[^\s*])\*(?![\w*])", r"\\emph{\1}", value)
    # The word test is the Unicode one, so an underscore after a Greek letter is not an opening mark.
    value = re.sub(r"(?<!\w)\\_(\S(?:(?!\\_).)*?)\\_(?![\w\\])", r"\\emph{\1}", value)
    # A name with underscores in text can break after an underscore, as a name in code can.
    value = value.replace(r"\_", r"\_\allowbreak{}")
    # The Markdown writes straight double quotes. One that follows a word or closing punctuation
    # closes, and every other one opens.
    value = re.sub(r'(?<![\w.,;:!?)\]}\x00])"', "“", value)
    value = value.replace('"', "”")
    while "\x00" in value:
        value = re.sub(r"\x00(\d+)\x00", lambda match: protected[int(match.group(1))], value)
    return value


def heading_text(value, chapter):
    plain = escape_text(re.sub(r"[`*$\\{}]", "", value))
    return r"\texorpdfstring{" + inline(value, chapter) + "}{" + plain + "}"


def split_row(line):
    row = line.strip()
    if row.startswith("|"):
        row = row[1:]
    if row.endswith("|") and not row.endswith("\\|"):
        row = row[:-1]
    return [cell.strip() for cell in re.split(r"(?<!\\)\|", row)]


def is_separator(line):
    cells = split_row(line)
    return bool(cells) and all(re.fullmatch(r":?-+:?", cell) for cell in cells)


def table_rows(lines, index):
    rows = [split_row(lines[index])]
    index += 2
    while index < len(lines) and lines[index].strip().startswith("|"):
        rows.append(split_row(lines[index]))
        index += 1
    width = max(len(row) for row in rows)
    return [row + [""] * (width - len(row)) for row in rows], index


def records(rows, chapter):
    header, body = rows[0], rows[1:]
    output = []
    for row in body:
        output.append(r"\tablerecord{" + inline(row[0], chapter) + "}")
        output.append(r"\begin{description}")
        for name, cell in zip(header[1:], row[1:]):
            if cell:
                output.append(r"\item[{" + inline(name, chapter) + "}] " + inline(cell, chapter))
        output.extend([r"\end{description}", ""])
    return output


def longtable(rows, chapter):
    width = len(rows[0])
    step = 0 if width <= 3 else 1 if width <= 5 else 2
    weights, longest = [], []
    for column in range(width):
        cells = [row[column] for row in rows]
        weights.append(max(sum(len(cell) for cell in cells) / len(cells), 6) ** 0.6)
        words = [len(word) for cell in cells for word in cell.split()]
        longest.append(min(max(words, default=1), 18))
    while True:
        size, points, gap = TABLE_SIZES[step]
        floors = [(characters * points / 2 + 2 * gap) / TEXT_WIDTH for characters in longest]
        if sum(floors) <= 1 or step == len(TABLE_SIZES) - 1:
            break
        step += 1
    # Every column gets its floor, and what is left of the width is shared by how much each column
    # holds. Scaling the floors afterwards would take a column back under its longest word.
    slack = max(1 - sum(floors), 0)
    total = sum(weights)
    fractions = [floor + slack * weight / total for floor, weight in zip(floors, weights)]
    total = sum(fractions)
    fractions = [fraction / total for fraction in fractions]
    spec = "".join(
        rf">{{\RaggedRight\arraybackslash}}p{{\dimexpr{fraction:.4f}\linewidth-2\tabcolsep\relax}}"
        for fraction in fractions
    )
    output = [
        "{" + size,
        rf"\setlength{{\tabcolsep}}{{{gap}pt}}",
        rf"\begin{{longtable}}{{{spec}}}",
        r"\toprule",
    ]
    for number, row in enumerate(rows):
        output.append(" & ".join(inline(cell, chapter) for cell in row) + r" \\")
        if number == 0:
            output.extend([r"\midrule", r"\endhead"])
    output.extend([r"\bottomrule", r"\end{longtable}", "}", ""])
    return output


def indentation(line):
    return len(line) - len(line.lstrip(" "))


def starts_block(line):
    stripped = line.strip()
    return (
        stripped.startswith(("#", "|", "```", "~~~", ">", "$$"))
        or bool(BULLET.match(line))
        or bool(re.fullmatch(r"(?:-{3,}|\*{3,}|_{3,})", stripped))
    )


def parse_list(lines, index, chapter):
    first = BULLET.match(lines[index])
    indent = len(first.group(1))
    ordered = first.group(2)[0].isdigit()
    items = []
    while index < len(lines):
        match = BULLET.match(lines[index])
        if not match or len(match.group(1)) != indent or match.group(2)[0].isdigit() != ordered:
            break
        content = indent + len(match.group(2)) + 1
        body = [match.group(3)]
        index += 1
        while index < len(lines):
            line = lines[index]
            if not line.strip():
                ahead = index
                while ahead < len(lines) and not lines[ahead].strip():
                    ahead += 1
                if ahead < len(lines) and indentation(lines[ahead]) > indent:
                    body.extend([""] * (ahead - index))
                    index = ahead
                    continue
                break
            if indentation(line) > indent:
                body.append(line[min(indentation(line), content):])
            elif BULLET.match(line) or starts_block(line):
                break
            else:
                body.append(line.strip())
            index += 1
        items.append(body)
        # A blank line between two items at the same depth keeps them in one list.
        ahead = index
        while ahead < len(lines) and not lines[ahead].strip():
            ahead += 1
        following = BULLET.match(lines[ahead]) if ahead < len(lines) else None
        if (
            following
            and len(following.group(1)) == indent
            and following.group(2)[0].isdigit() == ordered
        ):
            index = ahead
    environment = "enumerate" if ordered else "itemize"
    output = [rf"\begin{{{environment}}}"]
    for body in items:
        converted = convert_blocks(body, chapter)
        while converted and not converted[-1]:
            converted.pop()
        output.append(r"\item " + (converted[0] if converted else ""))
        output.extend(converted[1:])
    output.extend([rf"\end{{{environment}}}", ""])
    return output, index


def convert_blocks(lines, chapter, title=None):
    output = []
    paragraph = []
    index = 0

    def flush():
        if paragraph:
            output.append(" ".join(inline(part, chapter) for part in paragraph))
            output.append("")
            paragraph.clear()

    while index < len(lines):
        line = lines[index]
        stripped = line.strip()
        fence = re.match(r"^\s*(`{3,}|~{3,})", line)
        if fence:
            flush()
            marker = fence.group(1)
            code = []
            index += 1
            while index < len(lines) and not lines[index].strip().startswith(marker):
                code.append(lines[index])
                index += 1
            index += 1
            output.extend([r"\begin{Verbatim}", *code, r"\end{Verbatim}", ""])
            continue
        if stripped == "$$":
            flush()
            equation = []
            index += 1
            while index < len(lines) and lines[index].strip() != "$$":
                equation.append(lines[index])
                index += 1
            index += 1
            output.extend([r"\[", *equation, r"\]", ""])
            continue
        display = re.fullmatch(r"\$\$(.+)\$\$", stripped)
        if display and "$$" not in display.group(1):
            flush()
            output.extend([r"\[", display.group(1).strip(), r"\]", ""])
            index += 1
            continue
        if not stripped:
            flush()
            index += 1
            continue
        if re.fullmatch(r"(?:-{3,}|\*{3,}|_{3,})", stripped):
            flush()
            output.extend([r"\medskip", ""])
            index += 1
            continue
        heading = re.match(r"^(#{1,6})\s+(.+?)\s*#*\s*$", stripped)
        if heading:
            flush()
            level = len(heading.group(1))
            if level == 1 and title is not None and not title:
                title.append(heading.group(2))
            else:
                command = HEADINGS.get(max(level, 2))
                output.extend([f"\\{command}{{{heading_text(heading.group(2), chapter)}}}", ""])
            index += 1
            continue
        if stripped.startswith("|") and index + 1 < len(lines) and is_separator(lines[index + 1]):
            flush()
            rows, index = table_rows(lines, index)
            if max(len(cell) for row in rows for cell in row) > RECORD_CELL:
                output.extend(records(rows, chapter))
            else:
                output.extend(longtable(rows, chapter))
            continue
        if BULLET.match(line):
            flush()
            block, index = parse_list(lines, index, chapter)
            output.extend(block)
            continue
        if stripped.startswith(">"):
            flush()
            quoted = []
            while index < len(lines) and lines[index].strip().startswith(">"):
                quoted.append(re.sub(r"^\s*>\s?", "", lines[index]))
                index += 1
            output.extend([r"\begin{quote}", *convert_blocks(quoted, chapter), r"\end{quote}", ""])
            continue
        # **Purpose:** and **Scope:** sit on consecutive lines and read as separate paragraphs.
        if paragraph and re.match(r"^\*\*[^*]+\*\*", stripped):
            flush()
        paragraph.append(stripped)
        index += 1
    flush()
    return output


def pretty(stem):
    return stem.replace("_", " ").capitalize()


def markdown_chapter(research_paper, source, chapters):
    chapter = Chapter(THEORY / research_paper, source, chapters)
    title = []
    # An HTML comment is for the readers and tools of the Markdown, and it does not reach the paper.
    text = re.sub(r"<!--.*?-->", "", source.read_text(encoding="utf-8"), flags=re.S)
    body = convert_blocks(text.splitlines(), chapter, title)
    heading = title[0] if title else pretty(source.stem)
    return "\n".join(
        [
            r"\chapter{" + heading_text(heading, chapter) + "}",
            r"\label{" + label(source.stem) + "}",
            "",
            *body,
        ]
    ).rstrip() + "\n"


def pdf_chapter(research_paper, source):
    return "\n".join(
        [
            r"\chapter{" + escape_text(pretty(source.stem)) + "}",
            r"\label{" + label(source.stem) + "}",
            r"\includepdf[pages=-,scale=0.9,pagecommand={\thispagestyle{plain}}]{" + source.name + "}",
        ]
    ) + "\n"


def research_papers():
    return [
        research_paper_dir.relative_to(THEORY).as_posix()
        for shelf in SHELVES
        if (THEORY / shelf).is_dir()
        for research_paper_dir in sorted((THEORY / shelf).iterdir())
        if (research_paper_dir / "main.tex").is_file()
    ]


def research_paper_sources(research_paper, held):
    research_paper_dir = THEORY / research_paper
    # <dirname>.pdf is the own build of the research paper, which build_theory.sh copies beside
    # main.tex. It is output and never a source. A paper that includes it includes itself.
    built = research_paper_dir.name + ".pdf"
    return [
        path
        for path in sorted(research_paper_dir.iterdir())
        if path.suffix in (".md", ".pdf") and path.name not in held and path.name != built
    ]


def write_if_changed(path, text):
    if path.exists() and path.read_text(encoding="utf-8") == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--hold", default="", help="comma separated file names to leave out")
    arguments = parser.parse_args()
    held = {name.strip() for name in arguments.hold.split(",") if name.strip()}
    for research_paper in research_papers():
        sources = research_paper_sources(research_paper, held)
        chapters = {source.stem for source in sources}
        chapter_dir = THEORY / research_paper / "chapters"
        written = []
        includes = []
        for source in sources:
            target = chapter_dir / f"chapter_{source.stem.lower()}.tex"
            if source.suffix == ".md":
                text = markdown_chapter(research_paper, source, chapters)
            else:
                text = pdf_chapter(research_paper, source)
            if write_if_changed(target, text):
                written.append(target.name)
            includes.append(r"\include{chapters/" + target.stem + "}")
        order = "\n".join(includes) + "\n"
        if write_if_changed(chapter_dir / "order.tex", order):
            written.append("order.tex")
        keep = {f"chapter_{source.stem.lower()}.tex" for source in sources} | {"order.tex"}
        for stale in chapter_dir.glob("*.tex"):
            # This tool writes every chapter of a research paper. One with no source is stale.
            if stale.name not in keep:
                stale.unlink()
                written.append(f"removed {stale.name}")
        print(f"  {research_paper}: {len(sources)} chapters, {len(written)} files changed")
        for name in written:
            print(f"      {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
