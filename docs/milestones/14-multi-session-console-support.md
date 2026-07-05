← [Project milestones index](../milestones.md)

## Milestone 14 — Multi-Session Console Support

Allow multiple consoles to attach to the daemon, each bound to its own session. Ship a **`gemini-console`** client that connects over IPC, with a protocol to request a new session or attach to an existing one. Multiplex console input and output to the correct session; keep interpreter and runtime state isolated per session. Support graceful detach so a console can disconnect without terminating the session. Execution remains serial across sessions until Milestone 15. *Status: planned.*

M13 delivered the service host and minimal IPC plumbing; M14 crosses the IPC boundary with login and REPL byte streams. This is the first milestone where Gemini **feels like a multi-user Pick system** (UniData-style): multiple consoles, multiple sessions, but **still serial execution**—only one session runs interpreter work at a time until [**Milestone 15**](15-cooperative-multi-session-execution.md).

---

### 1. Purpose and rationale

Before M14, [`gemini-daemon`](../../src/daemon/Main.cpp) listens on a Unix domain socket and accepts M13 control messages (handshake, ping, shutdown, reserve-session stub) but has **no interactive operator path**. [`gemini-system`](../../src/Main.cpp) remains the only way to run catalogue login and a Tcl REPL, using in-process `stdin`/`stdout`/`stderr` on a single embedded session.

M12 defined session-owned I/O channels ([`GeminiSession::input()`](../../src/userland/tcl/GeminiSession.h) / `output()` / `diagnostic()`). M13 wired those channels to process streams in embedded mode and left remote transport as a documented boundary ([`docs/daemon.md`](../daemon.md)). M14 replaces the transport for daemon-attached consoles: **`gemini-console`** becomes a thin terminal front-end; the daemon bridges wire frames to the same session I/O API [`Main.cpp`](../../src/Main.cpp) already uses for login and REPL.

The design constraint from M12–M13 carries forward: **`gemini-system` external behaviour must not regress**; console attachment is an additional entry path on the same `GeminiSessionHost` / `SerialSessionRunner` stack, not a parallel interpreter implementation.

---

### 2. Scope

#### 2.1 `gemini-console` client

A new executable that:

- Connects to the daemon Unix socket configured by [`DaemonConfig`](../../src/core/daemon/DaemonConfig.h) (same default/env vars as `gemini-daemon`)
- Performs M13 handshake, then M14 session attach
- Multiplexes the operator terminal (`stdin`/`stdout`/`stderr`) to the daemon session channel for the bound port
- Supports **create** (new session + port) and **attach** (reconnect to an existing detached session by port)
- Exits cleanly on EOF or operator interrupt; **graceful detach** leaves the session object in the daemon table

#### 2.2 Session attach and port policy

- **New session:** daemon allocates a port via [`PortManager`](../../src/core/daemon/PortManager.h) at `createSession()` (M13 Stage 3 policy); console receives port in attach-ack
- **Existing session:** console sends attach with known port; daemon validates session exists, is not bound to another live console, and attaches the connection
- **Port / session id** feeds **`WHO`**, lock identity ([`GeminiSession::makeSessionLockId`](../../src/userland/tcl/GeminiSession.h)), and future **`LISTSESSIONS`** (M17)
- M14 **consumes** port policy; it does not redefine allocation rules

#### 2.3 Catalogue login over IPC

**Resolved (M14 v1):** each console runs the **full catalogue login** ([`LoginService`](../../src/core/login/LoginService.h), `LOGON PLEASE:` cadence) over the session I/O bridge—the same flow as [`Main.cpp`](../../src/Main.cpp). The daemon does **not** treat the Unix socket peer identity as the Pick account; socket **`0600`** permissions (M13) gate **who may connect**, not **which account** is logged in.

Auto-login env vars (`GEMINI_AUTO_LOGON`, `MD,AUTO-LOGON`) apply per console process as today.

#### 2.4 Session I/O bridge

- Extend IPC beyond M13 control messages with **session channel** frames carrying stdin/stdout/diagnostic bytes (or line-oriented chunks—pick one in Stage 1 and document)
- Implement stream adapters bound via [`GeminiSession::setInputStream`](../../src/userland/tcl/GeminiSession.h) / `setOutputStream` / `setDiagnosticStream`
- Interpreter error policy remains **session-local** (M12); M14 adds transport framing only

#### 2.5 Multi-connection daemon server

M13 [`DaemonIpcServer`](../../src/core/daemon/DaemonIpcServer.h) handles **one client connection at a time** (`clientFd_`). M14 requires:

