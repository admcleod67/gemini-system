← [Project milestones index](../milestones.md)

## Milestone 12 — Session Model Foundation (Expanded)

Milestone 12 established the formal session abstraction that all later service-oriented milestones (M13–M17) depend on. It unified the former fragmented execution model — **`UserSession`**, **`ShellSession`**, **`Runtime`**, and direct console I/O — into a single **`GeminiSession`** boundary representing one running Gemini System session. No daemon, concurrency, or multi-session scheduling was introduced; execution remains strictly single-threaded.

This milestone is the architectural hinge between the prototype and the service. A correct session boundary makes M13–M17 largely mechanical; an incorrect one forces the maintenance of two parallel systems.

---

### 1. Purpose and rationale

Before M12, session-relevant state was distributed across multiple types. M12 consolidated it into **`GeminiSession`**, which owns:

- The VM (**`Runtime`**)
- The interpreter stack (Tcl, BASIC, PROC, assembler) via **`Shell`**
- The account binding (from **`UserSession`** at login)
- The session’s I/O channels
- The lifecycle of the session itself

This is the foundation required for:

- A session table (M13)
- Remote consoles (M14)
- Cooperative multi-session execution (M15)
- Application mode (M16)
- Deployment as a Linux service (M17)

---

### 2. Scope

#### 2.1 Session object definition

**`GeminiSession`** owns all state required to execute commands and programs:

- **`Runtime`** — owned for the session lifetime
- **Shell / interpreter stack** — Tcl, BASIC, PROC, and assembler paths via **`Shell`**
- **Session I/O channels** — login, REPL, and interpreter output route through session `input()` / `output()` / `diagnostic()` (see [session.md](../session.md))
- **Account binding** — applied via **`attach(const UserSession&)`**

**Lock binding:** the session exposes per-session lock context (shared **`LockTable`**, session lock id on login per [Milestone 10](10-record-locking-multi-user.md)).

#### 2.2 Type migration (completed)

- **`UserSession`** remains the login DTO from **`LoginService`**; account fields are applied via **`GeminiSession::attach()`**.
- **`ShellSession`** was removed; its fields live on **`GeminiSession`**.
- **`Runtime`** is owned by **`GeminiSession`**.
- **`Shell`** holds **`GeminiSession&`** and does not own session state.

#### 2.3 Session lifecycle

Public API on [`GeminiSession`](../../src/userland/tcl/GeminiSession.h) (M13 daemon will drive the same transitions):

| Transition | API | Semantics |
|------------|-----|-----------|
| **create** | `GeminiSession::create()` | Allocate one **`Runtime`**, initialise interpreters, wire I/O |
| **attach** | `attach(UserSession)` | Bind authenticated account |
| **detach** | `detach()` | Remove account binding without destroying the session |
| **reset** | `reset()` | Clear interpreter and VM program state; retain same **`Runtime`** and I/O |
| **destroy** | `destroy()` / destructor | Full session teardown |

**VM lifetime vs reset:** **`reset`** does not reallocate the VM — it releases locks where applicable, clears Tcl env, active lists, breakpoints, and calls **`Runtime::loadProgram({})`**. **`detach`** is narrower (login/lock identity; used in **`LOGOFF`** before **`reset`**). **`QUIT`** calls **`reset()`** only.

#### 2.4 I/O boundary

Replace all direct use of **`std::cin`** / **`std::cout`** on **production paths** with a session-owned I/O interface. This includes **`LoginService`**, **`Shell::runTclRepl`**, and any interpreter path that currently writes directly to the console. Tests may continue to inject streams directly (existing **`Shell`** / **`Runtime`** stream hooks). This is the most important architectural seam for future multi-session support.

#### 2.5 I/O channel definition

A session exposes a minimal, well-defined I/O interface:

- **Input channel** — synchronous; **one underlying stream** shared by interpreters. Line-oriented for the Tcl REPL (**`getline`**); VM/BASIC **`INPUT`** uses the same stream (today: optional **`Shell::inputStream_`** / **`Runtime`** input stream).
- **Output channel** — synchronous byte stream for command results, banners, and interpreter **`Error: …`** lines (unchanged from today: errors go to the primary output channel in M12).
- **Error/diagnostic channel (optional)** — separate stderr-like stream for login failures and host diagnostics (today: **`std::cerr`** in **`Main.cpp`**), not for relocating interpreter **`Error: …`** output unless explicitly changed in a later milestone.

