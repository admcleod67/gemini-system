← [Project milestones index](../milestones.md)

## Milestone 13 — Service Daemon Architecture

Introduce the **Gemini Service Daemon (GSD)** as a long-running process that owns the VM substrate, Pick filesystem, boot-time **language registry**, shared **lock table**, and a **session table**. Add a minimal **IPC layer** (Unix domain sockets) for future console attachment, plus daemon lifecycle (start, stop, configuration). Sessions may be created and held in the table, but only one runs at a time—no cooperative scheduling yet. Security is minimal: host file permissions and socket access control. *Status: planned.*
