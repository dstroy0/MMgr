# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py nolibc: src/ built by each host toolchain, and held to no symbol of a C library.

The check holds src/ alone. test/ may call the C library as an oracle or to debug, and the checks environment
brings test/support/mmgr_host_traps.h into src/, so the check leaves that environment out.

For each toolchain in TOOLCHAINS, each environment of MMGR_ENVIRONMENTS but checks, each build of the toolchain's
row, and each variant in VARIANTS, three stages run:

  ir      the functions that the IR calls. clang and clang-cl read LLVM IR, gcc reads its optimized GIMPLE, and msvc
          has no IR that a tool can read. The IR holds no call of the C library. A call to a function that src/ does
          not define fails the stage, and so does a call to a name of LIBC_NAMES, whoever defines it. An intrinsic
          or a builtin that a code generator lowers to a call of the C library fails it too.
  object  every undefined symbol of every object, from llvm-nm, which reads ELF and COFF. A symbol that no object of
          src/ defines and that the toolchain's allowlist does not name fails the stage.
  link    every object linked into one image with no default library, one image for each forma of
          impensa_ancorae_acus. A stub defines each host and marker row of the allowlist. The link fails where a
          symbol stays undefined, and the linker names it.
  cycle   the call graph of the objects, from their relocations, tables of entries included. src/config/ defines
          memset, memcpy, memmove and memcmp, and the code of each must not reach its own name again. Such a call
          links, since src/ defines the name, and then calls itself until the stack runs out.

An LTO build moves code generation to the link, and the code generator there can lower a copy across two sources
to a call of the C library. So an LTO build reads the IR and the object that the link-time code generator gives:
the bitcode before code generation and the object after it for lld-link, the ltrans GIMPLE and the ltrans object for
gcc. msvc writes no form that a tool can read, and its link alone judges it. The link of an LTO build exports each
external symbol that src/ defines, so the link keeps every function and judges all of them.

An allowlist is test/verify/nolibc/<toolchain>.tsv: a symbol, its kind and the reason, one row each. A host row is a
function that the program defines, as embed_turnstile.h says. A marker row is data that the program defines. A
runtime row is a helper of the compiler's own runtime library, which the row of the toolchain links. A linker row is
a symbol that the linker itself defines. No row may name a function of the C library.

To add a toolchain, add one row to TOOLCHAINS and one allowlist beside the others. docs/quality/no-libc.md shows
each field of a row.

Everything lands in one run under build/, nolibc-<stamp>/<toolchain>/<environment>/<build>/<variant>/. The file
findings.tsv there holds each symbol that a stage found, the file that names it, and the verdict.
"""

import concurrent.futures
import glob
import os
import re
import shutil
import subprocess

import verify

# The variants of src/ that a build can choose. off is the default build. on turns on the external memory and the DMA
# modules, with the settings that their fuzz harnesses build with.
VARIANTS = {
    "off": ["MMGR_ENABLE_DMA=0", "MMGR_ENABLE_EXTRAM=0"],
    "on": ["MMGR_ENABLE_EXTRAM=1"] + verify.SPECIAL["memoriam_praetereo"][1],
}

# The module whose build chooses one source of several, its formas. The off variant compiles each forma, and links
# one image for each. Every other variant links the default forma alone.
FORMA_MODULE = "impensa_ancorae_acus"
FORMA_DEFAULT = "generic"


class Build:
    """One way a toolchain builds src/.

    label    the name of the build, and the name of its folder in the run.
    compile  the flags of each compile: the optimization level, and the LTO flag of an LTO build.
    lto      None for a build with no LTO. For an LTO build, the flags that the link adds.
    """

    def __init__(self, label, compile_flags, lto=None):
        self.label = label
        self.compile = compile_flags
        self.lto = lto


class Toolchain:
    """One compiler on this host: how it compiles, how it emits IR, and how it links an image with no C library.

    name      the row's name, and the name of its allowlist file.
    cc        the program to call.
    style     gnu for the flags of gcc and clang, msvc for the flags of cl and clang-cl.
    ir        llvm to read LLVM IR, gimple to read gcc's optimized GIMPLE, None for no IR stage.
    builds    the builds of the run, each a Build.
    flags     the flags that the toolchain needs at every compile.
    linker    the argv that links a DLL with no default library and no entry, before the objects and the output.
    runtime   the libraries of the compiler's own runtime, after the objects, which the runtime rows need.
    """

    def __init__(self, name, cc, style, ir, builds, flags, linker, runtime):
        self.name = name
        self.cc = cc
        self.style = style
        self.ir = ir
        self.builds = builds
        self.flags = flags
        self.linker = linker
        self.runtime = runtime
        self.env = None

    def found(self, program):
        """The path of a program, on the PATH of the toolchain's environment where it has one."""
        return shutil.which(program, path=(self.env or {}).get("PATH") or (self.env or {}).get("Path"))

    def present(self):
        return self.found(self.cc) is not None and self.found(self.linker[0]) is not None

    def object_suffix(self):
        return ".obj" if self.style == "msvc" else ".o"

    def compile_argv(self, root, source, out, flags, defines, extra=()):
        includes = [os.path.join(root, part) for part in ("src", "include", "deps/embedded_types/include")]
        cc = self.found(self.cc)
        if self.style == "msvc":
            cmd = [cc, "/nologo", "/c", "/std:c11"] + flags + self.flags
            cmd += ["/I" + path for path in includes] + ["/D" + define for define in defines]
            return cmd + list(extra) + ["/Fo" + out, source]
        cmd = [cc, "-c", "-std=c11"] + flags + self.flags
        cmd += ["-I" + path for path in includes] + ["-D" + define for define in defines]
        return cmd + list(extra) + ["-o", out, source]

    def ir_argv(self, root, source, out, flags, defines):
        if self.ir == "llvm" and self.style == "msvc":
            return self.compile_argv(root, source, out, flags, defines, ["/clang:-S", "/clang:-emit-llvm"])
        if self.ir == "llvm":
            return self.compile_argv(root, source, out, flags, defines, ["-S", "-emit-llvm"])
        return self.compile_argv(root, source, out + ".o", flags, defines, ["-fdump-tree-optimized=" + out])

    def lld_style(self):
        return os.path.basename(self.linker[0]).lower().startswith(("lld-link", "link"))

    def link_argv(self, objects, out, extra=(), exports=None):
        linker = [self.found(self.linker[0])] + self.linker[1:]
        if self.lld_style():
            defs = ["/DEF:" + exports] if exports else []
            return linker + list(extra) + defs + objects + ["/OUT:" + out] + self.runtime
        return linker + list(extra) + ["-o", out] + objects + ([exports] if exports else []) + self.runtime


