← [Project milestones index](../milestones.md)

## Milestone 19 — Standalone VM Runner

Ship a **Pick-independent** host-native bytecode runner: a separate executable that loads Gemini `.tbc` (and optional language modules), wires console I/O, and runs until halt — without catalogue, Tcl, session table, or Pick filesystem. Unblocks sister-project **apollo-compiler** Milestone 6 (standalone VM execution) while leaving Application and Service editions unchanged. *Status: implemented.*

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

- **`gemini-vm`** executable linking `gemini-core` (VM, parser, language registry/loader) **without** requiring Tcl shell, `GeminiSession` / session table, catalogue login, or daemon IPC.
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
2. **Spike** — run an Apollo-emitted console-only program (e.g. `hello.pas` / `count.pas` → `.tbc`) on `gemini-vm`. Apollo M5's bootstrap binding emits core `PRINT_*` / `INPUT_*` opcodes, so the first proof does not require a Pascal module.
3. **Harden** — CMake target, tests, docs; keep Pick hosts untouched.

#### 2.5 Pascal language module ownership

Apollo’s v1 builtins are console only (`write` / `writeln` / `read` / `readln`). Apollo Milestone 5's bootstrap binding emits Gemini core **`PRINT_*` / `INPUT_*`** opcodes, which was sufficient to complete the standalone proof without a language module. User `procedure` / `function` bodies remain ordinary `.tbc` code, not module entry points.

| Horizon | Where the Pascal I/O module lives |
|---------|-----------------------------------|
| **M19 proof (shipped)** | No module required: Apollo's `hello.pas` and `count.pas` compile to core console opcodes and run directly on `gemini-vm` |
| **Future `CALL_FUNC` binding** | Namespace **3** and console function IDs are published; the in-tree [`gemini-module-pascal`](../../modules/gemini-pascal/) remains registration-only |
| **Steady state** | Build and ship the Pascal helper module with **apollo-compiler** against Gemini’s published ABI; install into the runner’s / Pick host’s modules path. Gemini remains **ABI + loader**, not the long-term catalogue of every outside language’s implementations |

Rationale (aligned with Apollo Milestone 6): the bootstrap binding proves portability now; a later drop-in module can provide language-specific semantics without coupling Gemini and Apollo releases. When Apollo adopts the module binding, emitted namespace/function IDs and the loaded module **must match**.

---

### 3. Non-goals

- Mercury / third-repo split of the VM (revisit only if packaging pain demands it)
- Rewriting the VM from scratch
- Pascal or Pick **file I/O** as a requirement for the first proof
- Implementing the future Pascal `CALL_FUNC` helper module (see §2.5)
- CPU-bound multi-session fairness ([**Milestone 21**](21-execution-fairness-cpu-bound-yield.md))
- R83 compatibility gap closure ([compatibility notes](../compatibility-r83-pick.md); planned after this milestone)
- Changing Application/Service edition semantics

---

### 4. Compatibility expectations

- **`gemini-system` / daemon / console:** no intentional behaviour change; regression barred by full test suite.
- **Runtime:** nullable filesystem and language-registry freeze remain the extension points; Pick hosts keep binding FS and session I/O as today.
- **Apollo:** success when an Apollo-compiled console-only Pascal program runs on `gemini-vm`; the completed bootstrap path uses core console opcodes and does not require a module. Apollo does not reimplement a VM.

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

### 6. Delivered artifacts

| Area | Artifact |
|------|----------|
| Binary | **`gemini-vm`** ([`src/vm/Main.cpp`](../../src/vm/Main.cpp)) *(Stage 2)* |
| Review note | [`docs/vm.md`](../vm.md) § Standalone runner boundary *(Stage 1)* |
| Pascal console binding | Apollo M5 core `PRINT_*` / `INPUT_*` bootstrap binding; published future module IDs in [`pascal_function_ids.hpp`](../../include/gemini/pascal_function_ids.hpp) |
| Tests | Load+run `.tbc` fixture; `gemini-vm-smoke`; documented Apollo `hello.pas` / `count.pas` acceptance |
| Docs | Runner usage and Apollo workflow in [`docs/vm.md`](../vm.md); README/hub status |

---

### 7. Milestone completion criteria

- [x] Review recorded: Pick vs portable boundaries for the current VM
- [x] **`gemini-vm`** (or chosen name) builds and runs a `.tbc` with console I/O only, **no** catalogue/Tcl/session table/Pick FS
- [x] Apollo console binding available: core `PRINT_*` / `INPUT_*` bootstrap path shipped; namespace **3** IDs reserved for a future `CALL_FUNC` module
- [x] Apollo-compiled `examples/hello.pas` and `examples/count.pas` run successfully on `gemini-vm` (documented manual smoke)
- [x] Full existing `ctest` green; Application Edition smoke unchanged
- [x] Operator/developer docs describe how to invoke the standalone runner
- [x] Milestone hub / README list M19 implemented

