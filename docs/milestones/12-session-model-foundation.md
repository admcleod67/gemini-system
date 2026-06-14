← [Project milestones index](../milestones.md)

## Milestone 12 — Session Model Foundation

Introduce a formal session abstraction without yet introducing a daemon or concurrency. Refactor today’s split between **`UserSession`**, **`ShellSession`**, and process-level **`Runtime`** / I/O wiring into a single session boundary: account binding, Tcl interpreter, BASIC/PROC runtime, VM state, and I/O channels. Define session lifecycle (create, destroy, reset, attach, detach) and route console I/O through a session interface. Standalone mode continues to run exactly one active session internally. Execution remains single-threaded with no multi-session concurrency. *Status: planned.*
