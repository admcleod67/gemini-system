← [Project milestones index](../milestones.md)

## Milestone 22 — Host Filesystem Façade

Ship a **host-agnostic filesystem façade** bindable from the standalone runner and (later) Pick-shaped hosts, so compiled languages can target portable file I/O without hard-coding POSIX paths or VOC/Pick logical files. Unblocks Apollo Compiler **Milestone 8 Stage 3** (`file` / `text` dialect work) after console/numeric and shared math tracks. *Status: planned.*

Depends on [**Milestone 19**](19-standalone-vm-runner.md) (`gemini-vm` with unbound Pick `FileSystem`). Follows [**Milestone 21**](21-shared-math-language-module.md) in delivery priority. R83 gaps are [**Milestone 23**](../compatibility-r83-pick.md) (detail page TBD); CPU fairness is deferred [**Milestone 24**](24-execution-fairness-cpu-bound-yield.md).

**Standing invariant:** Pick [`FileSystem`](../../src/core/filesystem/FileSystem.h) semantics for Application/Service editions must not regress. Existing BASIC `OPEN`/`READ`/`WRITE` paths that use the Pick backend stay byte-identical unless an explicit adapter stage is scoped and tested. Full `ctest` green after every stage.

Consumer context: [`docs/apollo-consumer-notes.md`](../apollo-consumer-notes.md) §5 (host filesystem and `file` I/O).

---

### 1. Purpose and rationale

[`gemini-vm`](19-standalone-vm-runner.md) intentionally leaves the Pick filesystem **unbound** so console-only programs run without catalogue or VOC. Pascal (and other non-Pick languages) still need **open / read / write / close** against host paths. Baking POSIX calls into Apollo-emitted bytecode (or into a Pascal-only module) would fork the ABI and block Pick hosts from sharing the same emitter contract.

A **façade** is the shared contract: standalone binds a host-path backend; Pick hosts may later bind an adapter over logical files. Front-ends emit against the façade (opcodes and/or `CALL_FUNC`), not against a specific OS API.

---

### 2. Scope

#### 2.1 In scope (v1 — thin façade)

| Area | Intent |
|------|--------|
| **Interface** | Abstract host FS operations sufficient for sequential **text**/byte stream I/O: open (read/write/create), read, write, close, and a small stable error model |
| **Standalone backend** | Default POSIX (or portable C++ filesystem) backend wired from `gemini-vm` when enabled |
| **Binding** | Runtime or host hook so the façade is injectable (nullable when unbound — console-only programs still run) |
| **Emitter surface** | **`CALL_FUNC` namespace `hostfs` (id 7)** — locked in Stage 1; no new core opcodes |
| **Tests + docs** | Unit/integration tests on standalone; `vm.md` / `language-modules.md` / consumer-notes; hub |

Exact operation names, handle representation, and path string rules are locked below (Stage 1).

#### 2.1a Design lock (Stage 1)

| Topic | Lock |
|-------|------|
| Emitter surface | **`CALL_FUNC` module only** — no new core opcodes |
| Namespace | name **`hostfs`**, id **`7`** / `0x00000007` |
| Module | **`gemini-module-hostfs`** (`modules/gemini-hostfs/`) |
| Handles | Positive **`int`** stack values (1-based dense ids); `0` invalid |
| Binding | Injectable via language-module **`hostContext`** (pointer to façade); **nullable** — unbound → `HOSTFS: unbound` when handlers exist (Stage 2) |
| Paths | `std::string` host path; empty rejected; absolute OK; relative → process cwd (optional sandbox root is Stage 2 detail, not VOC) |
| Errors | Stable **`HOSTFS:`** prefix (distinct from Pick `OPEN:`/`READ:`, `MATH:`, `LANG:`) |
| Contract header | [`include/gemini/HostFilesystem.hpp`](../../include/gemini/HostFilesystem.hpp); function IDs in [`include/gemini/hostfs_function_ids.hpp`](../../include/gemini/hostfs_function_ids.hpp) |

