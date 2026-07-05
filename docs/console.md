# Console client (`gemini-console`)

**`gemini-console`** is a thin terminal front-end for a running **`gemini-daemon`**. It connects over a Unix domain socket, attaches to a session (create or re-attach by port), and pumps terminal bytes to the daemon session I/O bridge. The daemon owns the login and Tcl REPL loop — the same [`Main.cpp`](../src/Main.cpp) flow, driven through [`GeminiSession`](../src/userland/tcl/GeminiSession.h) channels.

See [Service daemon architecture](daemon.md) for IPC protocol and daemon lifecycle. See [Milestone 14](milestones/14-multi-session-console-support.md) and [Milestone 15](milestones/15-cooperative-multi-session-execution.md) for delivery history.

## Overview

| Component | Role |
|-----------|------|
| **`gemini-console`** | Handshake, attach/detach, `stdin`/`stdout`/`stderr` ↔ session I/O frames |
| **`gemini-daemon`** | Cold start, session table, IPC server, session worker (login + REPL) per attach |
| **`GeminiSession`** | Same session object and I/O API as embedded `gemini-system` |

The console does **not** interpret Tcl or run login logic locally. Catalogue login (`LOGON PLEASE:` cadence) runs in the daemon under [`GeminiDaemonRunner::startSessionWorker`](../src/userland/tcl/GeminiDaemonRunner.cpp).

## Prerequisites

- A running **`gemini-daemon`** on a reachable Unix domain socket
- **Catalogue and pick roots** configured on the **daemon** (`--catalog-root`, `--pick-root`, or `GEMINI_CATALOG_ROOT` / `GEMINI_FILESYSTEM_ROOT`) — not on the console
- Unix domain IPC (not available on Windows; [`Main.cpp`](../src/console/Main.cpp) exits with an error there)

Socket file permissions are **0600** — only the socket owner may connect. This gates **who may attach**, not **which Pick account** is logged in; account identity comes from catalogue login over the session bridge.

## CLI reference

```bash
gemini-console [--socket PATH] [--port N] [--help]
```

| Flag / env | Default | Purpose |
|------------|---------|---------|
| `--socket PATH` | `GEMINI_DAEMON_SOCKET`, else [`defaultDaemonSocketPath()`](../src/core/daemon/DaemonConfig.cpp) | Daemon Unix socket path |
| `--port N` | omitted → **create** new session | Attach to an existing **detached** session port |
| `--help` | | Print usage and exit |

Implementation: [`ConsoleConfig`](../src/core/daemon/ConsoleConfig.h), [`DaemonIpcClient`](../src/core/daemon/DaemonIpcClient.h).

## Typical workflows

### New session

```bash
# Terminal 1 — start daemon (example paths; adjust for your tree)
cd build/src
gemini-daemon --socket /tmp/gemini.sock \
  --catalog-root gemini --pick-root gemini/accounts/TST

# Terminal 2 — attach and create session
gemini-console --socket /tmp/gemini.sock
# → LOGON PLEASE:, type account, Tcl REPL banner
```

### Re-attach to a detached logged-in session

After a console disconnects gracefully, the session remains in the daemon table with login state preserved:

```bash
gemini-console --socket /tmp/gemini.sock --port 1
# → skips login if still logged in; REPL resumes
```

### Graceful detach

- **Ctrl-C** or **SIGTERM** — requests pump stop; [`disconnect()`](../src/core/daemon/DaemonIpcClient.cpp) sends **`DetachSession`** before closing the socket
- **stdin EOF** — pump exits after draining pending output; disconnect sends detach if still attached

The session object, login state, and daemon-assigned **`whoPort`** survive detach until daemon shutdown or explicit session destroy.

## Operator notes

### `WHO` and ports

**`WHO`** prints `port username account` using the **daemon-assigned port** from session create ([`SessionTable`](../src/userland/tcl/SessionTable.cpp)). With multiple consoles, each session gets a distinct port (typically 1, 2, …).

