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

## Milestone 2 — Multi-user (non-concurrent): account & dictionary model

**Goal:** Add a Pick-authentic **session**, **account**, and **login** model on top of the existing core: users and accounts are first-class; each session has its own account context, MD/VOC bindings, and runtime state. **No concurrency or locking**—single active session per process (or equivalent) is acceptable for this milestone.

**Catalogue & host layout**

- **`USERS` file:** Pick-style attribute records (e.g. username, password hash, default account, privileges). Initial implementation may remain host-backed (e.g. JSON) as long as the record shape and access path match the intended Pick model.
- **`ACCOUNTS` file:** Maps account names to host directories (e.g. account → VOC path / account root).
- **Per-account directory layout:** Under the host, e.g. `/gemini/accounts/<ACCOUNT>/` with **`MD`** and **`VOC`** (and any minimal structure required for command lookup).
- **`MD` / `VOC`:** Implemented as Pick-style **attribute-per-line** text files (not ad hoc blobs), with a **minimal MD/VOC structure per account** suitable for bootstrapping that account.

**Pointers & command resolution**

- **Full pointer semantics** where applicable for this milestone: **V, Q, D, A, X** (consistent with Pick dictionary behaviour).
- **TCL command resolution through VOC**—authentic Pick-style lookup (not a parallel shortcut resolver).

**Session model**

- Per-session state includes at least: **current account**, **current VOC** (and MD binding), **default file pointers**, **TCL environment**, and **BASIC runtime state**.
- **Session variables / bindings** aligned with a Pick-style model (e.g. **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, **`@TTY`**, **`@PRIVILEGES`**, plus MD/VOC bindings as implied by the account load).

**User-facing flow**

- **Core boot:** **`PickCore::BootMonitor`** cold-start output, then catalogue-backed **LOGON** (**`PickCore::LoginService`**, account-based, optional **`MD,AUTO-LOGON`**) in **`main`**, not part of the Tcl REPL; Tcl starts only after a **`UserSession`** exists. **`stdout`** cadence: interactive vs cold-start **`MD`/env auto** (echo + **`endl`**) spelled out under **`docs/gemini-bootstrap.md`**; **`runTclRepl`** banner has **no** leading newline.
- **`LOGIN`:** Load user, apply default account, load MD/VOC, initialise session context (pointers, TCL, BASIC as needed).
- **`LOGTO`:** Switch account, reload MD/VOC, reset session context for the new account.
- **`WHO`** and **`LOGOFF`:** Minimal introspection and clean session teardown / logout.

## Milestone 3 — TCL & filesystem maturation

- Additional TCL verbs beyond the Milestone 1 set (e.g. catalog/maintenance commands not covered today—distinct from existing **`LIST`** *file*, **`LIST-FILES`**, **`LIST-PROGRAMS`**, **`DELETE-FILE`**, …)
- Improved `READ` / `WRITE` behaviour
- Editor refinements
- Resolver introspection commands
- Early groundwork for attribute-aware records (no format changes yet)

## Milestone 4 — R83-flavoured fidelity pass

- Improved TCL ergonomics
- Optional quoting and tokenisation refinements
- VOC authoring from within the system
- BASIC and PROC polish
- Decision on long-term VOC format (canonical, structured, or hybrid)