---

### 8. Delivery plan

M19 is sequenced as **review → spike runner → Apollo proof → docs + close**. Each stage has an exit criterion; the full test suite must stay green. Only Stage 4 claims "Closes Milestone 19."

- **Stage 1 — Review**: inventory Pick coupling in VM entry paths; write boundary notes in [`docs/vm.md`](../vm.md); publish Pascal namespace **`3`** console function IDs for Apollo M5. **Exit criterion:** boundary section committed; IDs in header + schema + [`bytecode.md`](../bytecode.md); gaps and Stage 2 sketch recorded below. *Status: implemented.* Ships [`docs/vm.md`](../vm.md) § Standalone runner boundary; [`include/gemini/pascal_function_ids.hpp`](../../include/gemini/pascal_function_ids.hpp); updated [`bytecode.md`](../bytecode.md), [`language-namespaces.json`](../schemas/language-namespaces.json), [`language-modules.md`](../language-modules.md); §8.1 findings.

- **Stage 2 — Spike runner**: minimal **`gemini-vm`** executable — parse `.tbc`, run with stdout, FS unset; optional module-dir load; fixture test. **Exit criterion:** binary builds and runs [`programs/hello.tbc`](../../programs/hello.tbc); CTest smoke + unit prove `Hello, world`; missing args/file exit `1` with `gemini-vm:` stderr; full `ctest` green. *Status: implemented.* Ships [`src/vm/Main.cpp`](../../src/vm/Main.cpp), CMake target **`gemini-vm`** (Application install), [`tests/core/test_StandaloneVm.cpp`](../../tests/core/test_StandaloneVm.cpp), `gemini-vm-smoke`, usage note in [`docs/vm.md`](../vm.md).

- **Stage 3 — Apollo proof**: run Apollo-emitted console programs on `gemini-vm`; confirm whether the bootstrap proof requires a module. **Exit criterion:** Apollo `hello.pas` and `count.pas` compile and run successfully outside Pick. *Status: implemented.* Apollo M6 records the verified pipeline (`apolloc --emit … | gemini-vm /dev/stdin` and portable intermediate-file form); output is `Hello, Gemini!` and integers 1–10. The programs use core console opcodes, so no module is required.

- **Stage 4 — Docs + closes M19**: publish runner/Apollo usage; move Pascal `CALL_FUNC` handlers to follow-on; update README, docs hub, milestone hub, and changelog; run the full suite and Application install smoke. **Closes Milestone 19.** *Status: implemented.* Verification: **1023/1023** CTest cases passed; Runtime + Application component install contained `gemini-system` and `gemini-vm` (no Service binaries); installed `gemini-system` smoke and `gemini-vm programs/hello.tbc` passed.

#### 8.1 Stage 1 findings (gaps and Stage 2 sketch)

**Gaps tracked into Stages 2–3:**

1. ~~No host CLI for `.tbc` file paths~~ — resolved in Stage 2 (`gemini-vm`).
2. [`programs/hello.tbc`](../../programs/hello.tbc) used as Stage 2 fixture (`gemini-vm-smoke` + unit test).
3. [`LanguageModuleLoader`](../../src/core/languages/LanguageModuleLoader.cpp) accepts `hostContext` at load time but does not pass it to handlers at registration; `CALL_FUNC` dispatch passes `Runtime*` at call time — tracked with the future module binding.
4. [`Runtime.h`](../../src/core/vm/Runtime.h) includes [`FileSystem.h`](../../src/core/filesystem/FileSystem.h) (header coupling even when FS unset) — note only; not fixed in Stage 1.
5. ~~Apollo-compiled proof deferred~~ — completed with Apollo M5 output in Stage 3.
6. [`gemini-module-pascal`](../../modules/gemini-pascal/PascalModule.cpp) registers namespace **`3`** but has no handler table — IDs are published and handler implementation is follow-on work, not an M19 blocker.

**Apollo coordination:** M5 currently emits core console opcodes. A later module binding should import IDs from [`include/gemini/pascal_function_ids.hpp`](../../include/gemini/pascal_function_ids.hpp) and emit **`CALL_FUNC 3, <fn-id>, <arg-count>`** per [`bytecode.md`](../bytecode.md) § Pascal namespace.

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

- Implement the Pascal namespace **3** console handlers and switch Apollo from core console opcodes to `CALL_FUNC`; steady-state module ownership belongs in **apollo-compiler**
- Optional host filesystem façade (non-Pick) for BASIC/Pascal file I/O
- R83 compatibility gap work (within reason) — see [`compatibility-r83-pick.md`](../compatibility-r83-pick.md)
- CPU-bound cooperative yield — [**Milestone 21**](21-execution-fairness-cpu-bound-yield.md)

*Status: implemented.*
