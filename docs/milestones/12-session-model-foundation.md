← [Project milestones index](../milestones.md)

## Milestone 12 — Session Model Foundation (Expanded)

Milestone 12 establishes the formal session abstraction that all later service-oriented milestones (M13–M17) depend on. It unifies today’s fragmented execution model — **`UserSession`**, **`ShellSession`**, **`Runtime`**, and direct console I/O — into a single, coherent boundary that represents one running Gemini System session. No daemon, concurrency, or multi-session scheduling is introduced yet; execution remains strictly single-threaded.

This milestone is the architectural hinge between the prototype and the service. A correct session boundary makes M13–M17 largely mechanical; an incorrect one forces the maintenance of two parallel systems.

---

### 1. Purpose and rationale

The current system distributes session-relevant state across multiple types:

- **`Runtime`** owned by [`Main.cpp`](../../src/Main.cpp)
- Shell/interpreter state split between **`Shell`** and **`ShellSession`**
- Account identity carried in **`UserSession`**
- I/O wired directly to **`std::cin`** / **`std::cout`** in several locations (including [`LoginService`](../../src/core/login/LoginService.h) and [`Shell::runTclRepl`](../../src/userland/tcl/Shell.cpp))

Milestone 12 consolidates these into a single **`GeminiSession`** object (working name) that owns:

- The VM (**`Runtime`**)
- The interpreter stack (Tcl, BASIC, PROC, assembler)
- The account binding
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

Introduce a new session type (working name: **`GeminiSession`**) that owns all state required to execute commands and programs. This unifies the current split between **`UserSession`**, **`ShellSession`**, and the process-level **`Runtime`** — it does **not** introduce a parallel session layer alongside the existing types.

**`GeminiSession`** owns:

- **`Runtime`** — today allocated in **`Main.cpp`**, referenced by **`ShellSession`**
- **Shell / interpreter stack** — Tcl, BASIC, PROC, and assembler paths today split across **`Shell`** + **`ShellSession`**
- **Session I/O channels** — replace direct **`std::cin`** / **`std::cout`** in **`LoginService`**, **`Shell::runTclRepl`**, and related paths (build on existing optional stream hooks in **`Shell`** / **`Runtime`** where appropriate)
- **Account binding** — fields currently carried in **`UserSession`** and applied via **`attachUserSession`**

**Lock binding:** the session exposes the per-session lock context today wired through **`ShellSession`** (shared **`LockTable`**, session lock id on login per [Milestone 10](10-record-locking-multi-user.md)). M12 must not break per-session lock identity.

#### 2.2 Type migration plan

Milestone 12 removes the historical split between **`UserSession`**, **`ShellSession`**, and the process-level **`Runtime`**. The migration path is explicit absorption — not a wrapper hierarchy — so a single point of ownership exists for all session-scoped state:

- **`UserSession`** becomes a lightweight account-metadata struct owned by **`GeminiSession`**. No independent lifecycle for bound account state; no separate owner. It may remain the login DTO returned by **`LoginService`** and passed to **`attach()`**.
- **`ShellSession` is removed.** Its mutable fields (filesystem root, Tcl environment, lock identity, active list, login flags, breakpoint/suspend state, etc.) migrate directly into **`GeminiSession`**.
- **`Runtime`** is owned by **`GeminiSession`** for the session lifetime (today allocated in **`Main.cpp`**).
- **`Shell`** remains the interpreter front-end but **no longer owns session state**; it receives a reference to the session (today it embeds **`ShellSession session_`**).

#### 2.3 Session lifecycle

Define explicit, deterministic lifecycle transitions (later driven by the daemon in M13):

| Transition | Semantics |
|------------|-----------|
| **create** | Allocate **one** **`Runtime`**, initialise interpreters, create I/O channels |
| **attach** | Bind an authenticated account (today: **`applyUserSession`**) |
| **detach** | Remove account binding without destroying the session (today: **`clearLoginSession`**) |
| **reset** | Clear interpreter and VM *program* state while retaining the same **`Runtime`** instance and I/O channels |
| **destroy** | Full teardown of the session; **`Runtime`** destroyed with the session |

