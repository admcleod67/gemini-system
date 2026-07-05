← [Project milestones index](../milestones.md)

## Milestone 18 — Version 1.0 Release: Gemini System Service

Deliver the first stable **Version 1.0** Gemini System: a Pick-authentic, multi-session **service edition** and a matching **application edition**, both on the same architecture. Ship reliable daemon-based multi-session operation, integrated language libraries (Tcl, BASIC, PROC, assembler shell), multi-attribute Pick filesystem semantics, and complete **architecture**, **admin**, and **developer** documentation. Define and meet public **release criteria** for the repository and deliverables. *Status: planned.*

### Release scope

M18 is a **stabilization and documentation capstone**—not a bucket for new architecture. Avoid sneaking in telnet, SQL, distributed sessions, or transaction semantics; those belong in post-1.0 milestones.

**Dual deliverables:**

- **Service edition** — **`gemini-daemon`** + **`gemini-console`**; multi-session Linux deployment ([**Milestones 13–17**](13-service-daemon-architecture.md))
- **Application edition** — **`gemini-system`**; single-session embedded host on the same architecture (delivered M12–M13; see [Milestone 16 retirement note](16-standalone-edition-application-mode.md))

### Positioning

**Version 1.0** means Pick-authentic multi-user **file and lock semantics** on Linux (UniData-style service deployment), not full R83 terminal/phantom fidelity. Gemini remains grounded in Pick authenticity while shifting deployment focus from a single developer process toward a production Linux service.

### Release criteria (outline)

- Test matrix covering multi-session locks, cooperative scheduling, and language module boot
- **Architecture**, **admin**, and **developer** guides — including **Service Edition** vs **Application Edition** operator docs (`gemini-daemon`/`gemini-console` vs `gemini-system`; no console failover)
- Repository hygiene: milestone history, clean packaging (M17 install splits), public release checklist

### Out of scope for v1.0 (explicit)

- Remote access beyond local Unix domain sockets (SSH/telnet post-1.0)
- Hot-reload of language modules without daemon restart
- Session restore across cold daemon restart (unless explicitly delivered in M17)
- Transaction semantics, SQL gateways, or other post-Pick extensions