### Cooperative scheduling

Multiple consoles attached to **`gemini-daemon`** make progress concurrently at I/O boundaries ([`CooperativeSessionRunner`](../src/core/daemon/CooperativeSessionRunner.h)):

- Each console reaches **`LOGON PLEASE:`** and **`TCL>`** without waiting for another console to **`QUIT`**
- A session blocked in BASIC **`INPUT`** does not prevent another console from running Tcl
- Round-robin fairness rotates the execution token among idle-at-prompt sessions

Only one session runs interpreter work at any instant — this is cooperative scheduling, not preemption. See [Service daemon architecture — cooperative execution](daemon.md#cooperative-execution).

### Auto-logon

`GEMINI_AUTO_LOGON`, `GEMINI_AUTO_LOGIN`, and `MD,AUTO-LOGON` apply in the **daemon process** on the first catalogue login per port. Set these on the daemon environment, not the console. See [Gemini bootstrap](gemini-bootstrap.md).

### At most one console per session

A second attach to a port that already has a live console binding receives **`SessionAlreadyBound`**. Detach or disconnect the first console before re-attaching.

## Manual smoke runbook

Use this checklist to verify daemon + console integration (Milestone 15 §9). Run from a build tree with the `gemini/` bootstrap copied next to binaries (`cd build/src`).

**1. Start daemon**

```bash
gemini-daemon --socket /tmp/gemini-test.sock \
  --catalog-root gemini --pick-root gemini/accounts/TST --max-sessions 4
```

Verify boot banner on stdout and socket mode `0600` (`ls -l /tmp/gemini-test.sock`).

**2. First console — login and identity**

```bash
gemini-console --socket /tmp/gemini-test.sock
# LOGON: TST (or auto-logon if configured)
# Tcl: VERSION
# Tcl: WHO   → expect port 1, account TST
# Tcl: QUIT
```

**3. Second console — distinct port (both attached)**

```bash
# Terminal 3 — leave first console at TCL> (do not QUIT)
gemini-console --socket /tmp/gemini-test.sock
# login → WHO → expect port 2 (or next free port)
# both consoles should show TCL> concurrently
```

**4. Interleaved Tcl and BASIC INPUT**

With both consoles at **`TCL>`**:

```bash
# Console A: enter BASIC, run a program with INPUT (or RUN INPUTWAIT if seeded in BP)
# Console B: run WHO or VERSION while A waits at INPUT
# Both should respond without either appearing hung
```

**5. Graceful detach and re-attach**

```bash
# Console A: login, WHO, then Ctrl-C or EOF without QUIT (detach)
gemini-console --socket /tmp/gemini-test.sock --port 1
# → should skip login; WHO shows same port
```

**6. Shutdown**

Quit all consoles, then `kill -TERM` the daemon process. Socket file removed; clean exit.

## Embedded path unchanged

**`gemini-system`** remains the single-process embedded host (`maxSessions = 1`, direct stdio). It does not use `gemini-console` or IPC. Manual smoke for embedded mode is unchanged — see [Gemini bootstrap](gemini-bootstrap.md).

## Source map

| File | Role |
|------|------|
| [`src/console/Main.cpp`](../src/console/Main.cpp) | Console entry: connect, attach, pump |
| [`DaemonIpcClient.h`](../src/core/daemon/DaemonIpcClient.h) | IPC client, attach/detach, I/O pump |
| [`ConsoleConfig.h`](../src/core/daemon/ConsoleConfig.h) | CLI parsing |
| [`GeminiDaemonRunner.cpp`](../src/userland/tcl/GeminiDaemonRunner.cpp) | Daemon-side session worker |

## See also

- [Service daemon architecture](daemon.md) — IPC protocol, session worker, cooperative scheduling
- [Session model](session.md) — console I/O transport and detach semantics
- [Gemini bootstrap](gemini-bootstrap.md) — catalogue login and auto-logon
- [Developer shell (TCL)](tcl-shell.md) — Tcl commands after login
