← [Project milestones index](../milestones.md)

## Milestone 18 — Version 1.0 Release: Gemini System Service

Deliver the first stable **Version 1.0** Gemini System: a Pick-authentic, multi-session **service edition** and a matching **application edition**, both on the same architecture. Ship reliable daemon-based multi-session operation, integrated language libraries (Tcl, BASIC, PROC, assembler shell), multi-attribute Pick filesystem semantics, and complete **architecture**, **admin**, and **developer** documentation. Define and meet public **release criteria** for the repository and deliverables. *Status: planned.*

[**Milestone 17**](17-service-integration-deployment.md) closed Linux service integration (config, journald, systemd, admin Tcl, install components). M18 is the **stabilization and documentation capstone** that declares Version 1.0 — not a bucket for new architecture. Avoid sneaking in telnet, SQL, distributed sessions, transaction semantics, or CPU-bound fairness; those belong in post-1.0 milestones ([**Milestone 19**](19-execution-fairness-cpu-bound-yield.md) and later).

---

### 1. Purpose and rationale

Milestones 1–17 built a working Pick-like platform and a UniData-style Linux service path. What remains for a credible **1.0** is not another subsystem, but a **release boundary**:

1. **Prove** what already ships — multi-session locks, cooperative I/O scheduling, language-module boot, both editions
2. **Document** architecture, admin, and developer surfaces as a coherent v1.0 story (Service vs Application; known limits)
3. **Hygiene** — version identity, milestone history, packaging recipes, public release checklist
4. **Tag** Version 1.0 when the checklist is green

**Version 1.0** means Pick-authentic multi-user **file and lock semantics** on Linux (local Unix-domain service deployment), not full R83 terminal/phantom fidelity. Gemini stays grounded in Pick authenticity while treating production Linux service deployment as a first-class deliverable alongside the single-process application host.

**Design constraint:** no new session model, IPC protocol, or scheduler behaviour in M18. Bug fixes and doc/test gaps that block the release checklist are in scope; feature work is not.

---

### 2. Scope

#### 2.1 Dual deliverables (already built — M18 documents and freezes them)

| Edition | Binaries | Hosting |
|---------|----------|---------|
| **Service** | **`gemini-daemon`** + **`gemini-console`** | Multi-session Linux deployment ([M13–M17](13-service-daemon-architecture.md)) |
| **Application** | **`gemini-system`** | Single-session embedded host on the same architecture ([M12–M13](12-session-model-foundation.md); [M16 retirement](16-standalone-edition-application-mode.md)) |

**No console failover:** `gemini-console` requires a running daemon; connection failure is an error, not a switch to embedded mode.

#### 2.2 Test matrix (audit + fill gaps)

Publish and satisfy a **v1.0 test matrix** covering at least:

- Multi-session **record locks** (M10 + daemon-attached sessions)
- **Cooperative scheduling** at I/O yield points (M15)
- **Language module** boot / registration (M11)
- Admin verbs and lifecycle (M17): **`LISTSESSIONS`**, **`STATUS`**, **`KILLSESSION`**, **`SHUTDOWN`**
- Install layout smoke for **Runtime** / **Application** / **Service** components (M17)

Prefer documenting which existing `ctest` cases map to each row; add tests **only** where the matrix has a real hole. Do not invent product features to “fill” coverage.

**v1.0 test matrix (Stage 2)** — representative `ctest` / doctest names (not every assertion):

