← [Project milestones index](../milestones.md)

## Milestone 13 — Service Daemon Architecture

Introduce the **Gemini Service Daemon (GSD)** as a long-running process that owns the VM substrate, Pick filesystem, boot-time **language registry**, shared **lock table**, and a **session table**. Add a minimal **IPC layer** (Unix domain sockets) for future console attachment, plus daemon lifecycle (start, stop, configuration). Sessions may be created and held in the table, but only one runs at a time—no cooperative scheduling yet. Security is minimal: host file permissions and socket access control. *Status: planned.*

This milestone is the architectural hinge between the M12 session object and the multi-console service introduced in M14–M17. A correct daemon host makes later milestones largely mechanical; an incorrect one forces parallel execution paths.

---

### 1. Purpose and rationale

Before M13, process-scope assets (`BootMonitor`, frozen `LanguageRegistry`, shared `LockTable`) and session-scope assets (`GeminiSession`) are wired together ad hoc in [`Main.cpp`](../../src/Main.cpp). M13 introduces a single **process host** — the Gemini Service Daemon — that owns cold start and shared substrate, holds a **session table** of `GeminiSession` objects, and exposes a minimal IPC boundary for future console attachment.

M12 delivered the session boundary; M13 delivers the **service host** that will hold it. The design constraint from M12 carries forward: **standalone `gemini-system` is one session in the same model** (conceptually `maxSessions = 1`, in-process I/O), not a separate codebase. M13 adds a second entry point — **`gemini-daemon`** — on the same host implementation.

---

### 2. Scope

#### 2.1 Daemon object (`GeminiServiceDaemon`)

A process-scope host (name may be `GeminiServiceDaemon` or `GeminiDaemon` in code) that:

- Runs **cold start** once per process via [`BootMonitor`](../../src/core/boot/BootMonitor.h)
- Owns the frozen [`LanguageRegistry`](../../src/core/languages/LanguageRegistry.h) (loaded at boot per [Milestone 11](11-multi-language-runtime-infrastructure.md))
- Owns the shared [`LockTable`](../../src/core/locking/LockTable.h) (today via [`LockRegistry`](../../src/core/locking/LockRegistry.h) per [Milestone 10](10-record-locking-multi-user.md))
- Resolves catalogue / gemini root at daemon scope ([`HostBootstrap`](../../src/core/boot/HostBootstrap.h)); individual sessions vary **account context** only
- Holds a **session table** and **serial session runner**
- Hosts a **port manager** and minimal **IPC server**

**Defer hot-reload of language modules for v1.** M11 deliberately ruled out dynamic unload; “reload language libraries” means **restart the daemon**, not runtime module swap.

#### 2.2 Session table

- Create, hold, and destroy [`GeminiSession`](../../src/userland/tcl/GeminiSession.h) entries
- Drive the same lifecycle transitions M12 defined: `create` / `attach` / `detach` / `reset` / `destroy` (see [session.md](../session.md))
- Multiple session **objects** may exist simultaneously; see §2.4 for execution rules

#### 2.3 Port manager

Replace the boot stub **`PORT MANAGER: (stub)`** in [`BootMonitor.cpp`](../../src/core/boot/BootMonitor.cpp) with a real allocator:

- Assign stable **port / session id** at session create
- Release on session destroy
- Feed **`WHO`**, future **`LISTSESSIONS`**, and lock identity ([`GeminiSession::makeSessionLockId`](../../src/userland/tcl/GeminiSession.h))

Port assignment policy is owned by M13 (Stage 3); M14 consumes it when consoles attach.

#### 2.4 Serial session runner

Only **one** session executes interpreter work (REPL, BASIC, PROC, etc.) at a time. Other sessions may exist in the table but do not make progress until the runner grants the execution token. This is **not** cooperative scheduling (M15).

#### 2.5 Daemon lifecycle and configuration

- **Start / stop** — foreground daemon for M13; graceful shutdown on **SIGTERM** (release locks, destroy sessions, remove socket file)
- **`DaemonConfig`** — socket path, `maxSessions`, host paths (catalogue, modules, pick root)
- **Not** systemd / journald (Milestone 17)

#### 2.6 IPC foundation