# The four host toolchains. A new toolchain is one row here and one allowlist in test/verify/nolibc/. The flags of a
# row are the flags that mmgr_flags gives src/ for that toolchain: /GS- for the frontend of cl.
TOOLCHAINS = [
    # clang on Windows targets the MSVC ABI, and lld-link links its objects. /OPT:NOREF keeps every section, so the
    # link sees each symbol that each object names. /lldsavetemps keeps the bitcode and the object of the LTO
    Toolchain("clang", "clang", "gnu", "llvm",
              [Build("O0", ["-O0"]), Build("O2", ["-O2"]), Build("Os", ["-Os"]),
               Build("O2-lto", ["-O2", "-flto"], ["/lldsavetemps"]),
               Build("Os-lto", ["-Os", "-flto"], ["/lldsavetemps"])],
              [], ["lld-link", "/NOLOGO", "/DLL", "/NOENTRY", "/NODEFAULTLIB", "/OPT:NOREF"], []),
    # gcc links with no start files and no C library. --no-undefined makes the shared link name each symbol left.
    # One LTO partition gives one ltrans object and one ltrans dump to read
    Toolchain("gcc", "gcc", "gnu", "gimple",
              [Build("O0", ["-O0"]), Build("O2", ["-O2"]), Build("Os", ["-Os"]),
               Build("O2-lto", ["-O2", "-flto"], ["-O2", "-flto", "-flto-partition=one", "-save-temps"]),
               Build("Os-lto", ["-Os", "-flto"], ["-Os", "-flto", "-flto-partition=one", "-save-temps"])],
              [], ["gcc", "-shared", "-nostdlib", "-Wl,--no-undefined"], ["-lgcc"]),
    Toolchain("clang-cl", "clang-cl", "msvc", "llvm",
              [Build("Od", ["/Od"]), Build("O2", ["/O2"]), Build("O1", ["/O1"]),
               Build("O2-lto", ["/O2", "-flto"], ["/lldsavetemps"]),
               Build("O1-lto", ["/O1", "-flto"], ["/lldsavetemps"])],
              ["/clang:-std=c11", "/GS-"], ["lld-link", "/NOLOGO", "/DLL", "/NOENTRY", "/NODEFAULTLIB", "/OPT:NOREF"],
              []),
    # /GL defers code generation to the link, and /LTCG runs it there
    Toolchain("msvc", "cl", "msvc", None,
              [Build("Od", ["/Od"]), Build("O2", ["/O2"]), Build("O1", ["/O1"]),
               Build("O2-lto", ["/O2", "/GL"], ["/LTCG"]),
               Build("O1-lto", ["/O1", "/GL"], ["/LTCG"])],
              ["/experimental:c11atomics", "/GS-"],
              ["link", "/NOLOGO", "/DLL", "/NOENTRY", "/NODEFAULTLIB", "/OPT:NOREF"], []),
]