| Area | Coverage | Verdict |
|------|----------|---------|
| Multi-session locks (in-process) | `LockTable cross-session…`, `FileSystem S1 readU blocks S2…`, shell/PROC/M10 cross-shell locking, `GeminiSession` detach/reset release | **OK** |
| Multi-session locks (daemon-attached) | `DaemonIpcClient two consoles READU conflict returns RECORD LOCKED`; `DaemonIpcClient SYSPROG KILLSESSION destroys peer and releases locks` | **OK** |
| Cooperative I/O yield | `CooperativeSessionRunner yield…` / round-robin / starvation; `Schedulable IpcSessionChannel…`; `DaemonIpcClient session blocked in BASIC INPUT allows other session Tcl WHO` | **OK** |
| Language module boot | `LanguageModuleLoader…`, `BootMonitor cold start loads language modules…`, `LanguageRegistry…`, daemon coldStart freezes registry | **OK** |
| Admin verbs | `test_AdminCommands` host cases; `DaemonIpcClient SYSPROG LISTSESSIONS and STATUS…`, `…KILLSESSION…`, `…SHUTDOWN…` | **OK** |
| Install Application / Service | `Application component install excludes service binaries`; `Service component install excludes gemini-system` (both via Runtime + edition) | **OK** |
| Daemon config / unit shape | `loadDaemonConfigFile…` / `resolveDaemonConfig…`; `systemd gemini.service unit…`; `packaging daemon.conf…` | **OK**; live `systemctl` = manual §9 |
| Manual smoke | [`daemon.md`](../daemon.md) M17 checklists (`gemini-system`, systemd, Application packaging) | **OK** — reuse for public checklist Stage 5 |

**Remaining gaps** (Stage 5):

| Gap | Owner stage |
|-----|-------------|
| `PROJECT_VERSION` → `1.0.0`; CHANGELOG `[Unreleased]` → `[1.0.0]`; annotated tag **`v1.0.0`**; milestone hub / README status flips | **Stage 5** |

Runtime-only install assertion: **deferred** (not required for 1.0; Application/Service install cases cover the matrix row).

#### 2.3 Documentation capstone

Bring operator and developer docs to a v1.0 bar:

- **Architecture** — session model, daemon/IPC, cooperative execution (existing [`session.md`](../session.md), [`daemon.md`](../daemon.md); tighten cross-links and edition framing)
- **Admin** — systemd, config, admin Tcl, session-end contrast, cold restart = fresh sessions (largely M17); edition install recipes; migration notes for operators moving from `gemini-system`-only to Service Edition
- **Developer** — Tcl / BASIC / PROC / ASM entry points, bytecode/module docs (existing tree under [`docs/`](../README.md)); README and docs hub reflect **1.0** status
- **Known limitations** — CPU-bound multi-session starvation (I/O yield only); cold restart does not restore sessions; local UDS only — point at [M19](19-execution-fairness-cpu-bound-yield.md) for fairness

Edition naming and residual operator-doc work parked from [M16](16-standalone-edition-application-mode.md) land here. **Stage 3** ships edition glossary, known-limitations trio, Application→Service migration, and docs hub accuracy in operator docs.

#### 2.4 Repository hygiene and release packaging

- Align **`PROJECT_VERSION`** / annotated git tag with the public **1.0** identity (today: `0.18.0` in root [`CMakeLists.txt`](../../CMakeLists.txt); bump to `1.0.0` at Stage 5)
- Milestone hub / README: M18 **implemented**; path-to-1.0 closed; M19 marked post–v1.0
- Confirm M17 CMake install components remain the packaging story (CPack deferred — see §8)
- **Public release checklist** (Stage 5) that an outsider can run: build, `ctest`, Application smoke, Service install smoke, systemd checklist on Linux

---

### 3. Architecture (freeze — no M18 changes)

M18 does **not** change process topology. For reference:

```text
Service Edition:
  systemd (gemini.service)
       └── gemini-daemon ── Unix socket ── gemini-console (N)
              └── GeminiServiceDaemon + CooperativeSessionRunner

Application Edition:
  gemini-system ── createEmbedded() ── one session, stdio (no IPC)
```

Session lifecycle, IPC v1, and cooperative I/O yield remain as documented after M15/M17. Cold restart continues to mean **fresh sessions** (no restore).

---

### 4. Non-goals

- New admin verbs, IPC messages, or scheduler/VM yield behaviour
- CPU-bound multi-session fairness / operator **BREAK** — [**Milestone 19**](19-execution-fairness-cpu-bound-yield.md) **after** v1.0 is tagged
- Remote access beyond local Unix domain sockets (SSH/telnet post-1.0)
- Hot-reload of language modules without daemon restart
- Session restore across cold daemon restart
- Transaction semantics, SQL gateways, or other post-Pick extensions
- Distro packaging beyond what M17 already ships (CPack / `.deb` / `.rpm` deferred post-1.0 — see §8)

