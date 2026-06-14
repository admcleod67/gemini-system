← [Project milestones index](../milestones.md)

## Milestone 13 — Service Daemon Architecture

Introduce the **Gemini Service Daemon (GSD)** as a long-running process that owns the VM substrate, Pick filesystem, boot-time **language registry**, shared **lock table**, and a **session table**. Add a minimal **IPC layer** (Unix domain sockets) for future console attachment, plus daemon lifecycle (start, stop, configuration). Sessions may be created and held in the table, but only one runs at a time—no cooperative scheduling yet. Security is minimal: host file permissions and socket access control. *Status: planned.*

### Daemon scope

M13 explicitly owns assets already established in prior milestones:

- **One shared `LockTable`** — process-wide, per-session ownership ([**Milestone 10**](10-record-locking-multi-user.md), today via **`LockRegistry`**)
- **One frozen `LanguageRegistry`** — loaded once at cold start ([**Milestone 11**](11-multi-language-runtime-infrastructure.md), post-**`BootMonitor`**)
- **Catalogue / gemini root** at daemon scope; individual sessions vary **account context** only
- **Boot monitor** — one-shot cold-start ritual at daemon level, not per console

**Defer hot-reload of language modules for v1.** M11 deliberately ruled out dynamic unload; “reload language libraries” means **restart the daemon**, not runtime module swap.

### Execution model (M13 vs M14 vs M15)

- **M13:** multiple session **objects** may exist in the session table; **serial execution** (only one runs at a time)
- **M14:** multiple **attached consoles**, still serial execution
- **M15:** multiple sessions can **make progress** via cooperative yield at I/O boundaries

Without this distinction, “multi-session” in M13 can be mistaken for M14 or M15.

### Grounding

- **PORT MANAGER** (today a boot stub) needs stable **port / session id** assignment for **`WHO`**, **`LISTSESSIONS`**, and lock identity—address in M13 or early M14, not M18.
- IPC is Unix domain sockets only for v1; no remote telnet/SSH in this milestone.
- Sessions created here use the unified session type from [**Milestone 12**](12-session-model-foundation.md).