- Accept **multiple concurrent** console connections
- Map each connection → session port (when attached) and pump I/O for that session
- Keep M13 control messages (handshake, ping, shutdown, reserve-session) working on each connection

#### 2.6 REPL and serial execution

- Login and REPL run under [`GeminiSessionHost::runExclusive`](../../src/userland/tcl/GeminiSessionHost.h) / [`SerialSessionRunner`](../../src/core/daemon/SerialSessionRunner.h)—same as embedded [`Main.cpp`](../../src/Main.cpp)
- **At most one** session executes interpreter work (REPL, BASIC, PROC, etc.) at a time
- A second console may complete attach (and wait at login/REPL) while the runner is held; behaviour while blocked is **wait for the serial token**—no cooperative progress for the waiting session until M15
- Document operator-visible blocking; do not implement fairness or I/O yield here

#### 2.7 Graceful detach

- Console disconnect or explicit detach message **unbinds** the connection from the session I/O bridge
- Session object **remains** in [`SessionTable`](../../src/userland/tcl/SessionTable.h); login state and VM state preserved
- **`destroySession`** remains explicit (daemon shutdown, future admin kill in M17)—detach ≠ destroy
- Re-attach to the same port restores console I/O without re-login if the session was still logged in

#### 2.8 Execution model (M13 vs M14 vs M15)

| Milestone | What “multi-session” means |
|-----------|----------------------------|
| **M13** | Multiple session **objects** in table; **serial** execution; IPC **plumbing** only |
| **M14** | Multiple **attached consoles**; login/REPL over IPC; still serial execution |
| **M15** | Multiple sessions **make progress** via cooperative yield at I/O boundaries |

#### 2.9 Embedded / standalone path

**`gemini-system`** is unchanged: embedded GSD, `maxSessions = 1`, in-process I/O, existing login → REPL loop. M14 work must not alter operator-visible behaviour on that path.

---

### 3. Architecture

#### 3.1 Console attachment flow

```text
  gemini-console A          gemini-console B
        |                         |
        +-------- IPC (Unix) -----+
                    |
            +-------v--------+
            | gemini-daemon  |
            | GeminiDaemon   |
            | Runner         |
            +-------+--------+
                    |
        +-----------+-----------+
        |  Connection manager   |  (M14: multi-client)
        |  Session I/O bridge |
        +-----------+-----------+
                    |
        +-----------+-----------+
        | GeminiSessionHost   |
        | SessionTable        |
        | SerialSessionRunner |
        +-----------+-----------+
                    |
         +----------+----------+
         |                     |
   +-----v-----+         +-----v-----+
   | Session 1 |         | Session 2 |
   | port 1    |         | port 2    |
   | Runtime   |         | Runtime   |
   | + Shell   |         | + Shell   |
   +-----------+         +-----------+
```

#### 3.2 Entry points (after M14)

| Binary | Role |
|--------|------|
| **`gemini-system`** | Embedded GSD, one session, direct stdio (unchanged) |
| **`gemini-daemon`** | Long-running GSD; accepts multiple `gemini-console` connections |
| **`gemini-console`** | Terminal client; session attach + login/REPL over IPC |

All three link the same session and daemon host implementation where applicable—not parallel interpreters.

#### 3.3 IPC layering

| Layer | M13 | M14 |
|-------|-----|-----|
| Transport | Unix stream socket, `GEMI` frame header | Unchanged |
| Control plane | Handshake, ping, shutdown, reserve-session | Unchanged; reserve-session may be superseded by attach-with-create |
| Session plane | *(none)* | Attach/detach, session I/O frames, attach-ack with port |

Protocol version negotiation via handshake continues; M14 adds message types (and may bump **`clientProtocolVersion`** / server ack if required—Stage 1 decides v1 extension vs v2).

---

### 4. Invariants

- **One cold start per process** — unchanged from M13
- **One frozen `LanguageRegistry` per process** — unchanged
- **One shared `LockTable` per process** — sessions bind distinct lock ids per port
- **At most one running interpreter stack** — serial runner unchanged; M14 does not add threads or preemptive scheduling
- **Session M12 invariants hold** — one `Runtime`, one interpreter stack, isolated VM state per `GeminiSession`
- **Port uniqueness** — no two live sessions share the same port
- **Detach ≠ destroy** — console disconnect preserves session object until explicit destroy or daemon shutdown
- **At most one live console per session** — re-attach rejected while another connection is bound
- **`gemini-system` smoke checklist** — passes after every implementation stage

---

### 5. Non-goals

Milestone 14 does **not** introduce:

