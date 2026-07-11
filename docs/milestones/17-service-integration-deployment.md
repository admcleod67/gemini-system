← [Project milestones index](../milestones.md)

## Milestone 17 — Service Integration & Deployment

Make Gemini a first-class **Linux service**. Add a **systemd** unit (`gemini.service`) with proper lifecycle integration, **journald** logging for daemon output, and a **configuration file** for socket paths, session limits, and host settings. Expose **admin Tcl commands** such as **`LISTSESSIONS`**, **`KILLSESSION`**, **`STATUS`**, and Pick-style **`SHUTDOWN`**. Harden **graceful shutdown** and ship **install packaging** that separates the **Service Edition** (`gemini-daemon` + `gemini-console`) from the **Application Edition** (`gemini-system`). *Status: planned.*

M13–M15 delivered a multi-session daemon with IPC consoles and cooperative I/O yield. Operators still start `gemini-daemon` by hand in the foreground; there is no systemd unit, no admin session introspection beyond **`WHO`**, and no install layout that distinguishes service vs application deliverables. File-backed daemon config landed in Stage 1; remaining M17 work closes systemd, admin Tcl, and packaging so Gemini can run as a UniData-style Linux service ahead of the [**Milestone 18**](18-version-1-gemini-system-service.md) Version 1.0 release.

---

### 1. Purpose and rationale

Today’s service path works for development:

- [`gemini-daemon`](../../src/daemon/Main.cpp) starts in the foreground, installs **SIGTERM** / **SIGINT** handlers, cold-starts, and polls IPC ([`GeminiDaemonRunner`](../../src/userland/tcl/GeminiDaemonRunner.cpp))
- [`DaemonConfig`](../../src/core/daemon/DaemonConfig.h) resolves socket path, `maxSessions`, and host roots from **CLI and environment** only
- Binaries install via CMake `install(TARGETS …)` — no unit file, no config defaults under `/etc`, no bootstrap tree packaging
- Session operators see **`WHO`**; there is no daemon-wide **`LISTSESSIONS`** / **`KILLSESSION`** / **`STATUS`**

That is enough for Milestone 14–15 smoke, but not for production Linux deployment. Operators expect:

1. **`systemctl start gemini`** — daemon managed by systemd
2. Boot and runtime lines in **journald**
3. A **config file** (with CLI/env override) for socket, session caps, catalogue/pick roots
4. Admin visibility and control over attached sessions (especially runaway CPU-bound programs until [**Milestone 19**](19-execution-fairness-cpu-bound-yield.md))
5. A Pick-authentic **`SHUTDOWN`** verb that stops the whole system/daemon from a privileged Tcl session (not merely **`LOGOFF`** / **`QUIT`**)
6. Clear **Service Edition** vs **Application Edition** install stories (residual from retired [**Milestone 16**](16-standalone-edition-application-mode.md))

The design constraint from M12–M15 carries forward: **`gemini-system` external behaviour must not regress**; M17 is deployment and admin surface on the existing architecture, not a new session or IPC model.

---

### 2. Scope

#### 2.1 Configuration file

Extend [`DaemonConfig`](../../src/core/daemon/DaemonConfig.h) / [`resolveDaemonConfig`](../../src/core/daemon/DaemonConfig.cpp) with a **file-backed** config layer:

| Setting | Purpose |
|---------|---------|
| Socket path | Unix domain socket for `gemini-console` |
| `maxSessions` | Session table capacity |
| Catalogue / pick / modules roots | Same host paths CLI already supports |
| Optional log verbosity / identity | Enough for journald-friendly boot lines |

**Resolution order (M17 v1):** CLI overrides environment overrides config file overrides built-in defaults. Config format is **`key=value`** (Stage 1). Load a file only via `--config PATH` or `GEMINI_DAEMON_CONFIG` — auto-load of `/etc/gemini/daemon.conf` / XDG is deferred to later packaging stages.

**`gemini-console`** may gain a matching config for default socket path (optional stretch); daemon config is required.

#### 2.2 journald / logging hygiene

- Daemon **stdout** / **stderr** remain the primary log channel (systemd `StandardOutput=journal` / `StandardError=journal`)
- Ensure cold-start banner and M11 **`MODULES:`** lines flush promptly and remain journal-readable
- Avoid interactive assumptions when stdin is not a TTY (daemon under systemd has no console)
- Optional: structured prefix or syslog-style severity on diagnostic lines — stretch if not needed for exit criteria