# The LLVM intrinsics and the gcc builtins that a code generator can lower to a call of the C library, by the name
# of that call. The IR holds no call of the C library, and so each one fails the ir stage.
LOWERED = {
    "memcpy": "memcpy", "memmove": "memmove", "memset": "memset", "memcmp": "memcmp", "bcmp": "bcmp",
    "floor": "floor", "ceil": "ceil", "trunc": "trunc", "round": "round", "fmod": "fmod", "pow": "pow",
    "exp": "exp", "log": "log", "sqrt": "sqrt",
}


# The names of the C library that a call in the IR must not reach, whoever defines them. src/config/ defines the
# first four, for the calls that a code generator writes, and the IR still holds none.
LIBC_NAMES = frozenset(set(LOWERED.values()) | {"memset", "memcpy", "memmove", "memcmp", "memchr", "strlen"})


def environments(root):
    """The environments of MMGR_ENVIRONMENTS in the top CMakeLists.txt, as names and defines, without checks."""
    with open(os.path.join(root, "CMakeLists.txt"), encoding="utf-8") as handle:
        text = handle.read()
    block = re.search(r"set\(MMGR_ENVIRONMENTS\s*(.*?)\)", text, re.S)
    found = []
    for entry in re.findall(r'"([^"]*)"', block.group(1) if block else ""):
        name, _, defines = entry.partition("|")
        if name != "checks":
            found.append((name, [define for define in defines.split(",") if define]))
    return found


def allowlist(root, chain):
    """The rows of the toolchain's allowlist, by symbol: its kind and its reason."""
    path = os.path.join(root, "test", "verify", "nolibc", chain.name + ".tsv")
    rows = {}
    if not os.path.isfile(path):
        return rows
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("#") or not line.strip() or line.startswith("symbol\t"):
                continue
            # A tab or a run of two spaces or more parts the cells, so a row aligned by hand reads as one aligned by tabs
            cells = re.split(r"\t+| {2,}", line.rstrip("\n"), maxsplit=2)
            if len(cells) >= 3 and cells[1] in ("host", "marker", "runtime", "linker"):
                rows[cells[0]] = (cells[1], cells[2])
    return rows


def sources(root):
    """Every source of src/, and the forma sources of FORMA_MODULE apart, by forma.

    src/config/ holds the string shim, which defines memset, memcpy, memmove and memcmp. A program with no C library
    links it, and so every image here links it.
    """
    common = []
    formas = {}
    for module in sorted(os.listdir(os.path.join(root, "src"))):
        folder = os.path.join(root, "src", module)
        if not os.path.isdir(folder):
            continue
        for entry in sorted(os.listdir(folder)):
            if not entry.endswith(".c"):
                continue
            forma = re.match(re.escape(FORMA_MODULE) + r"_(\w+)\.c$", entry)
            if module == FORMA_MODULE and forma:
                formas[forma.group(1)] = os.path.join(folder, entry)
            else:
                common.append(os.path.join(folder, entry))
    return common, formas


def module_levels(root):
    """The OPTIMIZE flags that each module's CMakeLists.txt names, by the folder of the module.

    mmgr_add_module puts them after the build's own flags, and so the last level on the line is theirs. The run
    puts them in the same place, and so each module builds here at the level the real build gives it.
    """
    levels = {}
    for module in sorted(os.listdir(os.path.join(root, "src"))):
        path = os.path.join(root, "src", module, "CMakeLists.txt")
        if not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8") as handle:
            text = "\n".join(line for line in handle.read().splitlines() if not line.lstrip().startswith("#"))
        if "HEADER_ONLY" in text:
            continue
        named = re.search(r'\bOPTIMIZE\s+("([^"]*)"|(\S+?))\s*[\n)]', text)
        # A module that names no level takes -O2, as mmgr_add_module gives it
        levels[module] = [flag for flag in (named.group(2) or named.group(3)).split(";") if flag] if named else ["-O2"]
    return levels


def flags_for(chain, flags):
    """The flags of a module for one toolchain, each $<$<C_COMPILER_ID:ID>:flag> kept where the toolchain is ID.

    CMake names gcc GNU, and clang and clang-cl Clang, and cl MSVC.
    """
    compiler_id = {"gcc": "GNU", "clang": "Clang", "clang-cl": "Clang", "msvc": "MSVC"}.get(chain.name, "")
    kept = []
    for flag in flags:
        gated = re.match(r"^\$<\$<C_COMPILER_ID:([^>]+)>:(.*)>$", flag)
        if gated is None:
            kept.append(flag)
        elif compiler_id in gated.group(1).split(","):
            kept.append(gated.group(2))
    return kept