**VM lifetime vs reset:** **`create`** allocates a single **`Runtime`** for the session’s entire lifetime. **`reset`** does **not** destroy or reallocate the VM — it performs an in-process clear equivalent to today’s **`ShellSession::resetForQuit()`** (release locks where applicable, clear Tcl env, active lists, breakpoints, and call **`Runtime::loadProgram({})`**). **`destroy`** tears down the session object and its **`Runtime`**. **`detach`** is narrower than **`reset`** (login/lock identity cleared; aligns with LOGOFF paths); **`reset`** aligns with QUIT / end-of-session cleanup that clears interpreter state for a fresh REPL cycle.

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
- **Boot ritual stays process-level for M12:** [`BootMonitor`](../../src/core/boot/BootMonitor.h) and the frozen **`LanguageRegistry`** ([Milestone 11](11-multi-language-runtime-infrastructure.md)) remain outside the session. The thin host ([`Main.cpp`](../../src/Main.cpp)) runs cold start and attaches the registry to the VM **before** session creation; daemon scope is M13.
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

- No direct **`std::cin`** / **`std::cout`** on login/REPL production code paths (tests excepted)
- **`gemini-system`** manual smoke unchanged: login → Tcl → BASIC → LOGOFF
- All existing tests pass
- Per-session lock identity and M11 language dispatch unchanged from an operator perspective

---

### Delivery plan

Implementation is sequenced into vertical stages. Each stage ships a test-locked slice before the next starts; the full test suite must remain green after every stage. Introduce **`GeminiSession`** and **`Runtime`** ownership first; move **`Shell`** under session ownership and update the host early; add session I/O channels second; route login and REPL through session I/O third; absorb **`ShellSession`** and fold **`UserSession`** fourth; expose explicit lifecycle APIs fifth; finish with documentation and milestone close sixth. Detailed per-stage plans may live in `~/.cursor/plans/m12_stage_*.plan.md` during implementation; status is summarised here as each stage lands (matching the M8 / M9 / M10 / M11 precedent).

- **Stage 1 — Session skeleton + ownership**: introduce **`GeminiSession`**; own **`Runtime`** and construct or own **`Shell`** (interpreter front-end under session ownership, not duplicated inside the session type). Update [`Main.cpp`](../../src/Main.cpp) to host exactly one session after process-level boot ([`BootMonitor`](../../src/core/boot/BootMonitor.h), **`LanguageRegistry`** attach). External behaviour unchanged; may temporarily delegate to existing **`ShellSession`** internals. Tests: existing shell and integration tests green. *Status: planned.*

- **Stage 2 — Session I/O channels**: add session-owned I/O (input, output, optional diagnostic stream per §2.5); default to process **`std::cin`** / **`std::cout`** / **`std::cerr`**; wire existing **`Shell`** / **`Runtime`** stream hooks through the session. No login/REPL path migration yet beyond defaults. Tests: session I/O injectable in unit tests. *Status: planned.*

- **Stage 3 — Login + REPL on session I/O**: refactor [`LoginService`](../../src/core/login/LoginService.h) and [`Shell::runTclRepl`](../../src/userland/tcl/Shell.cpp) (and related interpreter output paths) to use session channels; remove direct **`std::cin`** / **`std::cout`** on production login/REPL paths (tests excepted). Tests: existing [`tests/tcl/test_Shell.cpp`](../../tests/tcl/test_Shell.cpp) and login flows green. *Status: planned.*

- **Stage 4 — Absorb `ShellSession`**: migrate mutable **`ShellSession`** fields into **`GeminiSession`** (filesystem root, Tcl env, lock binding, active list, login flags, breakpoint/suspend state); **`Shell`** holds **`GeminiSession&`** instead of embedding **`ShellSession`**; implement **`attach(const UserSession&)`** on the session; delete **`ShellSession`** type. Preserve per-session lock identity ([`tests/tcl/test_ShellSessionLocks.cpp`](../../tests/tcl/test_ShellSessionLocks.cpp), [`tests/tcl/test_ShellLocking.cpp`](../../tests/tcl/test_ShellLocking.cpp)). *Status: planned.*

- **Stage 5 — Lifecycle API**: explicit **`create`** / **`attach`** / **`detach`** / **`reset`** / **`destroy`** on **`GeminiSession`**; align **`reset`** with today’s **`ShellSession::resetForQuit()`** and **`detach`** with **`clearLoginSession()`** semantics (§2.3). Session lifecycle unit tests. *Status: planned.*

- **Stage 6 — Docs + closes M12**: satisfy §8 completion criteria; update milestone status; optional architecture cross-links in [`docs/`](../). Manual smoke: login → Tcl → BASIC → LOGOFF unchanged. **Closes Milestone 12.** *Status: planned.*

Only Stage 6's exit criteria should claim "Closes Milestone 12".

*Status: planned.*
