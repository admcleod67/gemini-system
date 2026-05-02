# Project milestones

This document is an **orienting roadmap**. Priorities and scope may change as the Gemini System evolves.

## Milestone 1 — Core System (completed)

Roughly reflects what exists in the tree today:

- Minimal TCL shell
- Filesystem with directory-per-file and record blobs
- Line-oriented editor (system `EDIT` / `ED>` workflow, delegated from BASIC)
- BASIC compiler and runtime
- VOC resolver and command lookup
- Ability to run BASIC programs end-to-end

## Milestone 2 — TCL & filesystem maturation

- Additional TCL verbs beyond the Milestone 1 set (e.g. catalog/maintenance commands not covered today—distinct from existing **`LIST`** *file*, **`LIST-FILES`**, **`LIST-PROGRAMS`**, **`DELETE-FILE`**, …)
- Improved `READ` / `WRITE` behaviour
- Editor refinements
- Resolver introspection commands
- Early groundwork for attribute-aware records (no format changes yet)

## Milestone 3 — simple multi-user (non-concurrent)

- `LOGIN` command
- Per-user filesystem roots
- Per-user environment variables
- Shared or per-user VOC (TBD)
- No concurrency or locking required

## Milestone 4 — R83-flavoured fidelity pass

- Improved TCL ergonomics
- Optional quoting and tokenisation refinements
- VOC authoring from within the system
- BASIC and PROC polish
- Decision on long-term VOC format (canonical, structured, or hybrid)
