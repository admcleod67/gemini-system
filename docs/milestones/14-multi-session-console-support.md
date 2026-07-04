← [Project milestones index](../milestones.md)

## Milestone 14 — Multi-Session Console Support

Allow multiple consoles to attach to the daemon, each bound to its own session. Ship a **`gemini-console`** client that connects over IPC, with a protocol to request a new session or attach to an existing one. Multiplex console input and output to the correct session; keep interpreter and runtime state isolated per session. Support graceful detach so a console can disconnect without terminating the session. Execution remains serial across sessions until Milestone 15. *Status: planned.*

### Console attachment

- **`gemini-console`** connects to the daemon over the M13 IPC layer (Unix domain socket).
- Each console runs catalogue **login** (`LOGON PLEASE:` flow) over IPC and receives an isolated Tcl/shell REPL for its session.
- **Graceful detach:** disconnecting a console does not destroy the session object in the daemon’s session table.

### Execution model

This is the first milestone where Gemini **feels like a multi-user Pick system** (UniData-style): multiple consoles, multiple sessions, but **still serial execution**—only one session runs at a time until [**Milestone 15**](15-cooperative-multi-session-execution.md).

### Grounding

- Assign stable **port / session id** when a console creates or attaches to a session — port assignment policy is defined in [**Milestone 13**](13-service-daemon-architecture.md) Stage 3 (`PortManager`); M14 consumes it at console attach time.
- **Authentication per console (TBD):** each **`gemini-console`** runs full catalogue login over IPC, or the daemon trusts the local socket user—decide before implementation.
- **Remote access:** Unix socket only for v1; SSH/telnet is post-1.0.
- Session isolation reuses the M12 session boundary: separate interpreter, VM state, and I/O channels per session.
