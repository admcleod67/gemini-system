# Gemini System

A **Pick System R83–like** environment built from the ground up in **modern C++**. It is heavily inspired by **Pick OS**
in spirit and direction, but it is **not** a port or clone: the design evolves deliberately and may include capabilities
beyond classic Pick—for example, **more direct interaction with the bytecode VM** (developer shell, stack inspection,
test hooks) to support experimentation and teaching.

The implementation remains intentionally incremental, but the platform is now broader: a mature bytecode VM/runtime,
filesystem-backed Pick file semantics, BASIC compiler/shell workflows, and ENGLISH/DICT query foundations. The goal is
still to grow the system in **small, verifiable steps** (tests, clear boundaries) rather than landing a large stack at once.

A phased **roadmap** (Milestone 1 onward) is summarized in **[`docs/milestones.md`](docs/milestones.md)** (hub); long-form text for each milestone lives under **[`docs/milestones/`](docs/milestones/)**. **Milestones 1–4** track what is implemented today (core stack, multi-account session and VOC, ENGLISH + filesystem maturation, and BASIC language fidelity stages), and later milestones remain subject to refinement.

## Building

Requires **CMake 3.16+** and a **C++17** toolchain.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Artifacts include **`gemini-system`** (host entry point with boot and Tcl REPL), and **`pick-tests`** (unit tests).

## Documentation

More detail lives under **[`docs/`](docs/README.md)**:

- **[Bytecode VM](docs/vm.md)** — `.tbc` format, opcodes, parser and runtime.
- **[Developer shell (TCL)](docs/tcl-shell.md)** — host shell commands and mode entry (`BASIC`, `PROC`, `ASM`).
- **[Assembler shell (ASM)](docs/assembler-shell.md)** — VM-level debugger workflow and command set.
- **[ENGLISH query core](docs/english.md)** — `LIST`, `SORT`, `COUNT`, `SELECT`, file-scoped `DICT-*` lookup, **`RESOLVE-FIELD`**, **`DEFINE-FIELD`**; active-list commands (`LIST-LIST`, `CLEAR-LIST`).
- **[File system](docs/filesystem.md)** — Pick logical files/records backing Tcl and BASIC I/O.
- **[Filesystem M3 model](docs/filesystem-m3.md)** — Milestone 3 attribute-aware record model and parse/serialize contract.
- **[BASIC shell](docs/basic-shell.md)** — program buffer, SAVE/LOAD, COMPILE/`RUN`; `EDIT` uses the shared system line editor.
- **[BASIC language](docs/basic-language.md)** — supported BASIC compiler subset and expression semantics.
- **[Compiler architecture](docs/compiler-architecture.md)** — parse/semantic/emit phase boundaries.
- **[Project milestones](docs/milestones.md)** — roadmap hub; **[per-milestone detail](docs/milestones/)** (`NN-slug.md`).

## Layout (high level)

- **`src/core/vm/`** — bytecode parser and VM runtime (`gemini-core`)
- **`src/core/filesystem/`** — Pick-style file and record layer (`gemini-core`)
- **`src/core/voc/`** — VOC dictionary parsing and program/script name resolution (`gemini-core`)
- **`src/userland/tcl/`** — Tcl host shell and command integration (`gemini-tcl`)
- **`src/userland/basic/`** — BASIC shell, compiler, and runtime components
- **`include/pick_system/`** — shared headers (e.g. version)
- **`include/pickvm/`** — public VM umbrella header (`core.hpp`)
- **`tests/`** — **doctest**-based tests
- **`programs/`** — sample `.tbc` bytecode files
- **`docs/`** — VM and shell documentation

## License

See [LICENSE](LICENSE).