def symbols(obj, which):
    """The external symbols of an object that llvm-nm lists, undefined or defined, each with its type letter.

    A weak symbol has a default in the object, and so it counts as defined: llvm-nm marks it w or v. A name that an
    /alternatename directive of the object points at a default counts as defined too, which is the weak default of
    the MSVC linker, but only where the object defines that default. The C runtime writes directives of its own, and
    their defaults are the C runtime's. Such a name is an alias, marked A, and no image exports it.
    """
    r = verify.run_tool(["llvm-nm", "--extern-only", obj])
    found = {}
    for line in r.stdout.splitlines():
        cells = line.split()
        if len(cells) < 2:
            continue
        letter = cells[-2]
        weak = letter in ("w", "v", "W", "V")
        undefined = letter == "U" and not weak
        if (which == "undefined") == undefined:
            found[cells[-1]] = letter
    if which == "defined" and obj.endswith(".obj"):
        directives = verify.run_tool(["llvm-readobj", "--coff-directives", obj])
        for name, default in re.findall(r"/alternatename:([^=\s]+)=(\S+)", directives.stdout, re.I):
            if default in found:
                found[name] = "A"
    return found


def is_identifier(symbol):
    """Whether a symbol is a C identifier, and so a name that the image can export."""
    return re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", symbol) is not None


# The mem intrinsics. One with a constant length lowers to stores in line or to a call, as the level and the target
# decide, and the object stage reads which. One with a length that the IR computes lowers to a call every time.
MEM_INTRINSICS = ("memset", "memcpy", "memmove")


def llvm_calls(path):
    """The functions that LLVM IR calls and does not define, and the intrinsics that lower to a call, each with the
    functions of the IR that hold the call.

    A mem intrinsic counts where its length is a value the IR computes. Every other intrinsic of LOWERED counts at
    each call of it.
    """
    with open(path, encoding="utf-8", errors="replace") as handle:
        lines = handle.read().splitlines()
    defined = {match.group(1).strip('"') for match in
               (re.match(r'^define\b[^@]*@("[^"]+"|[\w.$]+)\(', line) for line in lines) if match}
    calls, lowered = {}, {}
    function = ""
    for line in lines:
        head = re.match(r'^define\b[^@]*@("[^"]+"|[\w.$]+)\(', line)
        if head:
            function = head.group(1).strip('"')
            continue
        site = re.search(r'\b(?:call|invoke)\b[^@]*@("[^"]+"|[\w.$]+)\(', line)
        if site is None:
            continue
        name = site.group(1).strip('"')
        if not name.startswith("llvm."):
            if name not in defined:
                calls.setdefault(name, set()).add(function)
            continue
        stem = name.split(".")[1]
        mem = re.search(r"i64 (%[\w.]+), i1 \w+\)", line)
        if stem in MEM_INTRINSICS and mem:
            lowered.setdefault(stem, set()).add(function)
        elif stem in LOWERED and stem not in MEM_INTRINSICS:
            lowered.setdefault(LOWERED[stem], set()).add(function)
    return calls, lowered


def gimple_calls(path):
    """The functions that a GIMPLE dump calls and does not define, and the builtins that lower to a call, each with
    the functions of the dump that hold the call.

    A mem builtin counts where its last argument, the length, is not a constant, as llvm_calls counts the intrinsics.
    """
    defined, called, lowered = set(), {}, {}
    function = ""
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            head = re.match(r"^;; Function (\S+) ", line)
            if head:
                function = head.group(1)
                defined.add(function)
                continue
            # gcc marks a call in tail position with [tail call] after its semicolon
            call = re.match(r"^\s+(?:[^=;]+ = )?([A-Za-z_][\w.]*) \(.*\);(?:\s*\[[^\]]*\])*\s*$", line)
            if not call:
                continue
            name = call.group(1)
            if name.startswith("__builtin_"):
                stem = name[len("__builtin_"):]
                constant = re.search(r",\s*\d+\);(?:\s*\[[^\]]*\])*\s*$", line) is not None
                if stem in LOWERED and not (stem in MEM_INTRINSICS and constant):
                    lowered.setdefault(LOWERED[stem], set()).add(function)
            elif re.search(r"_\d+$", name) is None and not name.startswith("__atomic_"):
                # An SSA name ends in an underscore and a number, and a call through it is a call through a pointer.
                # The object stage judges what such a call reaches
                called.setdefault(name, set()).add(function)
    return {name: holders for name, holders in called.items() if name not in defined}, lowered


# The sources that an LTO build compiles with no LTO, as src/config/CMakeLists.txt builds them. The code generator
# writes its calls to the names that they define after LTO chose what to keep.
NO_LTO_SOURCES = ("mmgr_string_shim.c",)
LTO_FLAGS = ("-flto", "/GL")

# The names that src/ defines for a program with no C library. The code of each must not reach the same name again,
# or a call to it calls itself.
SHIM_NAMES = ("memset", "memcpy", "memmove", "memcmp")


