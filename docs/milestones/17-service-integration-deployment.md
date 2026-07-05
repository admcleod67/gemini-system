← [Project milestones index](../milestones.md)

## Milestone 17 — Service Integration & Deployment

Make Gemini a first-class **Linux service**. Add a **systemd** unit (`gemini.service`) with proper lifecycle integration, **journald** logging for daemon output, and configuration for socket paths, session limits, and related host settings. Expose **admin Tcl commands** such as **`LISTSESSIONS`**, **`KILLSESSION`**, and **`STATUS`**. Implement **graceful shutdown**: flush filesystem state and release session resources cleanly. *Status: planned.*

### Service integration

- **`gemini.service`** systemd unit with start/stop/restart lifecycle.
- **journald** integration for daemon stdout/stderr and boot summary lines (including M11 **`MODULES:`** boot log).
- **Configuration file** for socket paths, session limits, gemini/catalogue root, and related host settings.

### Admin surface

- **`LISTSESSIONS`**, **`KILLSESSION`**, **`STATUS`** (and related introspection) exposed via Tcl admin commands.
- Extend the **`SYSTEM`** introspection direction from [**Milestone 11**](11-multi-language-runtime-infrastructure.md) (e.g. **`SYSTEM LANGUAGES`**) to cover service and session state.

### Application edition packaging

Residual scope from the retired [**Milestone 16**](16-standalone-edition-application-mode.md) (architecture already delivered as **`gemini-system`** in M12–M13):

- **Service edition install** — `gemini-daemon`, `gemini-console`, bootstrap/modules tree, systemd unit, config file defaults.
- **Application edition install** — `gemini-system`, bootstrap/modules tree; no daemon unit or IPC socket.
- Clear separation in packaging/docs: **`gemini-console`** requires a running daemon (connection failure is an error; no embedded fallback).

### Grounding

- **Graceful shutdown:** flush filesystem state, release locks held by terminating sessions, tear down session objects cleanly.
- **Session persistence across daemon restart (TBD):** graceful shutdown may save in-flight state, but cold restart likely means fresh sessions unless explicitly designed otherwise—document the chosen behaviour.
