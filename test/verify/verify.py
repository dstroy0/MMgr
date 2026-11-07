# MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py verify: each module built by every toolchain on this host, its behavior and its lowering compared.

For each module chosen, by name or by tag, and each toolchain present:

  behavior   the module's fuzz harness linked with test/fuzz/fuzz_replay.c and the library that toolchain built,
             run on the same inputs drawn from one seed; the harness's oracle judges every build on the same bytes
  ir         clang only: the library emitted as LLVM IR and lowered from it, its objects' instructions compared with the
             direct compile's function by function, and the behavior run again on the build lowered from the IR
  gimple     gcc only: the optimized GIMPLE of the module's sources, kept beside the disassembly
  disasm     every object disassembled with llvm-objdump, a listing per function and its instruction count
  ghidra     every object of the module analyzed and decompiled by Ghidra headless in WSL, a .c per function

Everything lands in one run under build/, verify-<stamp>/<module>/<toolchain>/, with functions.tsv there holding
each function's instruction count under every toolchain and Ghidra's count beside it, and a function one toolchain
inlined or lowered differently reads off one table.
"""

import os
import re
import subprocess
import sys

# The groups --tag names. A module sits under every tag its job falls in.
TAGS = {
    "memory": ["locus_carcerum", "memoria_anularis", "memoria_externa", "memoria_operor", "memoriam_praetereo",
               "proximus_operor", "spatium"],
    "text": ["ascii_persona_bitorum", "cellularum_laboro", "impensa_ancorae_acus", "verba_scribo", "verbum_scrutor"],
    "number": ["clz", "endian", "fractio", "numeros_scribo", "pow5", "transformo"],
    "stream": ["bitorum_introitus_exitus", "octetus_introitus_exitus"],
}

# The library every behavior build links: each module's sources but the ones a harness builds in itself.
LIBRARY_SKIPPED = ("memoria_externa", "memoriam_praetereo", "confinium_exclusivum_infinitas", "confinium_externum")

DEFINES = ["MMGR_ENABLE_DMA=0", "MMGR_ENABLE_EXTRAM=0", "MMGR_CARCER_OWNERS=1024u", "_CRT_SECURE_NO_WARNINGS"]

# What a harness that builds its module in itself adds: its sources and its switches.
SPECIAL = {
    "memoria_externa": (["src/memoria_externa/memoria_externa.c"], ["MMGR_ENABLE_EXTRAM=1"]),
    "memoriam_praetereo": (["src/memoriam_praetereo/memoriam_praetereo.c", "src/memoriam_praetereo/praet_ordo.c",
                            "src/memoriam_praetereo/praet_descriptor.c"],
                           ["MMGR_ENABLE_DMA=1", "PRAET_CHANNELS=8u", "PRAET_SETTLE_MICROS=40u",
                            "PRAET_KEEPALIVE_MICROS=250u", "PRAET_RECOVERY=1", "PRAET_CLOCK_HZ=240000000u",
                            "PRAET_CLOCK_SOURCE=PRAET_CLOCK_CALLER"]),
    "impensa_ancorae_acus": (["src/impensa_ancorae_acus/impensa_ancorae_acus_generic.c"], ["FUZZ_ANCORAE_BARRED=32u"]),
}

GHIDRA = "~/ghidra/ghidra_12.1.2_PUBLIC/support/analyzeHeadless"
VCVARS = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"


class Toolchain:
    """One compiler: how it is called, how it names an object and a program, and how it links."""

    def __init__(self, name, cc, style, extra):
        self.name = name
        self.cc = cc
        self.style = style
        self.extra = extra
        self.env = None

    def present(self):
        return shutil_which(self.cc) is not None

    def compile(self, root, source, out, defines, extra=()):
        includes = [os.path.join(root, part) for part in ("src", "include", "deps/embedded_types/include",
                                                           "test/fuzz", "test/support")]
        if self.style == "msvc":
            cmd = [self.cc, "/nologo", "/c", "/std:c11", "/O2", "/Z7", "/W3"] + self.extra
            cmd += ["/I" + path for path in includes] + ["/D" + define for define in defines]
            cmd += list(extra) + ["/Fo" + out, source]
        else:
            cmd = [self.cc, "-c", "-std=c11", "-O2", "-g"] + self.extra
            cmd += ["-I" + path for path in includes] + ["-D" + define for define in defines]
            cmd += list(extra) + ["-o", out, source]
        return run_tool(cmd, self.env)

    def link(self, objects, out):
        if self.style == "msvc":
            cmd = [self.cc, "/nologo", "/Fe" + out] + objects + ["synchronization.lib"]
        else:
            cmd = [self.cc, "-o", out] + objects + (["-lsynchronization"] if os.name == "nt" else ["-lm", "-lpthread"])
        return run_tool(cmd, self.env)


def shutil_which(program):
    import shutil  # noqa: PLC0415  local, as this module keeps its imports to what every command needs
    return shutil.which(program)


def run_tool(cmd, env=None):
    return subprocess.run(cmd, capture_output=True, text=True, env=env)


def msvc_env():
    """The environment vcvars64 sets, read once, which cl needs for its headers and libraries."""
    if os.name != "nt" or not os.path.isfile(VCVARS):
        return None
    r = subprocess.run('cmd /c ""%s" >nul && set"' % VCVARS, capture_output=True, text=True, shell=True)
    env = {}
    for line in r.stdout.splitlines():
        if "=" in line:
            key, _, value = line.partition("=")
            env[key] = value
    return env or None


def toolchains():
    chains = [
        Toolchain("clang", "clang", "gnu", []),
        # GNU ld on PE-COFF keeps a weak default's offset in its section as an addend on every reference, and a
        # definition that replaces the default is reached that many bytes off. A section for each definition puts
        # every default at offset 0
        Toolchain("gcc", "gcc", "gnu", ["-ffunction-sections", "-fdata-sections"] if os.name == "nt" else []),
        Toolchain("clang-cl", "clang-cl", "msvc", ["/clang:-std=c11"]),
        Toolchain("msvc", "cl", "msvc", ["/experimental:c11atomics"]),
    ]
    env = msvc_env()
    for chain in chains:
        if chain.style == "msvc":
            chain.env = env
    return chains


def modules_chosen(root, names, tags):
    every = sorted(entry for entry in os.listdir(os.path.join(root, "src"))
                   if os.path.isfile(os.path.join(root, "src", entry, "CMakeLists.txt")) and entry not in
                   ("config", "confinium_exclusivum_infinitas", "confinium_externum"))
    chosen = list(names or [])
    for tag in tags or []:
        if tag not in TAGS:
            raise SystemExit("verify: no tag %s; the tags are %s" % (tag, ", ".join(sorted(TAGS))))
        chosen += TAGS[tag]
    if not chosen:
        chosen = every
    unknown = [name for name in chosen if name not in every]
    if unknown:
        raise SystemExit("verify: no module %s" % ", ".join(unknown))
    return sorted(set(chosen))


def library_sources(root):
    sources = []
    for module in sorted(os.listdir(os.path.join(root, "src"))):
        folder = os.path.join(root, "src", module)
        if not os.path.isdir(folder) or module in LIBRARY_SKIPPED or module == "config":
            continue
        for entry in sorted(os.listdir(folder)):
            if not entry.endswith(".c"):
                continue
            if module == "impensa_ancorae_acus" and entry != "impensa_ancorae_acus_generic.c":
                continue
            sources.append(os.path.join(folder, entry))
    sources.append(os.path.join(root, "test", "support", "host_wait.c"))
    return sources


def object_name(chain, folder, source):
    stem = os.path.splitext(os.path.basename(source))[0]
    return os.path.join(folder, stem + (".obj" if chain.style == "msvc" else ".o"))


def disassembled(objdump, obj):
    """Each function's instructions, mnemonic and operands with the addresses taken out."""
    r = run_tool([objdump, "-d", "--no-show-raw-insn", "--no-leading-addr", obj])
    functions = {}
    name = None
    for line in r.stdout.splitlines():
        head = re.match(r"^\s*(?:[0-9a-f]+\s+)?<([^>]+)>:\s*$", line)
        if head:
            name = head.group(1)
            functions[name] = []
            continue
        if name and line.startswith((" ", "\t")) and line.strip():
            text = re.sub(r"\b0x[0-9a-f]+\b|<[^>]*>", "#", line.strip())
            functions[name].append(re.sub(r"\s+", " ", text))
    return functions


