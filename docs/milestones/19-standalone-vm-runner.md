← [Project milestones index](../milestones.md)

## Milestone 19 — Standalone VM Runner

Ship a **Pick-independent** host-native bytecode runner: a separate executable that loads Gemini `.tbc` (and optional language modules), wires console I/O, and runs until halt — without catalogue, Tcl, session table, or Pick filesystem. Unblocks sister-project **apollo-compiler** Milestone 6 (standalone VM execution) while leaving Application and Service editions unchanged. *Status: planned.*

Version **1.0.0** ([**Milestone 18**](18-version-1-gemini-system-service.md)) delivered Pick-authentic multi-session service and application hosts. This milestone extracts a **thin VM entry point** for compiled programs (Apollo Pascal first; BASIC modules welcome) that must not drag Pick IDE/OS concerns.

**Standing invariant:** existing **`gemini-system`**, **`gemini-daemon`**, and **`gemini-console`** behaviour must not regress. Full `ctest` and Application Edition smoke remain green after every stage.

---

### 1. Purpose and rationale

Gemini already runs a bytecode VM on desktop OSes inside Pick hosts (`gemini-system` / daemon sessions). Operators and Apollo need a path that feels like **`gemini-vm program.tbc`**: load, run, exit — not cold-start, `LOGON PLEASE:`, or Tcl.

| Host | Role |
|------|------|
| **Application / Service** | Pick authenticity: catalogue, login, Tcl/BASIC/PROC, locks, VOC/MD |
| **Standalone VM runner** | Portable execution: `.tbc` + language plugins + stdin/stdout |

A separate binary keeps the boundary honest. Do **not** overload Application Edition with a “bare VM” mode that still boots Pick.

Sister project **apollo-compiler** Milestone 6 defines the acceptance story from the compiler side; this milestone owns the Gemini-side runner.

---

### 2. Scope

#### 2.1 Deliverable binary

- New executable (working name **`gemini-vm`**) linking `gemini-core` (VM, parser, language registry/loader) **without** requiring Tcl shell, `GeminiSession` / session table, catalogue login, or daemon IPC.
- CLI: path to `.tbc` (and flags as needed for modules path, help).
- Process exit status reflects successful halt vs load/runtime failure.

#### 2.2 Host surface (v1)

- **Console:** stdin / stdout / stderr (or equivalent stream hooks on the runtime).
- **Filesystem:** **unbound** — do not attach Pick [`FileSystem`](../../src/core/filesystem/FileSystem.h). Existing runtime behaviour when FS is null (“filesystem backend not configured”) is acceptable for console-only programs.
- **Language modules:** load optional shared modules (same ABI as M11) so Apollo/Pascal and BASIC `CALL_FUNC` work when present.

#### 2.3 Hard non-Pick boundary

The runner **must not** include or require:

- Catalogue / `ACCOUNTS.json` / account roots
- Tcl, BASIC shell, PROC host, assembler debugger REPL
- [`GeminiSession`](../../src/userland/tcl/GeminiSession.h), [`SessionTable`](../../src/userland/tcl/SessionTable.h), cooperative session scheduler
- Daemon IPC, ports, Pick lock-session identity
- Boot monitor Pick cold-start banner as a product UX (internal reuse of loader code is fine if it does not imply Pick attachment)

#### 2.4 Review + spike approach

1. **Review** — document Pick vs portable boundaries in the current VM and plugin model.
2. **Spike** — run an Apollo-emitted console-only program (e.g. `hello.pas` / `count.pas` → `.tbc`) on `gemini-vm`.
3. **Harden** — CMake target, tests, docs; keep Pick hosts untouched.

---

### 3. Non-goals

- Mercury / third-repo split of the VM (revisit only if packaging pain demands it)
- Rewriting the VM from scratch
- Pascal or Pick **file I/O** as a requirement for the first proof
- CPU-bound multi-session fairness ([**Milestone 21**](21-execution-fairness-cpu-bound-yield.md))
- R83 compatibility gap closure ([compatibility notes](../compatibility-r83-pick.md); planned after this milestone)
- Changing Application/Service edition semantics

---

### 4. Compatibility expectations

- **`gemini-system` / daemon / console:** no intentional behaviour change; regression barred by full test suite.
- **Runtime:** nullable filesystem and language-registry freeze remain the extension points; Pick hosts keep binding FS and session I/O as today.
- **Apollo:** success when an Apollo-compiled console-only Pascal program runs on `gemini-vm`; Apollo does not reimplement a VM.

---

### 5. Dependencies and grounding

| Dependency | Role |
|------------|------|
| [M11](11-multi-language-runtime-infrastructure.md) | Language module ABI / loader |
| VM + `.tbc` | [`docs/vm.md`](../vm.md), [`Runtime`](../../src/core/vm/Runtime.h) |
| [M18](18-version-1-gemini-system-service.md) | v1.0 baseline; editions remain Pick hosts |
| Apollo Compiler M6 | Acceptance program + emit path |

---

### 6. Deliverables (anticipated)

| Area | Artifact |
|------|----------|
| Binary | `gemini-vm` (name finalized in implementation) under `src/` + CMake |
| Review note | Short section in this milestone or `docs/vm.md`: Pick vs portable boundaries |
| Tests | Load+run `.tbc` fixture; console I/O smoke; optional Apollo-emitted artifact in CI or documented manual step |
| Docs | Runner usage in `docs/vm.md` and/or README; Apollo docs cross-link once available |

---

### 7. Milestone completion criteria

- [ ] Review recorded: Pick vs portable boundaries for the current VM
- [ ] **`gemini-vm`** (or chosen name) builds and runs a `.tbc` with console I/O only, **no** catalogue/Tcl/session table/Pick FS
- [ ] An Apollo-compiled console-only Pascal program runs on that target (automated or documented manual smoke)
- [ ] Full existing `ctest` green; Application Edition smoke unchanged
- [ ] Operator/developer docs describe how to invoke the standalone runner
- [ ] Milestone hub / README list M19 implemented when closed

---

### 8. Suggested implementation stages

1. **Review** — inventory Pick coupling in VM entry paths; write boundary notes. *Status: planned.*
2. **Spike runner** — minimal executable: parse `.tbc`, run with stdout, FS unset; fixture test. *Status: planned.*
3. **Modules + Apollo proof** — optional module load; run Apollo-emitted console program. *Status: planned.*
4. **Docs + closes M19** — vm/README notes; hub status; full suite green. **Closes Milestone 19.** *Status: planned.*

---

### 9. Follow-on (beyond M19)

- Optional host filesystem façade (non-Pick) for BASIC/Pascal file I/O
- R83 compatibility gap work (within reason) — see [`compatibility-r83-pick.md`](../compatibility-r83-pick.md)
- CPU-bound cooperative yield — [**Milestone 21**](21-execution-fairness-cpu-bound-yield.md)

*Status: planned.*