Standalone mode binds these channels to in-process streams; M14 will replace the transport with remote console IPC. Implementation may use **`std::istream`** / **`std::ostream`** (or thin wrappers) — no new wire protocol in M12. **Synchronous, blocking I/O for M12** — no async multiplexing (cooperative scheduling is M15).

#### 2.6 Standalone mode

Standalone **`gemini-system`** becomes:

- A process hosting exactly one **`GeminiSession`**
- With in-process I/O channels
- With no daemon, no session table, and no concurrency

**Design constraint (carries into M13+):** standalone / **`gemini-system`** is **one session in the same model** (conceptually **`maxSessions = 1`**, in-process I/O), not a separate codebase or parallel execution path. The session table and daemon arrive in M13; M12 defines the object and I/O boundary they will hold.

---

### 3. Invariants and error boundaries

#### 3.1 Error handling boundary

Error formatting and reporting remain **interpreter-local**. **`GeminiSession` does not catch, reformat, or aggregate interpreter errors in M12** — it routes interpreter output through the session’s I/O channels without introducing new error-handling semantics. Today **`Shell`** and language runtimes catch exceptions and write **`Error: …`** to the output channel; that behaviour is unchanged. **`GeminiSession` does not introduce a new error envelope or cross-interpreter error policy.** Transport-level error framing for remote consoles is M14 scope.

#### 3.2 Session invariants

A session guarantees:

- Exactly one **`Runtime`** instance
- Exactly one interpreter stack
- Exactly one active account binding (or none, if detached)
- Exclusive ownership of its I/O channels
- Isolation of VM state from other sessions (future-proofing for M15)

These invariants must hold even in standalone mode.

---

### 4. Non-goals

Milestone 12 does **not** introduce:

- A daemon
- A session table
- Multi-session scheduling
- Remote consoles
- Any change to Tcl/BASIC/PROC semantics
- Any change to filesystem or locking behaviour

These belong to M13–M17.

---

### 5. Compatibility expectations

External behaviour of **`gemini-system`** remains unchanged — only internal composition changes:

- Login flow unchanged
- Command semantics unchanged
- Interpreter behaviour unchanged
- Error messages and formatting unchanged (see §3.1)
- Locking semantics unchanged ([Milestone 10](10-record-locking-multi-user.md))
- Multi-language runtime behaviour unchanged ([Milestone 11](11-multi-language-runtime-infrastructure.md))

---

### 6. Grounding and dependencies

- Builds on [Milestone 2](02-multi-account-single-session.md) (account context) and [Milestone 10](10-record-locking-multi-user.md) (per-session lock identity) — formalize and unify, not reinvent.
- **Boot ritual stays process-level for M12:** [`BootMonitor`](../../src/core/boot/BootMonitor.h) and the frozen **`LanguageRegistry`** ([Milestone 11](11-multi-language-runtime-infrastructure.md)) are attached at process scope. [`Main.cpp`](../../src/Main.cpp) creates **`GeminiSession`**, runs cold start against the session **`Runtime`**, then attaches the registry before the login/REPL loop; daemon scope is M13.
- **External behaviour unchanged:** **`gemini-system`** should look the same to users after M12.

---

### 7. Deliverables

- New **`GeminiSession`** type
- Refactored login flow using session I/O
- Refactored shell/interpreter stack living inside the session
- Removal of direct **`std::cin`** / **`std::cout`** usage on production login/REPL paths
- Updated [`Main.cpp`](../../src/Main.cpp) to host a single session
- Documentation of lifecycle and invariants (this milestone doc)
- Tests: session lifecycle unit tests; existing shell/login tests remain green; I/O injectable via session channels

---

### 8. Milestone completion criteria

- No direct **`std::cin`** / **`std::cout`** on login/REPL production code paths (tests excepted; process boot **`stdout`** unchanged)
- **`gemini-system`** manual smoke: login → Tcl → BASIC → LOGOFF → re-login → QUIT (see checklist below)
- All tests pass (892+ at close)
- Per-session lock identity and M11 language dispatch unchanged from an operator perspective

**Manual smoke checklist:**