- Unix domain sockets only for v1; no remote telnet/SSH in this milestone
- Minimal wire protocol: version/handshake, ping, shutdown request; optional “reserve session slot” stub
- **No** login or REPL byte stream over IPC — that is [Milestone 14](14-multi-session-console-support.md)
- Socket access control via host file permissions (owner-only or documented group policy)

#### 2.7 Standalone / embedded path

**`gemini-system`** embeds the GSD with `maxSessions = 1`, binds session I/O to in-process `stdin`/`stdout`/`stderr`, and runs the existing login → REPL loop. External behaviour must remain unchanged from the operator’s perspective (M12 smoke checklist in §9).

#### 2.8 Execution model (M13 vs M14 vs M15)

| Milestone | What “multi-session” means |
|-----------|----------------------------|
| **M13** | Multiple session **objects** in table; **serial** execution; IPC **plumbing** only |
| **M14** | Multiple **attached consoles**; login/REPL over IPC; still serial execution |
| **M15** | Multiple sessions **make progress** via cooperative yield at I/O boundaries |

Without this distinction, “multi-session” in M13 can be mistaken for M14 or M15.

#### 2.9 Open design note (Stage 1)

[`BootContext`](../../src/core/boot/BootMonitor.h) today passes a session `Runtime*` into [`LanguageModuleLoader`](../../src/core/languages/LanguageModuleLoader.h) during cold start. At daemon scope, Stage 1 must choose either:

- **(a)** a short-lived bootstrap `Runtime` used only for module load at cold start, or
- **(b)** defer registry attach until the first session exists

Either approach is acceptable provided M11 “load and freeze at cold start” semantics are preserved.

---

### 3. Architecture

#### 3.1 Process vs session scope

| Scope | Owned by | Examples |
|-------|----------|----------|
| **Process (GSD)** | `GeminiServiceDaemon` | [`BootMonitor`](../../src/core/boot/BootMonitor.h), frozen [`LanguageRegistry`](../../src/core/languages/LanguageRegistry.h), shared [`LockTable`](../../src/core/locking/LockTable.h), [`PortManager`](../../src/core/daemon/PortManager.h) (planned), session table, IPC server |
| **Session** | [`GeminiSession`](../../src/userland/tcl/GeminiSession.h) | [`Runtime`](../../src/core/vm/Runtime.h), [`Shell`](../../src/userland/tcl/Shell.h), account binding, per-session filesystem root, lock session id, I/O channels |

#### 3.2 Entry points

| Binary | Role |
|--------|------|
| **`gemini-system`** | Embedded GSD, `maxSessions = 1`, direct console I/O (application edition packaging in [Milestone 16](16-standalone-edition-application-mode.md)) |
| **`gemini-daemon`** | Long-running GSD; foreground for M13; systemd wrapper in [Milestone 17](17-service-integration-deployment.md) |

Both link the same daemon host implementation — not parallel codebases.

#### 3.3 Component relationships

```text
                    +------------------ GeminiServiceDaemon ------------------+
                    |  BootMonitor   LanguageRegistry(frozen)   LockTable    |
                    |  PortManager   SessionTable   SerialSessionRunner       |
                    |  IpcServer (minimal; M14 attaches consoles)            |
                    +---------------------------+-----------------------------+
                                                |
              +---------------------------------+----------------------------------+
              |                                 |                                  |
       +------v------+                   +------v------+                    (future)
       | Session 1   |                   | Session 2   |                    gemini-console
       | GeminiSession                  | GeminiSession                   (Milestone 14)
       | Runtime+Shell                  | Runtime+Shell
       +-------------+                   +-------------+
```

---

### 4. Invariants

- **One cold start per process** — `BootMonitor` runs at daemon initialise, not per session or per console
- **One frozen `LanguageRegistry` per process** — populated at boot; immutable until daemon restart
- **One shared `LockTable` per process** — all sessions bind to the daemon-owned table; per-session lock ids remain distinct
- **At most one running interpreter stack** — serial runner enforces exclusive execution across sessions in M13
- **Session M12 invariants hold** — one `Runtime`, one interpreter stack, isolated VM state per `GeminiSession` (see [session.md](../session.md))
- **Port uniqueness** — no two live sessions share the same port / session id
- **IPC is transport only in M13** — no change to interpreter error policy; transport-level framing is M14 scope