def call_graph(objects):
    """Each labeled symbol of the objects, function or table, and the symbols that its relocations name.

    llvm-objdump -D labels the symbols of every section, the tables of the data as well as the functions. A table of
    entries is then a node, and its edges are the entries, so a call through a dispatch table is an edge as well.
    """
    graph = {}
    for obj in objects:
        text = verify.run_tool(["llvm-objdump", "-D", "-r", "--no-show-raw-insn", obj]).stdout
        node = None
        for line in text.splitlines():
            head = re.match(r"^[0-9a-f]+ <([^>]+)>:", line)
            if head:
                node = head.group(1)
                graph.setdefault(node, set())
                continue
            reloc = re.search(r"\s(?:R_\S+|IMAGE_REL_\S+)\s+(\S+?)(?:[-+]0x[0-9a-f]+)?$", line)
            if reloc and node is not None:
                graph[node].add(reloc.group(1))
    return graph


def cycles(graph):
    """The chains by which a definition of SHIM_NAMES reaches its own name again, one chain each at most."""
    defined = {}
    for name in SHIM_NAMES:
        for node in (name, name + "_default"):
            if node in graph:
                defined[name] = node
    found = []
    for name, start in sorted(defined.items()):
        seen = set()
        stack = [(start, [start])]
        while stack:
            node, chain = stack.pop()
            for target in sorted(graph.get(node, ())):
                body = defined.get(target, target)
                if body == start:
                    found.append(" -> ".join(chain + [target]))
                    stack = []
                    break
                if body not in seen:
                    seen.add(body)
                    stack.append((body, chain + [target]))
    return found


def stub_source(rows):
    """The C source of the link's stub: each host row a function, each marker row an int."""
    lines = []
    for symbol, (kind, _reason) in sorted(rows.items()):
        if kind == "host":
            lines.append("void %s(void) {}" % symbol)
        elif kind == "marker":
            lines.append("int %s = 0;" % symbol)
    return "\n".join(lines) + "\n"


def exports_file(path, defined):
    """A module definition file that exports each external symbol, data marked as data."""
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("EXPORTS\n")
        for symbol, letter in sorted(defined.items()):
            # A constant of the COFF pool and the alias of a weak default are names of no C identifier. A name with a
            # double underscore first is the compiler's, as the default that cl emits for __isa_available, and src/
            # defines no such name
            if not is_identifier(symbol) or symbol.startswith("__") or letter in ("w", "v", "W", "V", "A"):
                continue
            handle.write("    %s%s\n" % (symbol, "" if letter.upper() == "T" else " DATA"))


def unresolved(text):
    """The symbols that a linker's output names as undefined."""
    found = set()
    for pattern in (r"undefined symbol: (\S+)", r"unresolved external symbol (\S+)", r"undefined reference to `([^']+)'"):
        found.update(re.findall(pattern, text))
    return found


