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

### Grounding

- **Graceful shutdown:** flush filesystem state, release locks held by terminating sessions, tear down session objects cleanly.
- **Session persistence across daemon restart (TBD):** graceful shutdown may save in-flight state, but cold restart likely means fresh sessions unless explicitly designed otherwise—document the chosen behaviour.
- May include **application edition packaging** ([**Milestone 16**](16-standalone-edition-application-mode.md)) as a second build/deliverable target if M16 scope is folded in.
