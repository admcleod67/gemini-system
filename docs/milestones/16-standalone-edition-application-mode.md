← [Project milestones index](../milestones.md)

## Milestone 16 — Standalone Edition / Application Mode

Build the single-session **application edition** on top of the multi-session architecture rather than a separate codebase. Provide a **standalone wrapper** that launches the daemon in-process, creates one session, and binds the console directly (bypassing IPC for speed). Preserve the same interactive experience as today’s **`gemini-system`**. Optionally fall back to standalone when no system service is running. Package as a clean **Gemini Application Edition** deliverable. *Status: planned.*

### Application edition

- **Standalone wrapper:** in-process daemon (or equivalent session host) with **`maxSessions = 1`** and direct console I/O—no Unix socket hop.
- **Same UX** as today’s **`gemini-system`**: login flow, Tcl REPL, BASIC/PROC/ENGLISH behaviour unchanged from the user’s perspective.
- **Failover (optional):** if the system service is not running, launch application mode locally instead of requiring a running daemon.

### Grounding

- **Likely a thin milestone** if [**Milestone 12**](12-session-model-foundation.md) delivers “standalone = one session in the same model.” Most work is packaging, a direct I/O fast path, and a distinct deliverable name—not new architecture.
- **Open:** fold application-edition packaging into [**Milestone 17**](17-service-integration-deployment.md) as a second build target if M16 scope stays minimal.
- Not a fork: the application edition is one session in the same daemon/session architecture introduced in M12–M13.