class Config:
    """The stages of one toolchain, environment, build and variant, and what they found."""

    def __init__(self, root, folder, chain, build, defines, common, formas, variant, rows):
        self.root = root
        self.folder = folder
        self.chain = chain
        self.build = build
        self.defines = defines
        self.common = common
        self.formas = formas
        self.formas_here = sorted(formas) if variant == "off" else ([FORMA_DEFAULT] if formas else [])
        self.rows = rows
        self.levels = module_levels(root)
        self.failures = []
        self.findings = []
        self.defined = {}

    def judged(self, symbol):
        # A name of the C library is the C library's, even where the string shim defines it: src/ makes no such call
        if symbol in LIBC_NAMES:
            return "libc"
        if symbol in self.defined:
            return "own"
        if symbol in self.rows:
            return "allowed " + self.rows[symbol][0]
        return "libc"

    def found(self, stage, symbol, where, holders=()):
        """One finding: the stage, the symbol, the file that names it, the verdict, and the functions that hold it."""
        verdict = self.judged(symbol)
        functions = ",".join(sorted(holders)[:6])
        self.findings.append((stage, symbol, where, verdict, functions))
        if verdict == "libc":
            self.failures.append("%s %s in %s%s" % (stage, symbol, where, (" (%s)" % functions) if functions else ""))

    def tool(self, argv, folder):
        """One tool run in folder. A tool that writes beside its working directory writes into the run alone."""
        return subprocess.run(argv, capture_output=True, text=True, env=self.chain.env, cwd=folder)

    def compiled(self, folder, flags, with_ir):
        """Each source compiled into folder with flags. Returns each source with its object and its IR path."""
        os.makedirs(folder, exist_ok=True)
        results = []
        for source in self.common + [self.formas[forma] for forma in self.formas_here]:
            stem = os.path.splitext(os.path.basename(source))[0]
            obj = os.path.join(folder, stem + self.chain.object_suffix())
            own = [flag for flag in flags if flag not in LTO_FLAGS] \
                if os.path.basename(source) in NO_LTO_SOURCES else flags
            own = own + flags_for(self.chain, self.levels.get(os.path.basename(os.path.dirname(source)), []))
            r = self.tool(self.chain.compile_argv(self.root, source, obj, own, self.defines), folder)
            if r.returncode != 0:
                self.failures.append("compile %s: %s" % (os.path.relpath(source, self.root),
                                                         " | ".join((r.stdout + r.stderr).strip().splitlines()[:3])))
            ir_path = None
            if with_ir and self.chain.ir is not None:
                ir_path = os.path.join(folder, stem + (".ll" if self.chain.ir == "llvm" else ".gimple"))
                self.tool(self.chain.ir_argv(self.root, source, ir_path, flags, self.defines), folder)
            results.append((source, obj, ir_path))
        return results

    def ir_found(self, symbol, where, holders):
        """A call in the IR. A name of the C library fails, even where src/ defines it, as the string shim does."""
        self.found("ir", symbol, where, holders)

    def read_ir(self, path, obj=None):
        if not os.path.isfile(path):
            # gcc writes no dump for a source with no function, as a table alone or a module that its switch leaves
            # out. Such an object defines no code, and the object stage reads it all the same
            no_code = obj is not None and path.endswith(".gimple") and \
                not any(letter.upper() == "T" for letter in symbols(obj, "defined").values())
            if not no_code:
                self.failures.append("ir %s: no IR written" % os.path.basename(path))
            return
        calls, lowered = llvm_calls(path) if path.endswith(".ll") else gimple_calls(path)
        for symbol, holders in sorted(calls.items()):
            self.ir_found(symbol, os.path.basename(path), holders)
        # An intrinsic or a builtin that the code generator lowers to a call of the C library is that call in the IR
        for symbol, holders in sorted(lowered.items()):
            functions = ",".join(sorted(holders)[:6])
            self.findings.append(("ir", symbol, os.path.basename(path), "libc", functions))
            self.failures.append("ir intrinsic of %s in %s (%s)" % (symbol, os.path.basename(path), functions))

    def read_cycles(self, graph):
        """The cycle stage: no definition of SHIM_NAMES reaches its own name again through what it calls."""
        for chain in cycles(graph):
            self.findings.append(("cycle", chain.split(" -> ")[0], chain, "libc", chain.split(" -> ")[0]))
            self.failures.append("cycle %s" % chain)

    def read_objects(self, objects, graph):
        """The object stage: each undefined symbol, with the functions whose relocations name it."""
        undefined = {}
        for obj in objects:
            for symbol in symbols(obj, "undefined"):
                undefined.setdefault(symbol, []).append(os.path.basename(obj))
        holders = {}
        for node, targets in graph.items():
            for target in targets:
                holders.setdefault(target, set()).add(node)
        for symbol, where in sorted(undefined.items()):
            self.found("object", symbol, ",".join(sorted(where)[:4]), holders.get(symbol, ()))

    def read_code(self, objects):
        """The object stage and the cycle stage over one call graph of the objects."""
        graph = call_graph(objects)
        self.read_objects(objects, graph)
        self.read_cycles(graph)

    def stub(self):
        path = os.path.join(self.folder, "nolibc_stub.c")
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(stub_source(self.rows))
        obj = os.path.join(self.folder, "nolibc_stub" + self.chain.object_suffix())
        self.tool(self.chain.compile_argv(self.root, path, obj, [], []), self.folder)
        return obj

    def image_held(self, image, exports):
        """Whether the image reads back as an image, and exports each name of its definition file where it has one.

        A link that drops every function leaves nothing to judge, and so an image that exports less than its file
        names fails, as one that does not read back does.
        """
        headers = verify.run_tool(["llvm-readobj", "--file-headers", image])
        if headers.returncode != 0 or "ImageFileHeader" not in headers.stdout:
            self.failures.append("link %s: the image does not read back as an image" % os.path.basename(image))
            return
        if exports is None:
            return
        with open(exports, encoding="utf-8") as handle:
            named = {line.split()[0] for line in handle.read().splitlines()[1:] if line.strip()}
        listed = verify.run_tool(["llvm-readobj", "--coff-exports", image])
        given = set(re.findall(r"^\s*Name: (\S+)", listed.stdout, re.M))
        missing = sorted(named - given)
        if missing:
            self.failures.append("link %s: the image exports %d of %d names, %s left out" % (
                os.path.basename(image), len(named & given), len(named), ", ".join(missing[:4])))

    def linked(self, results, extra, exports):
        """One image for each forma, or one image where the config has no forma. Returns each image's path."""
        stub = self.stub()
        shared = [obj for source, obj, _ir in results if source in self.common]
        images = []
        for forma in self.formas_here or [None]:
            forma_obj = [obj for source, obj, _ir in results if forma is not None and source == self.formas[forma]]
            image = os.path.join(self.folder, "mmgr_%s.dll" % (forma or "image"))
            argv = self.chain.link_argv(shared + forma_obj + [stub], image, extra, exports)
            r = self.tool(argv, self.folder)
            for symbol in sorted(unresolved(r.stdout + r.stderr)):
                self.found("link", symbol, os.path.basename(image))
            if r.returncode != 0:
                # A link that fails fails the stage, whatever it names: a failed link leaves no image to judge
                self.failures.append("link %s: exit %d, %s" % (os.path.basename(image), r.returncode,
                                                               " | ".join((r.stdout + r.stderr).strip().splitlines()[:3])))
            else:
                self.image_held(image, exports)
            images.append(image)
        return images

    def run(self):
        os.makedirs(self.folder, exist_ok=True)
        if self.build.lto is None:
            results = self.compiled(self.folder, self.build.compile, True)
            if self.failures:
                return self
            for _source, obj, _ir in results:
                self.defined.update(symbols(obj, "defined"))
            self.read_code([obj for _source, obj, _ir in results])
            for _source, obj, ir_path in results:
                if ir_path is not None:
                    self.read_ir(ir_path, obj)
            self.linked(results, [], None)
        else:
            self.run_lto()
        with open(os.path.join(self.folder, "findings.tsv"), "w", encoding="utf-8") as table:
            table.write("stage\tsymbol\twhere\tverdict\tfunctions\n")
            for row in self.findings:
                table.write("\t".join(row) + "\n")
        return self

    def run_lto(self):
        plain_flags = [flag for flag in self.build.compile if flag not in LTO_FLAGS]
        plain = self.compiled(os.path.join(self.folder, "plain"), plain_flags, False)
        results = self.compiled(self.folder, self.build.compile, False)
        if self.failures:
            return
        for _source, obj, _ir in plain:
            self.defined.update(symbols(obj, "defined"))
        exports = os.path.join(self.folder, "exports.def")
        exports_file(exports, self.defined)
        extra = list(self.build.lto)
        if self.chain.ir == "gimple":
            extra += ["-fdump-tree-optimized", "-dumpdir", self.folder + os.sep]
        before = set(glob.glob(os.path.join(self.folder, "*")))
        self.linked(results, extra, exports)
        made = sorted(set(glob.glob(os.path.join(self.folder, "*"))) - before)
        if self.chain.ir == "llvm":
            for bitcode in [path for path in made if path.endswith("precodegen.bc")]:
                text = bitcode[:-len(".bc")] + ".ll"
                # clang reads bitcode, and with the passes off it writes the module as it is, as llvm-dis does
                verify.run_tool(["clang", "-S", "-emit-llvm", "-O0", "-Xclang", "-disable-llvm-passes", "-Wno-override-module",
                                 bitcode, "-o", text])
                self.read_ir(text)
            self.read_code([path for path in made if path.endswith((".lto.obj", ".lto.o"))])
            if not any(path.endswith("precodegen.bc") for path in made):
                self.failures.append("ir: the link wrote no bitcode before code generation")
        elif self.chain.ir == "gimple":
            for dump in [path for path in made if path.endswith(".optimized") and "ltrans" in path]:
                os.replace(dump, dump + ".gimple")
                self.read_ir(dump + ".gimple")
            self.read_code([path for path in made if path.endswith(".ltrans.o")])
            if not any("ltrans" in path for path in made):
                self.failures.append("ir: the link wrote no ltrans file")


