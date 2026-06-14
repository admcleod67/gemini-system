← [Project milestones index](../milestones.md)

## Milestone 14 — Multi-Session Console Support

Allow multiple consoles to attach to the daemon, each bound to its own session. Ship a **`gemini-console`** client that connects over IPC, with a protocol to request a new session or attach to an existing one. Multiplex console input and output to the correct session; keep interpreter and runtime state isolated per session. Support graceful detach so a console can disconnect without terminating the session. Execution remains serial across sessions until Milestone 15. *Status: planned.*
