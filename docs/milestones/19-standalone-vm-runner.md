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

Sister project **apollo-compiler** Milestone 6 defines the acceptance story from the compiler side; this milestone owns the Gemini-side runner and (for the spike only) may host a minimal Pascal I/O module — see §2.5.

---

### 2. Scope

#### 2.1 Deliverable binary

- New executable (working name **`gemini-vm`**) linking `gemini-core` (VM, parser, language registry/loader) **without** requiring Tcl shell, `GeminiSession` / session table, catalogue login, or daemon IPC.
- CLI: path to `.tbc` (and flags as needed for modules path, help).
- Process exit status reflects successful halt vs load/runtime failure.

#### 2.2 Host surface (v1)

- **Console:** stdin / stdout / stderr (or equivalent stream hooks on the runtime).
- **Filesystem:** **unbound** — do not attach Pick [`FileSystem`](../../src/core/filesystem/FileSystem.h). Existing runtime behaviour when FS is null (“filesystem backend not configured”) is acceptable for console-only programs.
- **Language modules:** load optional shared modules (same ABI as [M11](11-multi-language-runtime-infrastructure.md) / [`language-modules.md`](../language-modules.md)) so Apollo/Pascal and BASIC **`CALL_FUNC`** work when present. See §2.5 for Pascal ownership.

#### 2.3 Hard non-Pick boundary

The runner **must not** include or require:

- Catalogue / `ACCOUNTS.json` / account roots
- Tcl, BASIC shell, PROC host, assembler debugger REPL
- [`GeminiSession`](../../src/userland/tcl/GeminiSession.h), [`SessionTable`](../../src/userland/tcl/SessionTable.h), cooperative session scheduler
- Daemon IPC, ports, Pick lock-session identity
- Boot monitor Pick cold-start banner as a product UX (internal reuse of loader code is fine if it does not imply Pick attachment)

#### 2.4 Review + spike approach

1. **Review** — document Pick vs portable boundaries in the current VM and plugin model.
2. **Spike** — run an Apollo-emitted console-only program (e.g. `hello.pas` / `count.pas` → `.tbc`) on `gemini-vm`, with a Pascal I/O module whose namespace/function IDs match Apollo codegen.
3. **Harden** — CMake target, tests, docs; keep Pick hosts untouched.

#### 2.5 Pascal language module ownership

Apollo’s v1 builtins are console only (`write` / `writeln` / `read` / `readln`). Codegen (Apollo Milestone 5) must emit **`CALL_FUNC`** against Gemini’s published module ABI ([`bytecode.md`](../bytecode.md), [`language-modules.md`](../language-modules.md)) — not Pick-specific opcodes. User `procedure` / `function` bodies remain ordinary `.tbc` code, not module entry points.

| Horizon | Where the Pascal I/O module lives |
|---------|-----------------------------------|
| **M19 spike** | Acceptable to implement a minimal Pascal console module **in this repo** (extend stub [`gemini-module-pascal`](../../modules/) or equivalent) so `gemini-vm` + Apollo hello/count can prove the path without waiting on Apollo packaging |
| **Steady state** | Build and ship the Pascal helper module with **apollo-compiler** against Gemini’s published ABI; install into the runner’s / Pick host’s modules path. Gemini remains **ABI + loader**, not the long-term catalogue of every outside language’s implementations |

Rationale (aligned with Apollo Milestone 6): keeping every front-end’s builtins forever inside gemini-system couples releases. Drop-in modules keep Pick/Gemini from needing each language at **build** time — only at **deploy** time (module on disk + matching IDs in bytecode).

If the spike lands here, schedule a follow-on to move ownership to Apollo (documented in Apollo M6 follow-on). Namespace/function IDs used by Apollo emission and the loaded module **must match**.

---

### 3. Non-goals

- Mercury / third-repo split of the VM (revisit only if packaging pain demands it)
- Rewriting the VM from scratch
- Pascal or Pick **file I/O** as a requirement for the first proof
- Owning Pascal builtins in gemini-system **permanently** (spike only; see §2.5)
- CPU-bound multi-session fairness ([**Milestone 21**](21-execution-fairness-cpu-bound-yield.md))
- R83 compatibility gap closure ([compatibility notes](../compatibility-r83-pick.md); planned after this milestone)
- Changing Application/Service edition semantics

---

### 4. Compatibility expectations

- **`gemini-system` / daemon / console:** no intentional behaviour change; regression barred by full test suite.
- **Runtime:** nullable filesystem and language-registry freeze remain the extension points; Pick hosts keep binding FS and session I/O as today.
- **Apollo:** success when an Apollo-compiled console-only Pascal program runs on `gemini-vm` with a matching Pascal I/O module loaded; Apollo does not reimplement a VM.

---

### 5. Dependencies and grounding

| Dependency | Role |
|------------|------|
| [M11](11-multi-language-runtime-infrastructure.md) | Language module ABI / loader |
| VM + `.tbc` | [`docs/vm.md`](../vm.md), [`Runtime`](../../src/core/vm/Runtime.h) |
| Module ABI docs | [`language-modules.md`](../language-modules.md), [`bytecode.md`](../bytecode.md) |
| [M18](18-version-1-gemini-system-service.md) | v1.0 baseline; editions remain Pick hosts |
| Apollo Compiler M5 / M6 | `.tbc` emission + acceptance program; Pascal module ownership policy |

---

### 6. Deliverables (anticipated)