#### 2.3 systemd unit

Ship **`gemini.service`** (and install rules) that:

- Runs **`gemini-daemon`** as a long-lived service (`Type=simple` or `notify` if readiness is implemented)
- Loads config from the documented path (or `EnvironmentFile=`)
- Passes **SIGTERM** for graceful shutdown (already handled by [`GeminiDaemonRunner`](../../src/userland/tcl/GeminiDaemonRunner.cpp))
- Documents socket path and permissions (owner-only **0600** socket, as today)
- Supports `systemctl start|stop|restart|status gemini`

Not in M17: socket-activated systemd units (`gemini.socket`) unless Stage 3 proves trivial; prefer file-based Unix socket as today.

#### 2.4 Admin Tcl surface

Expose daemon/session introspection and control from a logged-in Tcl REPL (typically **SYSPROG** or documented privilege — decide privilege gate in Stage 4):

| Command | Behaviour |
|---------|-----------|
| **`LISTSESSIONS`** | List session ports, attach state (bound console / detached), login account if any, run state (`Runnable` / `Running` / `WaitingForInput` when available) |
| **`STATUS`** | Daemon summary: `maxSessions`, session count, socket path (if known), build/version, optional lock-table summary |
| **`KILLSESSION` *port*** | Destroy session: unbind console if attached, retire scheduler state, release locks ([Milestone 10](10-record-locking-multi-user.md)), free port via [`PortManager`](../../src/core/daemon/PortManager.h) |
| **`SHUTDOWN`** | Stop the **whole system** — same graceful teardown as **SIGTERM** / IPC **`ShutdownRequest`**: detach all consoles, destroy all sessions, release locks, unlink socket, exit the daemon process. On embedded **`gemini-system`**, exit the process cleanly after session teardown |

Extend **`SYSTEM`** introspection from [**Milestone 11**](11-multi-language-runtime-infrastructure.md) (`SYSTEM LANGUAGES`) toward service state — either as **`SYSTEM STATUS`** / **`SYSTEM SHUTDOWN`** aliases or dedicated top-level commands. Prefer **top-level admin verbs** for Pick familiarity; **`SYSTEM …`** may mirror them.

**Privilege:** M17 v1 may restrict admin commands to a documented account (e.g. **SYSPROG**) or always allow them in daemon sessions — lock the rule in Stage 4 tests. **`SHUTDOWN`** and **`KILLSESSION`** should use the stricter gate. Embedded **`gemini-system`** (`maxSessions = 1`) should still accept **`STATUS`** / **`LISTSESSIONS`** sensibly (one row / trivial summary) and **`SHUTDOWN`** as process exit.

**Not the same as:**

| Verb | Scope |
|------|-------|
| **`LOGOFF`** | End this user’s login; return to **`LOGON PLEASE:`** (session object may remain) |
| **`QUIT`** | End this session’s REPL / worker path |
| **`KILLSESSION`** | Destroy **one** session by port |
| **`SHUTDOWN`** | Stop the **daemon / process** (all sessions) |

#### 2.5 Graceful shutdown hardening

M13 already shuts down on **SIGTERM**: detach bound sessions, stop IPC (unlink socket), destroy sessions (release locks and ports). M17:

- Verify and document that **filesystem state** is consistent after shutdown (no corrupted host-backed files from mid-write — use existing flush points; add explicit flush if gaps appear)
- Ensure **`KILLSESSION`**, Tcl **`SHUTDOWN`**, IPC **`ShutdownRequest`**, and daemon **SIGTERM** share the same teardown path where practical
- Wire Tcl **`SHUTDOWN`** to [`GeminiDaemonRunner::requestShutdown`](../../src/userland/tcl/GeminiDaemonRunner.h) (daemon) or process exit (embedded)
- **Cold restart = fresh sessions** (v1 decision): in-flight REPL/VM state is **not** restored across daemon restart. Document clearly; do not invent session persistence in M17 (see [**Milestone 18**](18-version-1-gemini-system-service.md) out-of-scope)

#### 2.6 Install packaging (Service vs Application)

Residual scope from retired [**Milestone 16**](16-standalone-edition-application-mode.md):

| Edition | Install contents |
|---------|------------------|
| **Service** | `gemini-daemon`, `gemini-console`, bootstrap/modules tree, `gemini.service`, default config file |
| **Application** | `gemini-system`, bootstrap/modules tree; **no** daemon unit or IPC socket requirement |