- Cooperative multi-session scheduling or yield at I/O ([**Milestone 15**](15-cooperative-multi-session-execution.md))
- Application-edition packaging or service failover ([**Milestone 16**](16-standalone-edition-application-mode.md))
- **systemd**, **journald**, or admin Tcl commands **`LISTSESSIONS`** / **`KILLSESSION`** / **`STATUS`** ([**Milestone 17**](17-service-integration-deployment.md))
- Remote telnet/SSH (Unix domain socket only for v1)
- Session restore across daemon cold restart
- Changes to Tcl/BASIC/PROC/ENGLISH command semantics, locking rules, or language dispatch
- Trusting Unix peer credentials as Pick account identity (login remains catalogue-based)

---

### 6. Compatibility expectations

- **`gemini-system`** manual smoke (§9) unchanged—login, `VERSION`, `WHO`, BASIC one-liner, `LOGOFF`, `QUIT`
- M13 IPC clients that only handshake/ping/shutdown continue to work
- Login cadence and error messages match embedded mode ([`LoginService`](../../src/core/login/LoginService.h))
- Locking and multi-language behaviour unchanged ([M10](10-record-locking-multi-user.md), [M11](11-multi-language-runtime-infrastructure.md))

---

### 7. Dependencies and grounding

- Builds on [**Milestone 13**](13-service-daemon-architecture.md) — `GeminiServiceDaemon`, `GeminiSessionHost`, `PortManager`, `DaemonIpcServer`, protocol v1
- Builds on [**Milestone 12**](12-session-model-foundation.md) — `GeminiSession` I/O channels and lifecycle (`attach` / `detach` / `reset` / `destroy`)
- [`docs/daemon.md`](../daemon.md) documents the M13 IPC boundary; M14 extends it
- [`docs/session.md`](../session.md) documents session scope; console I/O is another transport for the same channels

---

### 8. Deliverables

**Code (anticipated paths):**

- **IPC session plane** — extend [`DaemonIpcProtocol.h`](../../src/core/daemon/DaemonIpcProtocol.h) (attach, detach, session I/O message types)
- **Multi-client server** — refactor [`DaemonIpcServer`](../../src/core/daemon/DaemonIpcServer.h) or add `DaemonConnectionManager` under `src/core/daemon/`
- **Session I/O bridge** — stream adapters (e.g. `IpcSessionStreams` or equivalent) under `src/core/daemon/` or `src/userland/tcl/`
- **Daemon integration** — extend [`GeminiDaemonRunner`](../../src/userland/tcl/GeminiDaemonRunner.cpp) accept/pump loop for attached consoles
- **`gemini-console`** executable — `src/console/Main.cpp` (or equivalent); CMake sibling to `gemini-system` / `gemini-daemon`

**Tests:**

- `tests/core/test_DaemonIpcSession.cpp` — attach/detach, session I/O encode/decode, multi-connection handshake
- `tests/core/test_SessionIoBridge.cpp` — round-trip bytes through bridge into `GeminiSession` streams
- `tests/console/test_GeminiConsole.cpp` — integration against temp-socket daemon (login + one Tcl command)
- Existing M13 IPC and session-table tests remain green

**Documentation:**

- Extend [`docs/daemon.md`](../daemon.md) with M14 session-plane protocol and console attachment
- Add [`docs/console.md`](../console.md) (or equivalent) — `gemini-console` usage, attach/create, detach semantics
- Updates to [`docs/session.md`](../session.md), [`docs/gemini-bootstrap.md`](../gemini-bootstrap.md), [`docs/README.md`](../README.md), root [`README.md`](../../README.md)

---

### 9. Milestone completion criteria

- **`gemini-console`** connects, handshakes, creates or attaches to a session, and runs catalogue login over IPC
- **Session I/O bridge** drives [`LoginService`](../../src/core/login/LoginService.h) and [`Shell::runTclRepl`](../../src/userland/tcl/Shell.cpp) through `GeminiSession` channels
- **Two consoles** may attach to **two sessions** simultaneously; **serial runner** ensures at most one executes interpreter work
- **Graceful detach:** disconnecting a console leaves the session in the table; re-attach to the same port restores I/O
- **`WHO`** reports daemon-assigned port; lock identity stable per session
- **`gemini-system`** passes manual smoke unchanged
- All existing tests pass; new tests cover session IPC, I/O bridge, and console integration
- Protocol and architecture documented in [`docs/daemon.md`](../daemon.md) and console reference

**Manual smoke checklist — `gemini-system`** (must pass after every stage):

