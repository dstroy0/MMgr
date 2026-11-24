# No C library in src/ {#qa_nolibc}

The check holds the code of `src/` to no symbol of a C library. It builds `src/` with each host
toolchain, and it reads the IR, the objects and the linked image of each build. `test/` can call
the C library as an oracle or to debug, and the check does not read `test/`.

## Run it

```sh
python test/harness.py nolibc                                   # every toolchain, environment and build
python test/harness.py nolibc --toolchain clang --build O2-lto  # one toolchain, one build
python test/harness.py nolibc --environment word16              # one environment
```

The run goes into `build/nolibc-<stamp>/<toolchain>/<environment>/<build>/<variant>/`. The file
`findings.tsv` in each folder lists each symbol that a stage found, the file that names it, the
verdict, and the functions that hold it. The verdict is `own`, `allowed`, or `libc`. The ir stage
reads the functions from the IR, and the object stage reads them from the relocations.

At its end, the run prints each symbol of a C library that it found, by toolchain and build. Each
row gives the stages that name the symbol and the functions that hold it. The rows go into
`summary.tsv` in the run, and the harness prints them again for an earlier run:

```sh
python test/harness.py nolibc --report build/nolibc-<stamp>
```

## What it holds

| Stage  | What it reads                                                                                                                            | What fails it                                                                                    |
| ------ | ---------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| ir     | The functions that the IR calls. clang and clang-cl give LLVM IR, gcc gives its optimized GIMPLE. msvc gives no IR that a tool can read. | A call to a function that `src/` does not define and that the allowlist does not name.           |
| object | Each undefined symbol of each object, from `llvm-nm`, which reads ELF and COFF.                                                          | A symbol that no object of `src/` defines and that the allowlist does not name.                  |
| link   | One image of all the objects, with no default library and no entry point.                                                                | A symbol that stays undefined, a link that fails, or an image that does not read back.           |
| cycle  | The call graph of the objects, from their relocations, the tables of entries too.                                                        | A definition of `memset`, `memcpy`, `memmove` or `memcmp` whose code reaches its own name again. |

## The string shim

A compiler writes calls to `memset`, `memcpy`, `memmove` and `memcmp` that no source names. It
writes them for a copy of a struct, for the zero members of a literal, and for some loops. `src/config/mmgr_string_shim.c` defines the four names over `memor`, and every image
of the check links it. A program with no C library links the target `mmgr_string_shim_<env>`.

The target is opt in, and `mmgr_<env>` does not link it. A suite that checks MMgr against the C
library keeps the names of the C library.

A call to one of the four names links, since `src/` defines it. So the link stage cannot see a
definition that calls itself, and the cycle stage looks for that. No code of `src/` calls the
four names, and the ir and object stages hold that too.

## The build rules that keep the C library out

| Rule                                              | Where                       | Why                                                                                    |
| ------------------------------------------------- | --------------------------- | -------------------------------------------------------------------------------------- |
| A module that names no `OPTIMIZE` builds at `-O2` | `mmgr_add_module`           | clang at `-O0` writes the zero fill of each `EMBED_CALL` literal as a call to `memset` |
| `/GS-` for the frontend of cl                     | `mmgr_flags`                | `/GS` needs the stack cookie and its check from the C runtime                          |
| No LTO for the string shim                        | `src/config/CMakeLists.txt` | LTO drops a definition that the code generator calls after LTO                         |

The check compiles each module with the `OPTIMIZE` that its `CMakeLists.txt` names, and `-O2`
where it names none, as the real build does. A build with `MMGR_MODULE_OPTIMIZE` off, as the
coverage build is, takes its own level and links the C library.

The IR holds no call of the C library. A call to `memset`, `memcpy`, `memmove` or `memcmp` fails
the ir stage, even where the string shim defines the name. So does a mem intrinsic whose length
the IR computes, and an intrinsic of `<math.h>` that lowers to a call. A mem intrinsic of a
constant length lowers to stores or to a call, as the level decides, and the object stage reads
which.

## The known answers

Each toolchain builds two known answers in each of its builds, ahead of the checks of `src/`. A
toolchain and a build whose known answers come out wrong give no verdict that counts, and the run
fails.

| File                             | What it holds                     | The right answer                                  |
| -------------------------------- | --------------------------------- | ------------------------------------------------- |
| `test/verify/nolibc/kat_clean.c` | Arithmetic on its arguments alone | Every stage passes, and the image links           |
| `test/verify/nolibc/kat_libc.c`  | A call to `strtol`                | The ir, object and link stages each name `strtol` |

`strtol` has no builtin form, and so each compiler writes a call to it at each level. An LTO build
writes no LTO object where its link fails, and so its link alone judges `kat_libc.c`.

