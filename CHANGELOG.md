# Changelog

All notable releases of Gemini System are documented here.
Older `v0.x` annotated tags (before 0.17.0) remain in git history.

## [1.0.0] - 2026-07-11

First stable **Version 1.0** release ([Milestone 18](docs/milestones/18-version-1-gemini-system-service.md)).

- **Dual editions** on one architecture: **Application** (`gemini-system`) and **Service** (`gemini-daemon` + `gemini-console`)
- Multi-session **record locks**, cooperative **I/O yield** scheduling, language-module boot
- Linux service path: daemon config file, journald-ready logging, `gemini.service`, SYSPROG admin Tcl (`LISTSESSIONS` / `STATUS` / `KILLSESSION` / `SHUTDOWN`), CMake **Runtime** / **Application** / **Service** install components
- Operator docs: edition glossary, Application→Service migration, session-end contrast, cold restart = fresh sessions
- **Known limitations (v1.0):** CPU-bound multi-session starvation (later: [Milestone 21](docs/milestones/21-execution-fairness-cpu-bound-yield.md)); cold restart does not restore sessions; local Unix domain sockets only

Operator detail: [`docs/daemon.md`](docs/daemon.md).

## [0.17.0] - 2026-07-11

Release 0.17.0 — Service integration & deployment (Milestone 17).

Ships daemon config file, journald-ready logging, systemd unit, SYSPROG
admin Tcl (LISTSESSIONS / STATUS / KILLSESSION / SHUTDOWN), Service vs
Application install components, and operator docs for session lifecycle
and cold restart.