1. Boot with catalogue → interactive or auto LOGON
2. Tcl: `VERSION`, `WHO`
3. Enter BASIC → run a one-line program → `END`
4. `LOGOFF` → re-login
5. `QUIT` → clean process exit

**Manual smoke checklist — `gemini-daemon` + `gemini-console`** (required for milestone closure):

1. Start `gemini-daemon` on a temp socket; verify boot banner and socket `0600`
2. Launch **`gemini-console`** → new session → catalogue login → `VERSION`, `WHO` (expect assigned port)
3. Launch a **second** `gemini-console` → second session → login → distinct port
4. Run a Tcl command on each console (one runs while the other waits on serial runner—acceptable in M14)
5. Disconnect first console (graceful detach) → session remains; re-attach by port → still logged in
6. `QUIT` both consoles → `SIGTERM` daemon → clean exit

---

### 10. Delivery plan

Implementation is sequenced into vertical stages. Each stage ships a test-locked slice before the next starts; the full test suite must remain green after every stage. **`gemini-system` external behaviour must not regress** until Stage 7 claims milestone closure. Detailed per-stage plans may live in `~/.cursor/plans/m14_stage_*.plan.md` during implementation; status is summarised here as each stage lands (matching the M12 / M13 precedent).

- **Stage 1 — IPC session-plane protocol**: extend [`DaemonIpcProtocol.h`](../../src/core/daemon/DaemonIpcProtocol.h) with attach/detach and session I/O message types (payload layouts, error codes, handshake compatibility rule); document frame semantics in header comment and [`docs/daemon.md`](../daemon.md) draft section. **Exit criterion:** encode/decode unit tests for all new message types; existing M13 IPC tests unchanged. Tests: `tests/core/test_DaemonIpcSession.cpp` (protocol only). *Status: implemented.*

- **Stage 2 — Multi-client connection manager**: refactor [`DaemonIpcServer`](../../src/core/daemon/DaemonIpcServer.h) (or add connection manager) to accept **multiple concurrent** clients; per-connection handshake state; integrate accept/poll loop in [`GeminiDaemonRunner`](../../src/userland/tcl/GeminiDaemonRunner.cpp). M13 single-client control messages still work. **Exit criterion:** two test clients handshake simultaneously on one daemon; daemon stable. Tests: extend `test_DaemonIpcSession.cpp`. *Status: implemented.*

- **Stage 3 — Session I/O bridge**: implement stream adapters that read/write session-plane frames; bind to [`GeminiSession`](../../src/userland/tcl/GeminiSession.h) via `setInputStream` / `setOutputStream` / `setDiagnosticStream`; map connection ↔ session port in daemon. **Exit criterion:** test pumps bytes from fake client into session `input()` and reads `output()` back over IPC. Tests: `tests/core/test_SessionIoBridge.cpp`. *Status: implemented.*

- **Stage 4 — `gemini-console` skeleton**: new executable; CLI (`--socket`, optional `--port` for attach); connect, handshake, attach-or-create, pump terminal ↔ session I/O until disconnect. No full login/REPL orchestration yet—echo or ping-over-session acceptable interim. **Exit criterion:** console binary connects to running daemon and exchanges session I/O frames. Tests: `tests/console/test_GeminiConsole.cpp` (minimal). *Status: implemented.*

- **Stage 5 — Login over IPC**: wire [`LoginService::runCatalogLogin`](../../src/core/login/LoginService.h) through session I/O bridge inside daemon-side attach handler (mirror [`Main.cpp`](../../src/Main.cpp) login loop without REPL); `gemini-console` drives interactive login. **Exit criterion:** console completes `LOGON PLEASE:` flow and session is attached with account binding. *Status: implemented.*

- **Stage 6 — REPL, multi-console, graceful detach**: run [`Shell::runTclRepl`](../../src/userland/tcl/Shell.cpp) under `runExclusive` for attached sessions; second console blocked on serial runner; implement detach (console disconnect or message) without `destroySession`; re-attach to existing port. **Exit criterion:** two-console manual smoke (§9); detach/re-attach test; `WHO` correct per session. *Status: planned.*

- **Stage 7 — Docs + closes M14**: finalize [`docs/daemon.md`](../daemon.md) session plane; add [`docs/console.md`](../console.md); update [`docs/session.md`](../session.md), [`docs/gemini-bootstrap.md`](../gemini-bootstrap.md), milestone index; audit §9 completion criteria; full test suite + both smoke checklists. **Closes Milestone 14.** *Status: planned.*

Only Stage 7's exit criteria should claim "Closes Milestone 14".

*Status: planned.*