# The known answers, in test/verify/nolibc/. kat_clean.c holds no symbol of a C library, and kat_libc.c calls
# KAT_LIBC_NAME, which no compiler writes in line.
KAT_CLEAN = "kat_clean.c"
KAT_LIBC = "kat_libc.c"
KAT_LIBC_NAME = "strtol"


def kat_wrong(chain, build, clean, libc):
    """How the two known answers of one toolchain and one build came out wrong. An empty list where both came right.

    kat_clean.c must pass every stage. The stages must name the call of kat_libc.c: the link in every build, and in
    a build with no LTO the object stage and, where the toolchain gives IR, the ir stage. An LTO build writes no
    object or IR of the LTO where its link fails, and so its link alone judges it.
    """
    wrong = []
    if clean.failures:
        wrong.append("%s: %s" % (KAT_CLEAN, "; ".join(clean.failures[:2])))
    named = {row[0] for row in libc.findings if row[1] == KAT_LIBC_NAME and row[3] == "libc"}
    expected = {"link"} if build.lto is not None else ({"object", "link"} | ({"ir"} if chain.ir else set()))
    missing = sorted(expected - named)
    if missing:
        wrong.append("%s: no %s stage names its %s" % (KAT_LIBC, ", ".join(missing), KAT_LIBC_NAME))
    return wrong