---

### 5. Compatibility expectations

- **`gemini-system`** external behaviour must not regress (boot → logon → Tcl/BASIC → `LOGOFF` / `QUIT` / embedded **`SHUTDOWN`**)
- Service Edition operator paths from M17 (§9 / [`daemon.md`](../daemon.md) smoke) remain valid
- Public docs must state honestly what 1.0 is **not** (R83 terminal fidelity, CPU fairness, remote attach)

---

### 6. Dependencies and grounding

| Dependency | Role |
|------------|------|
| [M12–M15](12-session-model-foundation.md) | Session model, daemon, consoles, cooperative I/O yield |
| [M17](17-service-integration-deployment.md) | Config, systemd, admin Tcl, install components — **prerequisite complete** |
| [M16 residual](16-standalone-edition-application-mode.md) | Edition naming, migration notes, release checklist |
| [M19](19-execution-fairness-cpu-bound-yield.md) | **Blocked until** M18 tags v1.0; document CPU starvation as known limit until then |

---

### 7. Deliverables

**Documentation / process:**

| Area | Artifact |
|------|----------|
| v1.0 test matrix | Table in this milestone and/or [`docs/`](../README.md) pointing at `ctest` coverage |
| Edition + migration | README / [`daemon.md`](../daemon.md) / short admin notes: Service vs Application; no failover |
| Known limitations | Explicit in release docs + [`daemon.md`](../daemon.md) |
| Public release checklist | §9 + Stage 5; runnable by an operator |
| Changelog / release notes | [`CHANGELOG.md`](../../CHANGELOG.md) with 1.0 entry (Stage 4); annotated tags kept |
| Status flips | This page, [`milestones.md`](../milestones.md), [`README.md`](../../README.md), [`docs/README.md`](../README.md) |

**Code / packaging (minimal):**

- Version bump to **1.0.0** (CMake `PROJECT_VERSION` and tag) at Stage 5
- Tests only where the matrix audit finds holes (Stage 2)
- No required new runtime features

---

### 8. Decisions (resolved)

| Topic | Choice (Stage 1) |
|-------|------------------|
| Public version tag | **`v1.0.0`** — set CMake `PROJECT_VERSION` to `1.0.0` and annotate the tag at Stage 5; leave **`0.18.0`** until then |
| Changelog | Add **`CHANGELOG.md`** at Stage 4 with a 1.0 entry; keep annotated git tags |
| System title string | **Keep** `"Gemini System Developer Edition"` in [`version.hpp`](../../include/pick_system/version.hpp) for v1.0 |
| CPack / `.deb` / `.rpm` | **Defer** post-1.0 — M17 `cmake --install` components are the packaging story |
| Docs structure | **Expand existing** [`daemon.md`](../daemon.md) / [`console.md`](../console.md) / README — no parallel admin-guide tree |
| CI systemd | **Operator checklist** only (same as M17); do not require live systemd in CI |

---

### 9. Milestone completion criteria

- v1.0 **test matrix** published; every row mapped to automated tests and/or an explicit manual smoke step
- Full **`ctest`** green
- **Architecture / admin / developer** docs describe both editions, no console failover, cold restart, and CPU-bound known limitation
- **Public release checklist** executed (Application + Service; systemd on Linux where available)
- **`PROJECT_VERSION`** and annotated git tag **`v1.0.0`** (or the §8-resolved equivalent)
- Milestone hub / README show M18 **implemented** / completed; M19 remains planned post–v1.0
- No new architecture claimed as part of 1.0 beyond M17

**Manual smoke** — reuse M17 / [`daemon.md`](../daemon.md) checklists:

- `gemini-system` 5-step
- systemd Service Edition (two consoles, admin verbs, `SHUTDOWN` / `systemctl stop` → fresh sessions)
- Application / Service install-component recipes

**Public release checklist** (execute at Stage 5; draft locked here):

