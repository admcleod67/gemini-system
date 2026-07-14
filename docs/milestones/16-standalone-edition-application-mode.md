← [Project milestones index](../milestones.md)

## Milestone 16 — Standalone Edition / Application Mode

*Status: **superseded** — delivered in [**Milestone 12**](12-session-model-foundation.md) and [**Milestone 13**](13-service-daemon-architecture.md); retired as a separate milestone.*

### Summary

M16 originally planned a single-session **application edition** on the same architecture as the multi-session service—not a separate codebase. That work landed with **`gemini-system`**: embedded [`GeminiServiceDaemon`](../src/core/daemon/GeminiServiceDaemon.h) via [`createEmbedded()`](../src/core/daemon/GeminiServiceDaemon.cpp), [`GeminiSessionHost`](../src/userland/tcl/GeminiSessionHost.h) with `maxSessions = 1`, one session, and direct `stdin`/`stdout`/`stderr` (no IPC server).

There is **no second standalone Pick executable** and **no client failover**: **`gemini-console`** attaches only to a running **`gemini-daemon`**; connection failure is an error, not a switch to embedded mode. (A Pick-independent bytecode runner is a separate post–v1.0 track — [**Milestone 19**](19-standalone-vm-runner.md).)

### Residual work (folded elsewhere)

| Topic | Where |
|-------|--------|
| Service vs application **install/packaging** (e.g. systemd unit ships daemon + console; application install is `gemini-system` + bootstrap) | [**Milestone 17**](17-service-integration-deployment.md) |
| **Edition naming**, operator docs, migration notes, v1.0 release checklist | [**Milestone 18**](18-version-1-gemini-system-service.md) |

### Deliverables (already shipped)

- **`gemini-system`** — Application Edition entry point ([`Main.cpp`](../../src/Main.cpp))
- **`gemini-daemon`** + **`gemini-console`** — Service Edition entry points (M14)
- Same login → Tcl REPL UX on both paths; shared session model and cooperative scheduler (M15)