def ghidra_decompiled(root, obj, out):
    """Ghidra headless in WSL over one object, every function decompiled into out."""
    def wsl_path(path):
        path = os.path.abspath(path).replace("\\", "/")
        return "/mnt/%s%s" % (path[0].lower(), path[2:]) if re.match(r"^[A-Za-z]:", path) else path
    os.makedirs(out, exist_ok=True)
    project = os.path.join(out, "project")
    os.makedirs(project, exist_ok=True)
    script_dir = os.path.join(root, "test", "verify")
    cmd = ["wsl", "-e", "bash", "-lc",
           "%s '%s' verify -import '%s' -scriptPath '%s' -postScript DecompileAll.java '%s' -deleteProject"
           % (GHIDRA, wsl_path(project), wsl_path(obj), wsl_path(script_dir), wsl_path(out))]
    r = run_tool(cmd)
    summary = {}
    table = os.path.join(out, "functions.tsv")
    if os.path.isfile(table):
        with open(table, encoding="utf-8") as rows:
            next(rows, None)
            for row in rows:
                cells = row.rstrip("\n").split("\t")
                if len(cells) >= 4:
                    summary[cells[0]] = int(cells[3])
    return r.returncode == 0 and os.path.isfile(table), summary


def verify(root, run_root, names, tags, runs, seed, use_ghidra, objdump):
    chains = [chain for chain in toolchains() if chain.present()]
    absent = [chain.name for chain in toolchains() if not chain.present()]
    modules = modules_chosen(root, names, tags)
    os.makedirs(run_root, exist_ok=True)
    print("verify: %s, toolchains %s%s" % (", ".join(modules), ", ".join(c.name for c in chains),
                                          (", absent " + ", ".join(absent)) if absent else ""))
    failures = []
    library = library_sources(root)

    for chain in chains:
        folder = os.path.join(run_root, "library", chain.name)
        os.makedirs(folder, exist_ok=True)
        broken = []
        for source in library:
            r = chain.compile(root, source, object_name(chain, folder, source), DEFINES)
            if r.returncode != 0:
                broken.append((source, (r.stdout + r.stderr).strip().splitlines()[:6]))
        chain.library = [object_name(chain, folder, source) for source in library]
        chain.broken = broken
        if broken:
            failures.append("%s: the library does not compile (%d sources)" % (chain.name, len(broken)))
            print("  %-9s library FAILED to compile: %d of %d sources" % (chain.name, len(broken), len(library)))
            for source, lines in broken[:3]:
                print("    %s" % os.path.relpath(source, root))
                for line in lines[:4]:
                    print("      " + line)

    clang = next((chain for chain in chains if chain.name == "clang"), None)
    if clang is not None and not clang.broken:
        ir_folder = os.path.join(run_root, "library", "clang-ir")
        os.makedirs(ir_folder, exist_ok=True)
        clang.ir_library = []
        for source in library:
            stem = os.path.splitext(os.path.basename(source))[0]
            ll = os.path.join(ir_folder, stem + ".ll")
            # The use lists kept in the text, since the code generator breaks ties by them and the text drops them
            # by default
            clang.compile(root, source, ll, DEFINES, ["-S", "-emit-llvm", "-Xclang", "-emit-llvm-uselists"])
            lowered = os.path.join(ir_folder, stem + ".o")
            # The IR is the optimizer's output already, and so the lowering runs the code generator alone: the
            # middle end run a second time over it is a different program from the direct compile's
            run_tool(["clang", "-c", "-O2", "-g", "-Xclang", "-disable-llvm-passes", ll, "-o", lowered])
            clang.ir_library.append(lowered)

    for module in modules:
        print("== %s" % module)
        module_root = os.path.join(run_root, module)
        counts = {}
        harness = os.path.join(root, "test", "fuzz", module + "_fuzz.c")
        extra_sources, extra_defines = SPECIAL.get(module, ([], []))
        module_sources = [os.path.join(root, "src", module, entry) for entry in
                          sorted(os.listdir(os.path.join(root, "src", module))) if entry.endswith(".c")]
        if module == "impensa_ancorae_acus":
            module_sources = [os.path.join(root, extra_sources[0])]

        for chain in chains:
            if chain.broken:
                continue
            folder = os.path.join(module_root, chain.name)
            os.makedirs(folder, exist_ok=True)
            defines = DEFINES + extra_defines
            objects = []
            for source in [harness, os.path.join(root, "test", "fuzz", "fuzz_replay.c")] + \
                    [os.path.join(root, path) for path in extra_sources]:
                obj = object_name(chain, folder, source)
                r = chain.compile(root, source, obj, defines)
                if r.returncode != 0:
                    failures.append("%s %s: the harness does not compile" % (module, chain.name))
                    print("  %-9s behavior FAILED to compile %s" % (chain.name, os.path.basename(source)))
                    for line in (r.stdout + r.stderr).strip().splitlines()[:4]:
                        print("      " + line)
                    objects = None
                    break
                objects.append(obj)
            if objects is not None:
                skipped = set()
                if extra_sources:
                    skipped = {os.path.splitext(os.path.basename(path))[0] for path in extra_sources}
                    if module == "impensa_ancorae_acus":
                        skipped.add("impensa_ancorae_acus_generic")
                program = os.path.join(folder, module + "_replay.exe")
                linked = objects + [obj for obj in chain.library
                                    if os.path.splitext(os.path.basename(obj))[0] not in skipped]
                r = chain.link(linked, program)
                if r.returncode != 0:
                    failures.append("%s %s: the replay does not link" % (module, chain.name))
                    print("  %-9s behavior FAILED to link" % chain.name)
                    for line in (r.stdout + r.stderr).strip().splitlines()[:6]:
                        print("      " + line)
                else:
                    played = run_tool([program, str(runs), str(seed)])
                    verdict = "ok" if played.returncode == 0 else "FAILED"
                    print("  %-9s behavior %s  %s" % (chain.name, verdict,
                                                      (played.stdout + played.stderr).strip().splitlines()[-1:]))
                    if played.returncode != 0:
                        failures.append("%s %s: %s" % (module, chain.name, (played.stderr.strip() or "exit %d" %
                                                                            played.returncode)[:200]))

            for source in module_sources:
                # A module the library skips is built with the harness, its switches its own, in the module's folder
                obj = object_name(chain, folder if module in LIBRARY_SKIPPED else
                                  os.path.join(run_root, "library", chain.name), source)
                if chain.name == "gcc":
                    gimple = os.path.join(folder, "gimple")
                    os.makedirs(gimple, exist_ok=True)
                    chain.compile(root, source, os.path.join(gimple, "dump.o"), DEFINES,
                                  ["-fdump-tree-optimized", "-dumpdir", gimple + os.sep])
                listing = disassembled(objdump, obj)
                with open(os.path.join(folder, os.path.basename(obj) + ".dis"), "w", encoding="utf-8") as out:
                    for function, lines in sorted(listing.items()):
                        out.write("%s:\n%s\n\n" % (function, "\n".join("    " + line for line in lines)))
                for function, lines in listing.items():
                    counts.setdefault(function, {})[chain.name] = len(lines)
                if use_ghidra:
                    done, summary = ghidra_decompiled(root, obj, os.path.join(folder, "ghidra",
                                                                              os.path.basename(obj)))
                    if not done:
                        failures.append("%s %s: ghidra did not decompile %s" % (module, chain.name,
                                                                                 os.path.basename(obj)))
                    for function, instructions in summary.items():
                        counts.setdefault(function, {})[chain.name + ":ghidra"] = instructions

        if clang is not None and not clang.broken:
            mismatched = []
            own_ir = os.path.join(module_root, "clang-ir")
            for source in module_sources:
                stem = os.path.splitext(os.path.basename(source))[0]
                if module in LIBRARY_SKIPPED:
                    # The library holds no IR of a module it skips: it is lowered here, with the module's switches
                    os.makedirs(own_ir, exist_ok=True)
                    ll = os.path.join(own_ir, stem + ".ll")
                    clang.compile(root, source, ll, DEFINES + extra_defines,
                                  ["-S", "-emit-llvm", "-Xclang", "-emit-llvm-uselists"])
                    run_tool(["clang", "-c", "-O2", "-g", "-Xclang", "-disable-llvm-passes", ll, "-o",
                              os.path.join(own_ir, stem + ".o")])
                    direct_obj = object_name(clang, os.path.join(module_root, "clang"), source)
                    lowered_obj = os.path.join(own_ir, stem + ".o")
                else:
                    direct_obj = object_name(clang, os.path.join(run_root, "library", "clang"), source)
                    lowered_obj = os.path.join(run_root, "library", "clang-ir", stem + ".o")
                if not (os.path.isfile(direct_obj) and os.path.isfile(lowered_obj)):
                    mismatched.append("%s (no object)" % stem)
                    continue
                direct = disassembled(objdump, direct_obj)
                lowered = disassembled(objdump, lowered_obj)
                for function in sorted(set(direct) | set(lowered)):
                    if direct.get(function) != lowered.get(function):
                        mismatched.append(function)
            print("  clang     ir       %s" % ("ok, every function lowers from the IR as the direct compile does"
                                              if not mismatched else
                                              "DIFFERS in %d functions: %s" % (len(mismatched),
                                                                               ", ".join(mismatched[:6]))))
            if mismatched:
                failures.append("%s: the IR lowers differently from the direct compile in %s" %
                                (module, ", ".join(mismatched[:6])))
            folder = os.path.join(module_root, "clang")
            harness_objects = [object_name(clang, folder, source) for source in
                               [harness, os.path.join(root, "test", "fuzz", "fuzz_replay.c")] +
                               [os.path.join(root, path) for path in extra_sources]]
            skipped = {os.path.splitext(os.path.basename(path))[0] for path in extra_sources}
            if module == "impensa_ancorae_acus":
                skipped.add("impensa_ancorae_acus_generic")
            program = os.path.join(folder, module + "_replay_from_ir.exe")
            linked = harness_objects + [obj for obj in clang.ir_library
                                        if os.path.splitext(os.path.basename(obj))[0] not in skipped]
            if all(os.path.isfile(obj) for obj in harness_objects) and clang.link(linked, program).returncode == 0:
                played = run_tool([program, str(runs), str(seed)])
                print("  clang     ir run   %s" % ("ok" if played.returncode == 0 else "FAILED"))
                if played.returncode != 0:
                    failures.append("%s: the build lowered from the IR fails its oracle: %s" %
                                    (module, played.stderr.strip()[:200]))
            else:
                failures.append("%s: the build lowered from the IR does not link" % module)

        columns = sorted({column for row in counts.values() for column in row})
        with open(os.path.join(module_root, "functions.tsv"), "w", encoding="utf-8") as table:
            table.write("function\t%s\n" % "\t".join(columns))
            for function in sorted(counts):
                table.write("%s\t%s\n" % (function, "\t".join(str(counts[function].get(c, "")) for c in columns)))
        differing = [function for function, row in counts.items()
                     if len({row.get(c.name) for c in chains if not c.broken}) > 1]
        print("  functions %d, instruction counts differing across toolchains in %d" % (len(counts), len(differing)))

    print("verify: run in %s" % run_root)
    if failures:
        print("%d failures:" % len(failures))
        for failure in failures:
            print("  " + failure)
        return 1
    return 0
