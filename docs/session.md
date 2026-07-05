# Session model (`GeminiSession`)

One **`PickShell::GeminiSession`** represents one running Gemini System session: the VM runtime, Tcl/BASIC/PROC/assembler interpreter stack, account binding, per-session filesystem state, and I/O channels.

See [Milestone 12](milestones/12-session-model-foundation.md) for rationale and delivery history.

## Process vs session scope

| Scope | Owned by | Examples |
|-------|----------|----------|
| **Process** | [`GeminiServiceDaemon`](../src/core/daemon/GeminiServiceDaemon.h) (embedded in [`Main.cpp`](../src/Main.cpp) or long-running [`gemini-daemon`](../src/daemon/Main.cpp)) | [`BootMonitor`](../src/core/boot/BootMonitor.h), frozen [`LanguageRegistry`](../src/core/languages/LanguageRegistry.h), shared [`LockTable`](../src/core/locking/LockTable.h), [`PortManager`](../src/core/daemon/PortManager.h), [`DaemonIpcServer`](../src/core/daemon/DaemonIpcServer.h) (daemon binary only) |
| **Composition** | [`GeminiSessionHost`](../src/userland/tcl/GeminiSessionHost.h) | [`SessionTable`](../src/userland/tcl/SessionTable.h), [`SerialSessionRunner`](../src/core/daemon/SerialSessionRunner.h) |
| **Session** | [`GeminiSession`](../src/userland/tcl/GeminiSession.h) | [`Runtime`](../src/core/vm/Runtime.h), [`Shell`](../src/userland/tcl/Shell.h), Tcl env, filesystem root, lock session id, I/O channels |

Standalone **`gemini-system`** embeds the daemon with `maxSessions = 1` and hosts exactly one session. The same `GeminiSession` type lives in the daemon session table when using **`gemini-daemon`** with one or more **`gemini-console`** attachments. See [Service daemon architecture](daemon.md), [Console client](console.md), and [Milestone 13](milestones/13-service-daemon-architecture.md).

## Lifecycle API

| Method | Purpose |
|--------|---------|
| **`create()`** | Allocate session: one `Runtime`, embedded `Shell`, default I/O wiring |
| **`attach(UserSession)`** | Bind authenticated account: pick root, identity, lock session id (Pick port from daemon when assigned at create) |
| **`detach()`** | Remove account binding (login flags, lock id); does not clear VM program state; daemon-assigned Pick port survives until session destroy |
| **`reset()`** | Clear interpreter/VM program state; same `Runtime` instance; releases locks |
| **`destroy()`** | Explicit teardown: detach if logged in, reset, clear I/O stream pointers |

**Host mapping:**

- **`LOGOFF`** (Tcl): `detach()` then `reset()` → `ShellRunResult::EndSession` → `main` re-runs login
- **`QUIT`** (Tcl): `reset()` → process exit
- **`LOGTO`**: `attach()` with new account after re-authentication

`Shell::attachUserSession` delegates to `GeminiSession::attach()`.

## I/O channels

| Channel | API | Default when unset |
|---------|-----|-------------------|
| Input | `input()` / `setInputStream` | `std::cin` |
| Output | `output()` / `setOutputStream` | `std::cout` |
| Diagnostic | `diagnostic()` / `setDiagnosticStream` | `std::cerr` |

Login ([`LoginService`](../src/core/login/LoginService.h)) and the Tcl REPL read/write through session channels. Process-level boot ([`BootMonitor`](../src/core/boot/BootMonitor.cpp)) still writes to `std::cout` by design.

Tests inject `std::istringstream` / `std::ostringstream` via `setInputStream` / `setOutputStream`.

## Console I/O transport (M14)

| Mode | Transport | Binding |
|------|-----------|---------|
| **Embedded** (`gemini-system`) | Process `stdin` / `stdout` / `stderr` | Default stream pointers on session create |
| **Daemon-attached** (`gemini-console`) | IPC session-plane frames via [`IpcSessionChannel`](../src/core/daemon/IpcSessionChannel.h) | [`GeminiDaemonRunner`](../src/userland/tcl/GeminiDaemonRunner.cpp) sets streams on **`AttachSession`** |

Login ([`LoginService`](../src/core/login/LoginService.h)) and the Tcl REPL use the same session channel API in both modes — only the transport differs.

**Detach ≠ destroy:** when a console disconnects or sends **`DetachSession`**, stream pointers are cleared but the session object remains in [`SessionTable`](../src/userland/tcl/SessionTable.h). **`loggedIn()`**, VM state, and daemon-assigned **`whoPort`** are preserved until re-attach or **`destroySession`**.

**At most one live console per session** — a second attach to the same port while bound receives **`SessionAlreadyBound`**.

See [Console client](console.md) for operator workflows and [Service daemon architecture](daemon.md) for IPC details.

## Invariants

- Exactly one `Runtime` per session for the session lifetime
- Exactly one interpreter stack (`Shell` and nested BASIC/PROC/ASM modes)
- At most one active account binding (`loggedIn()` or detached)
- Daemon-assigned Pick port is set at session create ([`SessionTable`](../src/userland/tcl/SessionTable.cpp)); survives **`LOGOFF`** until **`destroy()`**
- Session-scoped VM state isolated from other sessions (cooperative scheduling is M15)
- Interpreter errors remain interpreter-local; the session routes bytes, not error policy

## Source map

| File | Role |
|------|------|
| [`GeminiSession.h`](../src/userland/tcl/GeminiSession.h) | Session type, state, lifecycle, I/O |
| [`GeminiSessionHost.h`](../src/userland/tcl/GeminiSessionHost.h) | Embedded/daemon composition: daemon + table + serial runner |
| [`SessionTable.h`](../src/userland/tcl/SessionTable.h) | Session object table, port allocation at create |
| [`Main.cpp`](../src/Main.cpp) | Embedded: `create()`, boot, login loop, `attach()` |
| [`GeminiDaemonRunner.cpp`](../src/userland/tcl/GeminiDaemonRunner.cpp) | Daemon: attach binds IPC streams; session worker drives login/REPL |
| [`Shell.h`](../src/userland/tcl/Shell.h) | Interpreter front-end; holds `GeminiSession&` |

## See also

- [Service daemon architecture](daemon.md) — GSD, `gemini-daemon`, IPC
- [Console client](console.md) — `gemini-console` attach/create and detach
- [Gemini bootstrap](gemini-bootstrap.md) — catalogue login and account roots
- [Developer shell (TCL)](tcl-shell.md) — Tcl REPL after login
- [Concurrency and record locking](concurrency.md) — lock binding on `attach()`
- [Milestone 14 — Multi-session console](milestones/14-multi-session-console-support.md) — console attachment (implemented)