**`gemini-console`** requires a running daemon — connection failure is an **error**, not a fallback to embedded mode.

CMake / packaging may use install components, CPack components, or documented `cmake --install` targets — pick one approach in Stage 6.

#### 2.7 Embedded / standalone path

**`gemini-system`** remains the Application Edition: embedded GSD, `maxSessions = 1`, direct stdio. M17 must not change operator-visible login/REPL behaviour. Admin commands may appear with single-session semantics.

---

### 3. Architecture

#### 3.1 Deployment topology (after M17)

```text
  systemd (gemini.service)
           |
           v
    gemini-daemon  <--- config file + CLI/env overrides
           |
      Unix socket (0600)
           |
     +-----+-----+
     |           |
  console A   console B
     |           |
  LISTSESSIONS / KILLSESSION / STATUS / SHUTDOWN  (admin Tcl in privileged session)
```

#### 3.2 Config resolution

```text
  built-in defaults
        |
  config file  (e.g. /etc/gemini/daemon.conf)
        |
  environment  (GEMINI_DAEMON_SOCKET, …)
        |
  CLI flags    (--socket, --max-sessions, …)
        v
  DaemonConfig  →  GeminiDaemonRunner / GeminiServiceDaemon
```

#### 3.3 Session teardown paths

| Trigger | Effect |
|---------|--------|
| Console detach | Unbind I/O; session remains (M14) |
| **`KILLSESSION`** | Destroy **one** session; release locks and port; unbind console |
| Tcl **`SHUTDOWN`** | Request full daemon/process stop (same path as SIGTERM) |
| Daemon **SIGTERM** / IPC **ShutdownRequest** | Detach all, destroy all sessions, unlink socket |

---

### 4. Non-goals

Milestone 17 does **not** introduce:

- Remote access beyond local Unix domain sockets (SSH/telnet — post-1.0)
- Session restore / persistence across cold daemon restart
- CPU-bound opcode-budget yield or operator **BREAK** ([**Milestone 19**](19-execution-fairness-cpu-bound-yield.md))
- Preemptive threading or changes to cooperative I/O yield (M15)
- Hot-reload of language modules without daemon restart
- Full privilege / RBAC system beyond a minimal admin gate for kill/list/shutdown
- Host OS power-off / reboot (`poweroff`, `reboot`) — **`SHUTDOWN`** stops Gemini only; the Linux host remains up unless the operator/systemd handles that separately
- Windows service packaging
- Socket activation as a requirement (optional stretch only)

---

### 5. Compatibility expectations

- **`gemini-system`** manual smoke (§9) unchanged
- M14–M15 console attach/detach/cooperative prompts unchanged
- Existing CLI/env config continues to work; config file is additive
- Locking semantics unchanged ([Milestone 10](10-record-locking-multi-user.md)); **`KILLSESSION`** must release that session’s locks
- **`WHO`**, port policy, and session identity unchanged
- Operator-visible **additions**: systemd lifecycle, config file, admin Tcl verbs (**`LISTSESSIONS`**, **`STATUS`**, **`KILLSESSION`**, **`SHUTDOWN`**)

---

### 6. Dependencies and grounding

- Builds on [**Milestone 13**](13-service-daemon-architecture.md) — daemon lifecycle, [`DaemonConfig`](../../src/core/daemon/DaemonConfig.h), SIGTERM shutdown, IPC
- Builds on [**Milestone 14**](14-multi-session-console-support.md) — multi-console attach, graceful detach
- Builds on [**Milestone 15**](15-cooperative-multi-session-execution.md) — session run states for **`LISTSESSIONS`**
- Builds on [**Milestone 11**](11-multi-language-runtime-infrastructure.md) — **`SYSTEM LANGUAGES`** introspection direction
- Residual packaging from [**Milestone 16**](16-standalone-edition-application-mode.md) (superseded)
- Feeds [**Milestone 18**](18-version-1-gemini-system-service.md) — Version 1.0 packaging and admin docs

---

### 7. Deliverables

**Code / packaging (anticipated):**