1. Boot with catalogue → interactive or auto LOGON
2. Tcl: `VERSION`, `WHO`
3. Enter BASIC → run a one-line program → `END`
4. `LOGOFF` → re-login
5. `QUIT` → clean process exit

---

### Delivery plan

Implementation is sequenced into vertical stages. Each stage ships a test-locked slice before the next starts; the full test suite must remain green after every stage. Introduce **`GeminiSession`** and **`Runtime`** ownership first; move **`Shell`** under session ownership and update the host early; add session I/O channels second; route login and REPL through session I/O third; absorb **`ShellSession`** and fold **`UserSession`** fourth; expose explicit lifecycle APIs fifth; finish with documentation and milestone close sixth. Detailed per-stage plans may live in `~/.cursor/plans/m12_stage_*.plan.md` during implementation; status is summarised here as each stage lands (matching the M8 / M9 / M10 / M11 precedent).

- **Stage 1 — Session skeleton + ownership**: introduce **`GeminiSession`**; own **`Runtime`** and construct or own **`Shell`** (interpreter front-end under session ownership, not duplicated inside the session type). Update [`Main.cpp`](../../src/Main.cpp) to host exactly one session after process-level boot ([`BootMonitor`](../../src/core/boot/BootMonitor.h), **`LanguageRegistry`** attach). External behaviour unchanged; may temporarily delegate to existing **`ShellSession`** internals. Implementation: [`GeminiSession`](../../src/userland/tcl/GeminiSession.h), [`Main.cpp`](../../src/Main.cpp), tests in [`tests/tcl/test_GeminiSession.cpp`](../../tests/tcl/test_GeminiSession.cpp). *Status: implemented.*

- **Stage 2 — Session I/O channels**: add session-owned I/O (input, output, optional diagnostic stream per §2.5); default to process **`std::cin`** / **`std::cout`** / **`std::cerr`**; wire existing **`Shell`** / **`Runtime`** stream hooks through the session. No login/REPL path migration yet beyond defaults. Implementation: I/O API and **`bindIoToShellAndRuntime`** on [`GeminiSession`](../../src/userland/tcl/GeminiSession.h); tests in [`tests/tcl/test_GeminiSession.cpp`](../../tests/tcl/test_GeminiSession.cpp). *Status: implemented.*

- **Stage 3 — Login + REPL on session I/O**: refactor [`LoginService`](../../src/core/login/LoginService.h) caller and [`Shell::runTclRepl`](../../src/userland/tcl/Shell.cpp) to use session channels; remove direct **`std::cin`** / **`std::cout`** on production login/REPL paths. Implementation: [`Shell::setOutputStream`](../../src/userland/tcl/Shell.h), [`Main.cpp`](../../src/Main.cpp) login via `session.input()` / `output()` / `diagnostic()`, REPL test in [`tests/tcl/test_GeminiSession.cpp`](../../tests/tcl/test_GeminiSession.cpp). *Status: implemented.*

- **Stage 4 — Absorb `ShellSession`**: migrate mutable **`ShellSession`** fields into **`GeminiSession`** (filesystem root, Tcl env, lock binding, active list, login flags, breakpoint/suspend state); **`Shell`** holds **`GeminiSession&`** instead of embedding **`ShellSession`**; implement **`attach(const UserSession&)`** on the session; delete **`ShellSession`** type. Preserve per-session lock identity ([`tests/tcl/test_GeminiSessionLocks.cpp`](../../tests/tcl/test_GeminiSessionLocks.cpp), [`tests/tcl/test_ShellLocking.cpp`](../../tests/tcl/test_ShellLocking.cpp)). *Status: implemented.*

- **Stage 5 — Lifecycle API**: explicit **`create`** / **`attach`** / **`detach`** / **`reset`** / **`destroy`** on **`GeminiSession`**; align **`reset`** with today’s **`ShellSession::resetForQuit()`** and **`detach`** with **`clearLoginSession()`** semantics (§2.3). Session lifecycle unit tests. *Status: implemented.*

- **Stage 6 — Docs + closes M12**: satisfy §8 completion criteria; update milestone status; architecture reference in [`docs/session.md`](../session.md) and cross-links in [`docs/`](../). Manual smoke checklist in §8. **Closes Milestone 12.** *Status: implemented.*

Only Stage 6's exit criteria should claim "Closes Milestone 12".

*Status: implemented.*