## LTO

An LTO build moves the code generator to the link. There, it can lower a copy across two sources
to a call of the C library. No object of one source shows that call. So an LTO build reads what
the link-time code generator gives:

| Toolchain       | IR                                                          | Object                                      |
| --------------- | ----------------------------------------------------------- | ------------------------------------------- |
| clang, clang-cl | The bitcode before code generation, from `/lldsavetemps`    | The object of the LTO, from `/lldsavetemps` |
| gcc             | The GIMPLE of the ltrans unit, from `-fdump-tree-optimized` | The ltrans object, from `-save-temps`       |
| msvc            | None: `/GL` writes no form that a tool can read             | None: the link alone judges it              |

The link of an LTO build exports each external symbol that `src/` defines. An LTO link removes a
function that nothing calls, and an image with no function holds no symbol of a C library. So an
image that exports less than its definition file names fails the link stage.

## The matrix

| Axis        | Values                                                                              |
| ----------- | ----------------------------------------------------------------------------------- |
| Toolchain   | clang, gcc, clang-cl, msvc                                                          |
| Environment | host, word32, word16, idx16, from `MMGR_ENVIRONMENTS` in the top `CMakeLists.txt`   |
| Build       | Three optimization levels and two LTO builds for each toolchain                     |
| Variant     | `off`, the default build. `on`, with `MMGR_ENABLE_EXTRAM` and `MMGR_ENABLE_DMA` set |

The `checks` environment is not in the matrix. It puts `test/support/mmgr_host_traps.h` into
`src/`, and that header calls `fprintf` and `abort` to report a failed expectation.

The `off` variant links one image for each forma of `impensa_ancorae_acus`, because a build
chooses one forma source of five.

## The allowlist

Each toolchain has one allowlist, `test/verify/nolibc/<toolchain>.tsv`. A row has a symbol, a
kind and a reason. Two spaces or a tab separate the cells.

| Kind    | What it is                                                                  | What the link does                          |
| ------- | --------------------------------------------------------------------------- | ------------------------------------------- |
| host    | A function that the program defines, as `embed_turnstile.h` says            | A stub defines it                           |
| marker  | Data that the program defines, as `_fltused` for the MSVC ABI               | A stub defines it                           |
| runtime | A helper of the compiler's own runtime library, as `___chkstk_ms` of libgcc | The row of the toolchain links that library |
| linker  | A symbol that the linker defines, as `__ImageBase`                          | Nothing                                     |

No row names a function of the C library. A row of that kind removes the check that the row is
for.

## Add a toolchain

1. Put the compiler, a linker and `llvm-nm` on the `PATH`.
2. Add a `Toolchain` row to `TOOLCHAINS` in `test/verify/nolibc.py`.
3. Add `test/verify/nolibc/<name>.tsv`. Copy the host rows of a toolchain with the same ABI.
4. Run `python test/harness.py nolibc --toolchain <name>`.
5. Read each `libc` verdict. Fix the source, or add a row where the symbol is of a kind above.

A row has these fields:

| Field     | What it holds                                                                               |
| --------- | ------------------------------------------------------------------------------------------- |
| `name`    | The name of the row, and the name of its allowlist                                          |
| `cc`      | The compiler to call                                                                        |
| `style`   | `gnu` for the flags of gcc and clang, `msvc` for the flags of cl and clang-cl               |
| `ir`      | `llvm`, `gimple`, or `None` where the toolchain gives no IR that a tool can read            |
| `builds`  | Each `Build`: a label, the compile flags, and for an LTO build the flags that the link adds |
| `flags`   | The flags of each compile                                                                   |
| `linker`  | The argv that links a DLL with no default library and no entry point                        |
| `runtime` | The libraries of the compiler's runtime, after the objects                                  |

This row adds a clang that targets the GNU ABI of MinGW. It links with lld, and it reads its IR
as the clang row does. `--entry=` with no name gives the image no entry point. `--Xlink` gives
`/lldsavetemps` to the COFF linker, which keeps the bitcode and the object of the LTO:

```python
Toolchain("clang-mingw", "clang", "gnu", "llvm",
          [Build("O2", ["-O2", "--target=x86_64-w64-mingw32"]),
           Build("O2-lto", ["-O2", "-flto", "--target=x86_64-w64-mingw32"],
                 ["-flto", "-Wl,--Xlink=/lldsavetemps"])],
          [], ["clang", "--target=x86_64-w64-mingw32", "-fuse-ld=lld", "-shared", "-nostdlib",
               "-Wl,--no-undefined", "-Wl,--entry="], []),
```

Its allowlist is a copy of `gcc.tsv`, since the two share the ABI.