---

### 5. Non-goals

Milestone 13 does **not** introduce:

- **`gemini-console`** client, login/REPL over IPC, or console multiplexing ([**Milestone 14**](14-multi-session-console-support.md))
- Cooperative multi-session scheduling or yield at I/O ([**Milestone 15**](15-cooperative-multi-session-execution.md))
- Application-edition packaging, failover when no service is running ([**Milestone 16**](16-standalone-edition-application-mode.md))
- **systemd** unit, **journald**, production config polish, or admin Tcl commands **`LISTSESSIONS`** / **`KILLSESSION`** / **`STATUS`** ([**Milestone 17**](17-service-integration-deployment.md))
- Remote telnet/SSH, hot-reload of language modules without daemon restart, or session restore across cold restart
- Any change to Tcl/BASIC/PROC/ENGLISH command semantics, locking rules, or language dispatch

---

### 6. Compatibility expectations

External behaviour of **`gemini-system`** remains unchanged — only internal composition changes:

- Login flow unchanged ([`LoginService`](../../src/core/login/LoginService.h), `LOGON PLEASE:` cadence)
- Command semantics unchanged
- Interpreter behaviour unchanged
- Error messages and formatting unchanged (interpreter-local; see M12 §3.1)
- Locking semantics unchanged ([Milestone 10](10-record-locking-multi-user.md))
- Multi-language runtime behaviour unchanged ([Milestone 11](11-multi-language-runtime-infrastructure.md))
- Boot banner sequence preserved; **`PORT MANAGER:`** line becomes live status instead of `(stub)`

---

### 7. Dependencies and grounding

- Builds on [**Milestone 12**](12-session-model-foundation.md) — `GeminiSession` lifecycle and I/O boundary; daemon drives the same transitions
- Builds on [**Milestone 11**](11-multi-language-runtime-infrastructure.md) — boot-time module load and frozen registry
- Builds on [**Milestone 10**](10-record-locking-multi-user.md) — shared lock table and per-session lock ids
- **PORT MANAGER** — stable port assignment for **`WHO`**, **`LISTSESSIONS`** (M17), and lock identity; implemented in M13 Stage 3, consumed by M14
- Sessions use **`GeminiSession`** from M12 (implemented; see [session.md](../session.md))
- IPC is Unix domain sockets only for v1

---

### 8. Deliverables

**Code (anticipated paths):**

- **`GeminiServiceDaemon`** — `src/core/daemon/` (daemon host, config, lifecycle)
- **`SessionTable`**, **`SerialSessionRunner`**, **`PortManager`** — `src/core/daemon/`
- **`gemini-daemon`** executable — `src/daemon/Main.cpp` (or equivalent); CMake sibling to [`gemini-system`](../../src/CMakeLists.txt)
- Refactored [`Main.cpp`](../../src/Main.cpp) — embedded daemon + single session + login loop
- Minimal IPC server and protocol types — `src/core/daemon/`

**Tests:**

- `tests/core/test_GeminiDaemon.cpp` — bootstrap, registry frozen, shared lock table
- `tests/core/test_SessionTable.cpp` — create/destroy, serial execution simulation
- `tests/core/test_PortManager.cpp` — port uniqueness and release
- `tests/core/test_DaemonIpc.cpp` — handshake over temp socket path

**Documentation:**

- [`docs/daemon.md`](../daemon.md) — GSD architecture, embedded vs `gemini-daemon`, IPC boundary vs M14 (Stage 6)
- Updates to [`docs/session.md`](../session.md), [`docs/gemini-bootstrap.md`](../gemini-bootstrap.md)

---

### 9. Milestone completion criteria

- **`GeminiServiceDaemon`** owns process-scope boot, frozen registry, and shared lock table
- **Session table** holds multiple `GeminiSession` objects; **serial runner** ensures at most one executes interpreter work at a time
- **Port manager** assigns stable port/session ids; **`WHO`** and lock identity remain consistent
- **`gemini-daemon`** starts, accepts configuration, shuts down cleanly on SIGTERM
- **IPC server** listens on a Unix domain socket; minimal handshake protocol documented; socket permissions enforced
- **`gemini-system`** passes manual smoke unchanged (see checklist below)
- All existing tests pass; new tests cover session table, port manager, and IPC handshake
- Architecture reference in [`docs/daemon.md`](../daemon.md); [`docs/session.md`](../session.md) updated