| Area | Path / artifact |
|------|-----------------|
| Config file parser + resolution | [`DaemonConfig`](../../src/core/daemon/DaemonConfig.h) / `.cpp` |
| systemd unit | e.g. `packaging/systemd/gemini.service` (exact path TBD) |
| Default config sample | e.g. `packaging/gemini/daemon.conf.example` |
| Admin Tcl | [`Shell.cpp`](../../src/userland/tcl/Shell.cpp) + host/session-table hooks |
| **`KILLSESSION`** / **`SHUTDOWN`** teardown | [`GeminiSessionHost`](../../src/userland/tcl/GeminiSessionHost.h) / [`GeminiDaemonRunner`](../../src/userland/tcl/GeminiDaemonRunner.cpp) |
| Install components | [`src/CMakeLists.txt`](../../src/CMakeLists.txt), packaging rules |

**Tests:**

- Config file parsing and override precedence
- Admin command unit/integration tests (`LISTSESSIONS`, `STATUS`, `KILLSESSION`, `SHUTDOWN`)
- Lifecycle: SIGTERM and Tcl **`SHUTDOWN`** share teardown; clean with config file + multi-session
- Optional: smoke script or documented `systemctl` checklist (CI may skip systemd-dependent tests)

**Documentation:**

- Extend [`docs/daemon.md`](../daemon.md) — config file, systemd, admin commands, cold-restart policy
- Update [`docs/console.md`](../console.md) — service prerequisites under systemd
- Admin / install notes (may live in daemon.md until M18 expands operator guides)
- Mark this milestone **implemented** at Stage 7 closure; update [`docs/milestones.md`](../milestones.md)

---

### 8. Open decisions (resolve during stages)

| Topic | Options | Default lean |
|-------|---------|--------------|
| Config file format | key=value vs INI sections | key=value, one setting per line |
| Default config path | `/etc/gemini/daemon.conf` vs XDG | `/etc/gemini/daemon.conf` for service; document XDG for user |
| Admin privilege | SYSPROG-only vs all logged-in sessions | SYSPROG-only for **`KILLSESSION`** / **`SHUTDOWN`**; list/status open or same gate |
| **`STATUS`** / **`SHUTDOWN`** vs **`SYSTEM …`** | Top-level vs SYSTEM subcommand | Top-level verbs; optional **`SYSTEM STATUS`** / **`SYSTEM SHUTDOWN`** aliases |
| **`SHUTDOWN`** confirmation | Immediate vs confirm prompt | Immediate for M17 v1 (document danger); optional confirm stretch |
| systemd Type | `simple` vs `notify` | `simple` unless readiness socket is easy |

---

### 9. Milestone completion criteria

- **Config file** loads daemon settings; CLI/env override documented and tested
- **`gemini.service`** starts/stops `gemini-daemon` cleanly under systemd (manual smoke on Linux)
- Boot / module lines appear in **journald** when run via the unit
- **`LISTSESSIONS`**, **`STATUS`**, **`KILLSESSION`**, and **`SHUTDOWN`** work from a privileged daemon-attached console; **`KILLSESSION`** releases locks and frees the port; **`SHUTDOWN`** stops the daemon via the same path as **SIGTERM**
- Graceful **SIGTERM** and Tcl **`SHUTDOWN`** remain clean with multiple sessions; cold restart documented as fresh sessions
- Embedded **`gemini-system`**: privileged **`SHUTDOWN`** exits the process cleanly (Application Edition)
- **Service** and **Application** install layouts documented (and CMake/packaging ships them)
- **`gemini-system`** passes manual smoke unchanged
- All existing tests pass; new tests cover config and admin commands
- Documented in [`docs/daemon.md`](../daemon.md)

**Manual smoke checklist — `gemini-system`** (must pass after every stage):

1. Boot with catalogue → interactive or auto LOGON
2. Tcl: `VERSION`, `WHO`
3. Enter BASIC → run a one-line program → `END`
4. `LOGOFF` → re-login
5. `QUIT` → clean process exit

**Manual smoke checklist — systemd service** (required for milestone closure on Linux):

1. Install unit + config pointing at a test catalogue/pick root and temp or documented socket
2. `systemctl start gemini` → daemon running; journal shows boot banner / `MODULES:`
3. Attach **two** `gemini-console` instances → login → **`WHO`** distinct ports
4. From one console: **`LISTSESSIONS`**, **`STATUS`**
5. From one console: **`KILLSESSION`** the other port → other console disconnects or errors cleanly; locks released
6. Re-attach a second console, then from SYSPROG: **`SHUTDOWN`** → daemon exits; socket removed; consoles disconnect; `systemctl` shows inactive (or restart → fresh sessions)
7. Alternate stop path: `systemctl stop gemini` → same clean teardown; restart → fresh sessions (no restore)

