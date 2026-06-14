← [Project milestones index](../milestones.md)

## Milestone 12 — Session Model Foundation

Introduce a formal session abstraction without yet introducing a daemon or concurrency. Refactor today’s split between **`UserSession`**, **`ShellSession`**, and process-level **`Runtime`** / I/O wiring into a single session boundary: account binding, Tcl interpreter, BASIC/PROC runtime, VM state, and I/O channels. Define session lifecycle (create, destroy, reset, attach, detach) and route console I/O through a session interface. Standalone mode continues to run exactly one active session internally. Execution remains single-threaded with no multi-session concurrency. *Status: planned.*

### Refactor target

Today **`Main.cpp`** owns one **`Runtime`**, one **`Shell`**, and pipes login directly to **`std::cin`** / **`std::cout`**. **`ShellSession`** holds mutable shell state but does not own the VM; **`UserSession`** is an immutable login handoff attached after the fact. M12 unifies this split—it does **not** introduce a parallel session layer alongside the existing types.

Introduce a single session type (name TBD, e.g. **`GeminiSession`**) that **owns**:

- **`Runtime`** — today allocated in **`Main.cpp`**, referenced by **`ShellSession`**
- **Shell / interpreter stack** — Tcl, BASIC, PROC, and assembler paths today split across **`Shell`** + **`ShellSession`**
- **Session I/O channels** — replace direct **`std::cin`** / **`std::cout`** in **`LoginService`**, **`Shell::runTclRepl`**, and related paths (build on existing optional stream hooks in **`Shell`** / **`Runtime`** where appropriate)
- **Account binding** — fields currently carried in **`UserSession`** and applied via **`attachUserSession`**

**Design constraint (carries into M13+):** standalone / **`gemini-system`** is **one session in the same model** (conceptually **`maxSessions = 1`**, in-process I/O), not a separate codebase or parallel execution path. The session table and daemon arrive in M13; M12 defines the object and I/O boundary they will hold.

### Grounding

- Builds on existing [**Milestone 2**](02-multi-account-single-session.md) account context and [**Milestone 10**](10-record-locking-multi-user.md) per-session lock identity—formalize and unify, not reinvent.
- **Boot ritual stays process-level for M12:** **`BootMonitor`** and the frozen **`LanguageRegistry`** ([**Milestone 11**](11-multi-language-runtime-infrastructure.md)) remain outside the session; daemon scope is M13.
- **External behaviour unchanged:** **`gemini-system`** should look the same to users after M12; only internal composition changes.
- **Highest-leverage early decision:** get this boundary right and M13–M17 become mostly plumbing; get it wrong and two systems must be maintained.
