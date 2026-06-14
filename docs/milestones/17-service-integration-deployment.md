← [Project milestones index](../milestones.md)

## Milestone 17 — Service Integration & Deployment

Make Gemini a first-class **Linux service**. Add a **systemd** unit (`gemini.service`) with proper lifecycle integration, **journald** logging for daemon output, and configuration for socket paths, session limits, and related host settings. Expose **admin Tcl commands** such as **`LISTSESSIONS`**, **`KILLSESSION`**, and **`STATUS`**. Implement **graceful shutdown**: flush filesystem state and release session resources cleanly. *Status: planned.*