def summary(run_root):
    """The symbols of the C library that a run found, by toolchain, build and symbol.

    Each entry holds the stages that named the symbol, the environments and variants where they did, and the functions
    that hold it. The summary reads the findings.tsv of each config, and so it reads an earlier run as it reads this
    one. It leaves out the known answers, since kat_libc.c holds its call on purpose.
    """
    table = {}
    for path in glob.glob(os.path.join(run_root, "*", "*", "*", "*", "findings.tsv")):
        chain, env, build, variant = path.split(os.sep)[-5:-1]
        if env == "kat":
            continue
        with open(path, encoding="utf-8") as handle:
            next(handle, None)
            for line in handle:
                cells = line.rstrip("\n").split("\t")
                if len(cells) < 4 or cells[3] != "libc":
                    continue
                entry = table.setdefault((chain, build, cells[1]), {"stages": set(), "builds": set(), "functions": set()})
                entry["stages"].add(cells[0])
                entry["builds"].add(env + "/" + variant)
                if len(cells) > 4 and cells[4]:
                    entry["functions"].update(cells[4].split(","))
    return table


def report(run_root):
    """Prints the summary of a run and writes it to summary.tsv in the run.

    Returns 1 where the run found a symbol of the C library, and 0 where it found none.
    """
    table = summary(run_root)
    path = os.path.join(run_root, "summary.tsv")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("toolchain\tbuild\tsymbol\tstages\tbuilds\tfunctions\n")
        for (chain, build, symbol), entry in sorted(table.items()):
            row = (chain, build, symbol, ",".join(sorted(entry["stages"])), ",".join(sorted(entry["builds"])),
                   ",".join(sorted(entry["functions"])))
            handle.write("\t".join(row) + "\n")
            print("  left %-8s %-6s %-24s %-18s %s" % (chain, build, symbol, row[3], row[5][:90] or "-"))
    print("nolibc: %d symbols of a C library left, summary in %s" % (len(table), path))
    return 1 if table else 0


def nolibc(root, run_root, names, environment_names, build_labels, jobs):
    chains = [chain for chain in TOOLCHAINS if (not names or chain.name in names)]
    env = verify.msvc_env()
    for chain in chains:
        if chain.style == "msvc" or chain.linker[0] == "link":
            chain.env = env
    present = [chain for chain in chains if chain.present()]
    absent = [chain.name for chain in chains if chain not in present]
    envs = [(name, defines) for name, defines in environments(root)
            if not environment_names or name in environment_names]
    common, formas = sources(root)
    print("nolibc: toolchains %s%s; environments %s; variants %s" % (
        ", ".join(chain.name for chain in present), (", absent " + ", ".join(absent)) if absent else "",
        ", ".join(name for name, _defines in envs), ", ".join(VARIANTS)))

    configs = []
    kats = []
    kat_folder = os.path.join(root, "test", "verify", "nolibc")
    for chain in present:
        rows = allowlist(root, chain)
        for build in chain.builds:
            if build_labels and build.label not in build_labels:
                continue
            pair = []
            for kat in (KAT_CLEAN, KAT_LIBC):
                folder = os.path.join(run_root, chain.name, "kat", build.label, os.path.splitext(kat)[0])
                pair.append(Config(root, folder, chain, build, VARIANTS["off"], [os.path.join(kat_folder, kat)], {},
                                   "kat", rows))
            kats.append(((chain.name, build.label), build, pair))
        for env_name, env_defines in envs:
            for build in chain.builds:
                if build_labels and build.label not in build_labels:
                    continue
                for variant, variant_defines in VARIANTS.items():
                    folder = os.path.join(run_root, chain.name, env_name, build.label, variant)
                    configs.append(((chain.name, env_name, build.label, variant),
                                    Config(root, folder, chain, build, env_defines + variant_defines, common, formas,
                                           variant, rows)))

    every = [config for _key, _build, pair in kats for config in pair] + [config for _key, config in configs]
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        list(pool.map(lambda config: config.run(), every))

    # The known answers first. A toolchain and a build whose known answers came out wrong give no verdict that counts
    untrusted = {}
    for key, build, (clean, libc) in sorted(kats, key=lambda kat: kat[0]):
        wrong = kat_wrong(next(chain for chain in present if chain.name == key[0]), build, clean, libc)
        print("  kat %-8s %-6s %s" % (key[0], key[1], "ok" if not wrong else "WRONG"))
        for line in wrong:
            print("      " + line)
        if wrong:
            untrusted[key] = wrong

    failed = 0
    for key, config in sorted(configs, key=lambda pair: pair[0]):
        allowed = sum(1 for row in config.findings if row[3].startswith("allowed"))
        counts = (key[0], key[2]) not in untrusted
        verdict = "ok" if not config.failures else "FAILED"
        print("  %-8s %-6s %-6s %-3s %s  allowed %d" % (key + (verdict if counts else "NOT COUNTED", allowed)))
        for failure in config.failures[:8]:
            print("      " + failure)
        failed += 1 if (config.failures or not counts) else 0
    print("nolibc: run in %s" % run_root)
    report(run_root)
    print("nolibc: %d of %d known answers came out right" % (len(kats) - len(untrusted), len(kats)))
    print("nolibc: %d of %d builds hold no symbol of a C library" % (len(configs) - failed, len(configs)))
    return 1 if (failed or untrusted) else 0
