# Documentation

Technical reference for Gemini System. Paths below are relative to this directory.

## VM and bytecode

- **[Bytecode VM](vm.md)** — text `.tbc` format, opcodes, stack, parser output (`LoadedBytecode`), C++ runtime API.

## Shells and host commands

- **[Developer shell (TCL)](tcl-shell.md)** — system-level TCL commands (`BASIC`, `PROC`, `ASM`, filesystem, variables, VOC-backed `RUN`/`EDIT`, …).
- **[Assembler shell (ASM)](assembler-shell.md)** — VM-level debugger workflow (`STEP`, `CONT`, breakpoints, trace, dumps).
- **[ENGLISH query core](english.md)** — `LIST`, `SORT`, `COUNT`, `SELECT`; file-scoped **`DICT-<file>`** / global **`DICT`**; **`RESOLVE-FIELD`**; **`DEFINE-FIELD`** (minimal type-**`A`** DICT authoring); active-list lifecycle (`LIST-LIST`, `CLEAR-LIST`).

## BASIC

- **[BASIC shell](basic-shell.md)** — BASIC program buffer, SAVE/LOAD, COMPILE/`RUN`; `EDIT` delegates to the system line editor.
- **[BASIC language](basic-language.md)** — compiler subset (`LET`, `PRINT`, `END`, …), expression rules, diagnostics.
- **[Compiler architecture](compiler-architecture.md)** — BASIC compiler phases: parse, semantic analysis, bytecode emit.

## Storage and VOC

- **[File system](filesystem.md)** — Pick-style logical files and records (host-backed store used by Tcl and BASIC runtime I/O).
- **[Filesystem M3 model](filesystem-m3.md)** — Milestone 3 attribute-aware record model and parse/serialize contract.

## Roadmap

- **[Project milestones](milestones.md)** — phased roadmap; **through Milestone 4** tracks what is implemented today; later milestones evolve.
