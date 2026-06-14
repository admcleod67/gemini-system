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

**Type migration (TBD):** prefer **absorption** of **`UserSession`** / **`ShellSession`** into **`GeminiSession`** rather than a wrapper hierarchy; familiar internal names may remain where they reduce churn, but ownership and lifecycle move to the session boundary.

**Lock binding:** the session exposes the per-session lock context today wired through **`ShellSession`** (shared **`LockTable`**, session lock id on login per [Milestone 10](10-record-locking-multi-user.md)). M12 must not break per-session lock identity.

#### 2.2 Session lifecycle

Define explicit, deterministic lifecycle transitions (later driven by the daemon in M13):

| Transition | Semantics |
|------------|-----------|
| **create** | Allocate VM, initialise interpreters, create I/O channels |
| **attach** | Bind an authenticated account |
| **detach** | Remove account binding without destroying the session |
| **reset** | Clear interpreter state and VM state while retaining I/O |
| **destroy** | Full teardown of the session |

#### 2.3 I/O boundary

Replace all direct use of **`std::cin`** / **`std::cout`** on **production paths** with a session-owned I/O interface. This includes:

- **`LoginService`**
- **`Shell::runTclRepl`**
- Any interpreter path that currently writes directly to the console

Tests may continue to inject streams directly (existing **`Shell`** / **`Runtime`** stream hooks). This is the most important architectural seam for future multi-session support.

#### 2.4 Standalone mode

Standalone **`gemini-system`** becomes:

- A process hosting exactly one **`GeminiSession`**
- With in-process I/O channels
- With no daemon, no session table, and no concurrency

**Design constraint (carries into M13+):** standalone / **`gemini-system`** is **one session in the same model** (conceptually **`maxSessions = 1`**, in-process I/O), not a separate codebase or parallel execution path. The session table and daemon arrive in M13; M12 defines the object and I/O boundary they will hold.

---

### 3. Invariants

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

*Status: planned.*