**Manual smoke checklist — Application Edition packaging:**

1. Install application component only → `gemini-system` runs without daemon
2. Confirm `gemini-console` is not required for application-only use

---

### 10. Delivery plan

Implementation is sequenced into vertical stages. Each stage ships a test-locked slice before the next starts; the full test suite must remain green after every stage. **`gemini-system` external behaviour must not regress** until Stage 7 claims milestone closure. Detailed per-stage plans may live in `~/.cursor/plans/m17_stage_*.plan.md` during implementation; status is summarised here as each stage lands (matching the M13–M15 precedent).

- **Stage 1 — Config file support**: extend [`DaemonConfig`](../../src/core/daemon/DaemonConfig.h) with file parsing and documented resolution order (CLI > env > file > defaults); sample config; unit tests for precedence and invalid keys. **Exit criterion:** `gemini-daemon --config path` (or equivalent) starts with file-backed socket/maxSessions/host paths; existing CLI/env tests still pass. *Status: implemented.* Ships `loadDaemonConfigFile`, `--config` / `GEMINI_DAEMON_CONFIG`, [`packaging/gemini/daemon.conf.example`](../../packaging/gemini/daemon.conf.example), and extended [`tests/core/test_DaemonConfig.cpp`](../../tests/core/test_DaemonConfig.cpp).

- **Stage 2 — Logging / journald readiness**: audit daemon stdout/stderr under non-TTY; ensure cold-start and **`MODULES:`** lines flush; document systemd `StandardOutput=journal` expectations; fix any interactive-only assumptions in the daemon entry path. **Exit criterion:** headless daemon start produces complete boot banner to a pipe/file; documented journald behaviour. *Status: planned.*

- **Stage 3 — systemd unit**: add `gemini.service` (+ install rules); wire `ExecStart` to installed `gemini-daemon` with config path; SIGTERM stop; document `systemctl` operator steps. **Exit criterion:** unit file present and installable; manual systemd smoke starts/stops daemon (CI may mark systemd tests as optional/manual). *Status: planned.*

- **Stage 4 — Admin LISTSESSIONS + STATUS**: implement Tcl **`LISTSESSIONS`** and **`STATUS`** (and optional **`SYSTEM`** aliases) using [`SessionTable`](../../src/userland/tcl/SessionTable.h) / [`CooperativeSessionRunner`](../../src/core/daemon/CooperativeSessionRunner.h) state; privilege gate decided and tested. **Exit criterion:** two-console integration test shows both ports in **`LISTSESSIONS`**; **`STATUS`** reports capacity and count. *Status: planned.*

- **Stage 5 — KILLSESSION + SHUTDOWN + shared teardown**: implement **`KILLSESSION` *port*** (unbind console if any, `retireSession`, `destroySession`, release locks/ports) and Pick-style Tcl **`SHUTDOWN`** that calls the same graceful stop path as **SIGTERM** / IPC **`ShutdownRequest`** (`requestShutdown` on the daemon; process exit on embedded). Privilege-gate both destructive verbs. **Exit criterion:** kill one of two sessions while the other continues; locks held by killed session are gone; **`SHUTDOWN`** from a privileged console stops the daemon cleanly (integration test); embedded **`SHUTDOWN`** exits `gemini-system`. *Status: planned.*

- **Stage 6 — Install packaging (Service vs Application)**: CMake/CPack (or equivalent) components — service ships daemon+console+unit+config; application ships `gemini-system`+bootstrap/modules; document no console failover. **Exit criterion:** documented install recipes; packaging files in tree; smoke checklist §9 Application Edition passes. *Status: planned.*

- **Stage 7 — Docs + closes M17**: update [`docs/daemon.md`](../daemon.md), [`docs/console.md`](../console.md), [`docs/tcl-shell.md`](../tcl-shell.md); document **`SHUTDOWN`** vs **`LOGOFF`** / **`QUIT`** / **`KILLSESSION`**; cold-restart = fresh sessions; audit §9; full test suite + systemd and `gemini-system` smoke. **Closes Milestone 17.** *Status: planned.*

Only Stage 7's exit criteria should claim "Closes Milestone 17".

*Status: planned.*