**Manual smoke checklist** (same as M12 — must pass on **`gemini-system`** after every implementation stage):

1. Boot with catalogue → interactive or auto LOGON
2. Tcl: `VERSION`, `WHO`
3. Enter BASIC → run a one-line program → `END`
4. `LOGOFF` → re-login
5. `QUIT` → clean process exit

---

### 10. Delivery plan

Implementation is sequenced into vertical stages. Each stage ships a test-locked slice before the next starts; the full test suite must remain green after every stage. **`gemini-system` external behaviour must not regress** until Stage 6 claims milestone closure. Detailed per-stage plans may live in `~/.cursor/plans/m13_stage_*.plan.md` during implementation; status is summarised here as each stage lands (matching the M8 / M9 / M10 / M11 / M12 precedent).

- **Stage 1 — Daemon core extraction**: introduce **`GeminiServiceDaemon`** under `src/core/daemon/`; move process-scope boot from [`Main.cpp`](../../src/Main.cpp) into daemon `coldStart()` / `initialize()` (`BootMonitor`, `LanguageRegistry` freeze, shared `LockTable`); refactor **`gemini-system`** to create an embedded daemon, create one session, and run the existing login/REPL loop. Resolve the bootstrap `Runtime` design note in §2.9. **Exit criterion:** external `gemini-system` behaviour unchanged; boot banner still correct. Tests: `tests/core/test_GeminiDaemon.cpp`. *Status: planned.*

- **Stage 2 — Session table + serial runner**: implement **`SessionTable`** (`createSession()`, `destroySession()`, lookup by id/port) and **`SerialSessionRunner`** (exclusive execution token; second session blocked or queued while first runs REPL); wire every session to the daemon’s shared `LockTable` (replace ad-hoc [`LockRegistry`](../../src/core/locking/LockRegistry.h) wiring in [`Shell.cpp`](../../src/userland/tcl/Shell.cpp) with daemon-owned injection). **Exit criterion:** multi-session object tests pass; standalone still single active session. Tests: `tests/core/test_SessionTable.cpp`. *Status: planned.*

- **Stage 3 — Port manager**: implement **`PortManager`** (allocate/release port numbers at session create/destroy); replace **`PORT MANAGER: (stub)`** in [`BootMonitor.cpp`](../../src/core/boot/BootMonitor.cpp) with live status; integrate with login/`attach` so [`whoPort`](../../src/userland/tcl/GeminiSession.h) reflects daemon-assigned id. **Exit criterion:** unique ports across concurrent session objects; `WHO` stable for session lifetime. Tests: `tests/core/test_PortManager.cpp`. *Status: planned.*

- **Stage 4 — Daemon lifecycle + configuration**: implement **`DaemonConfig`** (socket path, `maxSessions`, host paths); add **`gemini-daemon`** executable; foreground run loop; **SIGTERM** graceful shutdown (release locks, destroy sessions, remove socket file). Not systemd/journald (M17). **Exit criterion:** `gemini-daemon` starts, runs idle, stops cleanly; `gemini-system` still works embedded. Tests: lifecycle start/stop, config defaults. *Status: planned.*

- **Stage 5 — IPC foundation**: Unix domain socket server inside GSD; minimal protocol (version/handshake, ping, shutdown request; optional “reserve session slot” stub — **no** login or REPL byte stream); filesystem permissions on socket. **Exit criterion:** test client connects, handshakes, disconnects; daemon remains stable. Tests: `tests/core/test_DaemonIpc.cpp`. *Status: planned.*

- **Stage 6 — Docs + closes M13**: add [`docs/daemon.md`](../daemon.md); update [`docs/session.md`](../session.md) and [`docs/gemini-bootstrap.md`](../gemini-bootstrap.md); link from [`docs/milestones.md`](../milestones.md); audit §9 completion criteria; run full test suite + M12 smoke. **Closes Milestone 13.** *Status: planned.*

Only Stage 6's exit criteria should claim "Closes Milestone 13".
