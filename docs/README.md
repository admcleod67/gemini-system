# Documentation

Technical reference for Gemini System. Paths below are relative to this directory.

## VM and bytecode

- **[Bytecode VM](vm.md)** — text `.tbc` format, opcodes, stack, parser output (`LoadedBytecode`), C++ runtime API.

## Multi-language runtime

- **[Bytecode contract for external compilers](bytecode.md)** — **`CALL_FUNC`** encoding, namespace/function IDs, stack semantics.
- **[Language module ABI](language-modules.md)** — **`register_language`**, building shared modules, reference BASIC walkthrough.
- **[Language namespace schema](schemas/language-namespaces.json)** — machine-readable ID catalogue.

## Shells and host commands

- **[Developer shell (TCL)](tcl-shell.md)** — system-level TCL commands (`BASIC`, `PROC`, `ASM`, filesystem, variables, VOC-backed `RUN`/`EDIT`, …).
- **[Session model](session.md)** — `GeminiSession` lifecycle, I/O channels, and standalone hosting.
- **[Service daemon](daemon.md)** — `GeminiServiceDaemon`, `gemini-daemon`, IPC, and process vs session scope.
- **[Console client](console.md)** — `gemini-console` terminal client, attach/create, graceful detach.
- **[PROC language](proc.md)** — host-interpreted PROC execution model, token/substitution rules, Tcl bridge, and statement semantics.
- **[Assembler shell (ASM)](assembler-shell.md)** — VM-level debugger workflow (`STEP`, `CONT`, breakpoints, trace, dumps).
- **[ENGLISH query core](english.md)** — `LIST`, `SORT`, `COUNT`, `SELECT`; file-scoped **`DICT-<file>`** / global **`DICT`**; computed F/I fields; **`RESOLVE-FIELD`**, **`LIST-DICT`**, **`DEFINE-FIELD`** (minimal type-**`A`** DICT authoring); active-list lifecycle (`LIST-LIST`, `CLEAR-LIST`).

## BASIC

- **[BASIC shell](basic-shell.md)** — BASIC program buffer, SAVE/LOAD, COMPILE/`RUN`; `EDIT` delegates to the system line editor.
- **[BASIC language](basic-language.md)** — compiler subset (`LET`, `PRINT`, `END`, …), expression rules, diagnostics.
- **[BASIC file I/O (DICT-aware reads)](basic-file-io.md)** — `READV` on computed F/I fields.
- **[Compiler architecture](compiler-architecture.md)** — BASIC compiler phases: parse, semantic analysis, bytecode emit.

## Storage and VOC

- **[File system](filesystem.md)** — Pick-style logical files and records (host-backed store used by Tcl and BASIC runtime I/O).
- **[Filesystem M3 model](filesystem-m3.md)** — Milestone 3 attribute-aware record model and parse/serialize contract.
- **[DICT dictionary items](dict.md)** — A/S/F/I item types, lookup precedence, authoring, and Tcl introspection.
- **[Correlatives and computed attributes](correlatives.md)** — F/I syntax, evaluation, and conversion codes.

## Roadmap

- **[Project milestones](milestones.md)** — hub index; **Milestones 1–18** completed (Version **1.0.0**); **Milestone 19** (post–v1.0) covers CPU-bound execution fairness.
- **[Changelog](../CHANGELOG.md)** — release notes (current: **1.0.0**).
- **[Milestone detail pages](milestones/)** — one file per milestone (`NN-slug.md` for sortable filenames).
- **[Compatibility (R83/Pick)](compatibility-r83-pick.md)** — implemented/partial/deferred Gemini compatibility notes versus classic Pick behavior.