1. `cmake` build + full `ctest` green
2. Application Edition install smoke — Runtime + Application → `gemini-system` present; no daemon/console ([`daemon.md`](../daemon.md#install-packaging-service-vs-application))
3. Service Edition install smoke — Runtime + Service → daemon, console, `daemon.conf`, `gemini.service`
4. `gemini-system` 5-step manual smoke ([`daemon.md`](../daemon.md#manual-smoke-checklists))
5. Linux systemd checklist ([`daemon.md`](../daemon.md#manual-smoke-checklists)) — operator host; not required in CI
6. Set `PROJECT_VERSION` to **1.0.0**; retitle [`CHANGELOG.md`](../../CHANGELOG.md) `[Unreleased]` → `[1.0.0] - <date>`; annotated tag **`v1.0.0`**; flip this milestone, [`milestones.md`](../milestones.md), and README status

Operator smoke checklists also live under [`daemon.md` Manual smoke](../daemon.md#manual-smoke-checklists). The Version 1.0 public checklist above is authoritative for Stage 5.

---

### 10. Delivery plan

M18 is sequenced as **audit → fill gaps → docs → hygiene → release**. Each stage has an exit criterion; the full test suite must stay green. Do **not** start [Milestone 19](19-execution-fairness-cpu-bound-yield.md) until Stage 5 tags Version 1.0. Detailed per-stage plans may live in `~/.cursor/plans/m18_stage_*.plan.md` during implementation; status is summarised here as each stage lands.

- **Stage 1 — Release audit**: inventory existing tests and docs against §2.2–§2.4; list gaps only (no code yet unless a trivial doc typo blocks reading). Resolve §8 decisions. **Exit criterion:** written gap list + proposed matrix rows; open decisions table updated or deferred with owners. *Status: implemented.* Ships draft §2.2 test matrix, Stage 1–5 gap list, and §8 decisions (resolved) in this milestone page.

- **Stage 2 — Test matrix fill**: add or extend automated tests **only** for matrix holes (primary: daemon-attached two-console lock conflict; optional Runtime-only install); publish final matrix with `ctest` names for locks, cooperative yield, module boot, admin, and install layout. **Exit criterion:** matrix table complete with links/names; `ctest` green; no new product features. *Status: implemented.* Ships `DaemonIpcClient two consoles READU conflict returns RECORD LOCKED` in [`test_GeminiConsole.cpp`](../../tests/console/test_GeminiConsole.cpp); final §2.2 matrix; Runtime-only install deferred.

- **Stage 3 — Docs capstone**: edition naming/migration notes; hub accuracy ([`docs/README.md`](../README.md)); surface known limitations (CPU starvation → M19; cold restart; **local UDS**); tighten architecture/admin/developer cross-links. **Exit criterion:** docs review against §2.3; Application and Service operator stories readable without the milestone page. *Status: implemented.* Ships edition glossary, known-limitations trio, and Application→Service migration in [`docs/daemon.md`](../daemon.md); console/session edition wording; docs hub roadmap fix.

- **Stage 4 — Repo hygiene**: add `CHANGELOG.md` (1.0 entry draft), version bump preparation, packaging recipe verification, milestone/docs hub consistency for a post-1.0 world (except final status flip). **Exit criterion:** release notes draft ready; install recipes smoke-passed; hub prose accurate aside from M18 status flip. *Status: implemented.* Ships [`CHANGELOG.md`](../../CHANGELOG.md) (`Unreleased` 1.0 draft + `0.17.0`); public release checklist in §9; README/docs hub links; Application and Service `cmake --install` smoke to `/tmp/gemini-*-m18` passed (`PROJECT_VERSION` still `0.18.0`).

- **Stage 5 — Tag Version 1.0 + closes M18**: run public release checklist; set `PROJECT_VERSION` to **1.0.0**; annotated tag **`v1.0.0`**; flip this milestone, [`milestones.md`](../milestones.md), and README status. **Closes Milestone 18.** *Status: planned.*

Only Stage 5's exit criteria should claim "Closes Milestone 18".

*Status: planned.*