| Area | Artifact |
|------|----------|
| Binary | `gemini-vm` (name finalized in implementation) under `src/` + CMake |
| Review note | [`docs/vm.md`](../vm.md) § Standalone runner boundary *(Stage 1)* |
| Pascal I/O (spike) | Published IDs in [`pascal_function_ids.hpp`](../../include/gemini/pascal_function_ids.hpp) *(Stage 1)*; handlers in Stage 3 |
| Tests | Load+run `.tbc` fixture; console I/O smoke; optional Apollo-emitted artifact in CI or documented manual step |
| Docs | Runner usage in `docs/vm.md` and/or README; cross-link Apollo M6 |

---

### 7. Milestone completion criteria

- [x] Review recorded: Pick vs portable boundaries for the current VM
- [ ] **`gemini-vm`** (or chosen name) builds and runs a `.tbc` with console I/O only, **no** catalogue/Tcl/session table/Pick FS
- [ ] Pascal I/O module available (Gemini spike **or** Apollo-built) with namespace/function IDs matching Apollo Milestone 5 emission
- [ ] An Apollo-compiled console-only Pascal program runs on that target (automated or documented manual smoke)
- [ ] Full existing `ctest` green; Application Edition smoke unchanged
- [ ] Operator/developer docs describe how to invoke the standalone runner
- [ ] Milestone hub / README list M19 implemented when closed

---

### 8. Delivery plan

M19 is sequenced as **review → spike runner → modules + Apollo proof → docs + close**. Each stage has an exit criterion; the full test suite must stay green. Only Stage 4 claims "Closes Milestone 19."

- **Stage 1 — Review**: inventory Pick coupling in VM entry paths; write boundary notes in [`docs/vm.md`](../vm.md); publish Pascal namespace **`3`** console function IDs for Apollo M5. **Exit criterion:** boundary section committed; IDs in header + schema + [`bytecode.md`](../bytecode.md); gaps and Stage 2 sketch recorded below. *Status: implemented.* Ships [`docs/vm.md`](../vm.md) § Standalone runner boundary; [`include/gemini/pascal_function_ids.hpp`](../../include/gemini/pascal_function_ids.hpp); updated [`bytecode.md`](../bytecode.md), [`language-namespaces.json`](../schemas/language-namespaces.json), [`language-modules.md`](../language-modules.md); §8.1 findings.

- **Stage 2 — Spike runner**: minimal **`gemini-vm`** executable — parse `.tbc`, run with stdout, FS unset; fixture test. *Status: planned.*

- **Stage 3 — Modules + Apollo proof**: Pascal console module (spike in-tree acceptable); load modules; run Apollo-emitted console program. *Status: planned.*

- **Stage 4 — Docs + closes M19**: runner usage notes; ownership handoff if spike stayed in-tree; hub status; full suite green. **Closes Milestone 19.** *Status: planned.*

#### 8.1 Stage 1 findings (gaps and Stage 2 sketch)

**Gaps tracked into Stages 2–3:**

1. No host CLI for `.tbc` file paths — only Pick-record `RUN` and test `parseFile`.
2. [`programs/hello.tbc`](../../programs/hello.tbc) is sample/docs only — good Stage 2 fixture candidate.
3. [`LanguageModuleLoader`](../../src/core/languages/LanguageModuleLoader.cpp) accepts `hostContext` at load time but does not pass it to handlers at registration; `CALL_FUNC` dispatch passes `Runtime*` at call time — sufficient for console module handlers.
4. [`Runtime.h`](../../src/core/vm/Runtime.h) includes [`FileSystem.h`](../../src/core/filesystem/FileSystem.h) (header coupling even when FS unset) — note only; not fixed in Stage 1.
5. Apollo-compiled proof deferred until Apollo M5 emission + Stage 3 module handlers.
6. [`gemini-module-pascal`](../../modules/gemini-pascal/PascalModule.cpp) registers namespace **`3`** but has no handler table yet — IDs published; implementation in Stage 3.

**Apollo coordination:** Apollo Milestone 5 codegen should import IDs from [`include/gemini/pascal_function_ids.hpp`](../../include/gemini/pascal_function_ids.hpp) and emit **`CALL_FUNC 3, <fn-id>, <arg-count>`** per [`bytecode.md`](../bytecode.md) § Pascal namespace. Variadic `write`/`writeln` source calls compile to one **`CALL_FUNC` per argument**.

**Minimal Stage 2 sketch:**

```text
gemini-vm program.tbc [--modules PATH]
  registry = LanguageRegistry::instance()
  loadLanguageModules(registry, modulesPath or GEMINI_MODULES_PATH or ./gemini/modules)
  Runtime rt; rt.setLanguageRegistry(&registry);  // FS stays null
  loaded = Parser().parseFile(argv[1])
  rt.loadProgram(loaded.program, loaded.sourceLinePerInstr)
  rt.run()
  exit 0 on clean halt; exit 1 on exception
```

No `BootMonitor` banner UX, no catalogue, no `GeminiSession`, no `applyDefaultFileSystemRoot`.

---

### 9. Follow-on (beyond M19)

- Move Pascal I/O module ownership to **apollo-compiler** if the M19 spike lived in gemini-system (steady-state drop-in module)
- Optional host filesystem façade (non-Pick) for BASIC/Pascal file I/O
- R83 compatibility gap work (within reason) — see [`compatibility-r83-pick.md`](../compatibility-r83-pick.md)
- CPU-bound cooperative yield — [**Milestone 21**](21-execution-fairness-cpu-bound-yield.md)

*Status: planned.*