**Function table (v1)** — sequential byte/text streams. Line / `eof` / `eoln` stay in language layers (Apollo/Pascal follow-on).

| ID | Name | Arity | Stack (top last) | Result |
|----|------|-------|------------------|--------|
| 0 | `OpenRead` | 1 | path | handle `int` |
| 1 | `OpenWrite` | 1 | path | handle `int` (create/truncate) |
| 2 | `OpenAppend` | 1 | path | handle `int` (create if missing) |
| 3 | `Read` | 2 | handle, maxBytes | `string` (empty at EOF) |
| 4 | `Write` | 2 | handle, data | `int` bytes written |
| 5 | `Close` | 1 | handle | `int` `0` |

Locked error strings for Stage 2 handlers: `HOSTFS: unbound`, `HOSTFS: invalid path`, `HOSTFS: not found`, `HOSTFS: open failed`, `HOSTFS: bad handle`, `HOSTFS: read failed`, `HOSTFS: write failed`, `HOSTFS: close failed`.

#### 2.2 Hard non-goals (v1)

- Full **Pick VOC / MD / account-root** mapping inside the façade (may be a later adapter stage)
- Replacing or rewriting BASIC Pick `OPEN_FILE` / `READ_REC` / lock opcodes
- Pascal dialect **`eof` / `eoln` / field-width** formatting (Pascal language module follow-on once façade exists)
- Shared math ([**Milestone 21**](21-shared-math-language-module.md)), R83 ([**Milestone 23**](../compatibility-r83-pick.md)), fairness ([**Milestone 24**](24-execution-fairness-cpu-bound-yield.md))
- Requiring Apollo to emit façade calls in the same Gemini release

---

### 3. Compatibility

- **Standalone:** programs that never touch the façade behave as today with FS unbound.
- **Pick hosts:** continue using existing `FileSystem` binding; façade adapter is optional follow-on, not a silent redirect of BASIC record I/O.
- **Apollo:** success when Gemini documents the contract and standalone tests prove open/read/write/close; Apollo M8 Stage 3 may then design Pascal `file`/`text` lowering.

---

### 4. Deliverables

| Area | Artifact |
|------|----------|
| Contract | Header + docs for façade API and error strings |
| Backend | Standalone host-path implementation; `gemini-vm` wiring |
| Emitter surface | Published IDs (module) and/or opcode rows in [`vm.md`](../vm.md) |
| Tests | Temp-dir open/read/write/close; unbound still safe |
| Docs | Consumer-notes §5 status; hub; runner boundary notes |

---

### 5. Milestone completion criteria

- [ ] Façade interface + standalone backend implemented and tested
- [ ] Documented emitter surface (`CALL_FUNC` and/or opcodes) with stable errors
- [ ] Pick Application/Service filesystem tests unchanged in intent; full `ctest` green
- [ ] Consumer notes / hub list M22 implemented when closed

---

### 6. Suggested implementation stages

1. **Design lock** — operations, handles, path rules, error prefixes, module vs opcode choice; publish IDs + empty module + abstract contract header. *Status: implemented.*
2. **Standalone backend + binding** — `gemini-vm` can enable host FS; unit tests. *Status: planned.*
3. **Emitter surface + docs** — bytecode/language-modules/consumer-notes, hub. **Closes Milestone 22.** *Status: planned.*

Only Stage 3 claims “Closes Milestone 22.” Optional **Pick adapter** is explicitly follow-on unless pulled into a later stage of this page.

---

### 7. Follow-on (beyond M22)

- Pick-shaped façade adapter (logical files / account roots) without breaking BASIC record opcodes
- Pascal module `eof`/`eoln` / text helpers on top of the façade
- Apollo M8 Stage 3 dialect binding (compiler-side)
- R83 compatibility — [**Milestone 23**](../compatibility-r83-pick.md) (detail page TBD)
- CPU-bound cooperative yield — [**Milestone 24**](24-execution-fairness-cpu-bound-yield.md)

*Status: planned.*
