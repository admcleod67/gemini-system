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

## Milestone 2 — Multi-account (single session): catalogue, account context & dictionary model

**Goal:** Add a Pick-authentic **session** and **account** model on top of the existing core. **Accounts** are first-class: each session has one **current account** (Pick root, MD/VOC bindings, and runtime state). **Distinct user identities** (separate from account, `USERS` catalogue, default-account selection) are **out of scope** for the delivered Milestone 2 path and remain roadmap. **No concurrency or locking**—one active session per process is acceptable for this milestone.

The host hands **`PickCore::UserSession`** into userland; today **`username`** and **`accountName`** mirror the **account id** until a real user catalogue exists.

**Catalogue & host layout**

- **`ACCOUNTS` catalogue (host-backed, e.g. JSON):** Maps account names to host directories (account → Pick root / VOC path). This is what **console `LOGON`** uses today; see **`docs/gemini-bootstrap.md`**.
- **Per-account directory layout:** Under the catalogue, e.g. `gemini/accounts/<ACCOUNT>/` with **`MD`** and **`VOC`** (and minimal structure for command lookup).
- **`MD` / `VOC`:** Pick-style **attribute-per-line** text (not ad hoc blobs), with a **minimal MD/VOC structure per account** suitable for bootstrapping that account.
- **`USERS` catalogue (deferred):** Pick-style records (username, password hash, default account, privileges) and host-backed JSON (`USERS.json`) remain the **target** model; optional attachment for boot messaging may exist, but **logon does not depend on it** until user–account separation is implemented.

**Pointers & command resolution**

- **Full pointer semantics** where applicable for this milestone: **V, Q, D, A, X** (consistent with Pick dictionary behaviour).
- **TCL command resolution through VOC**—authentic Pick-style lookup (not a parallel shortcut resolver).

**Session model**

- Per-session state includes at least: **current account** (Pick root), **current VOC** (and MD binding), **default file pointers**, **TCL environment**, and **BASIC runtime state**.
- **Session system fields** (read-only, session-backed): **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**—aligned with Tcl/PROC/BASIC and catalogue logon. Further Pick-style bindings (**`@TTY`**, **`@PRIVILEGES`**, and richer MD-derived state) remain **stretch** targets for this milestone.

**Operator-facing flow**

- **Core boot:** **`PickCore::BootMonitor`** cold-start output, then catalogue **`LOGON`** (**`PickCore::LoginService::runCatalogLogin`**, **account id** only, optional **`MD,AUTO-LOGON`** / env auto-logon) in **`main`**—not inside the Tcl REPL. Tcl starts only after **`Shell::attachUserSession`** receives a **`UserSession`**. **`stdout`** cadence (interactive **`LOGON PLEASE:`** vs auto-logon **`endl`** behaviour, blank line before the Tcl banner) is spelled out under **`docs/gemini-bootstrap.md`**; **`runTclRepl`** has **no** leading newline.
- **`LOGTO`:** Switch account within an existing Tcl session, reload Pick root / MD/VOC context, reset session state for the new account.
- **`WHO`** and **`LOGOFF`:** Minimal introspection and clean session teardown; **`LOGOFF`** returns the host to the core **`LOGON`** phase (**no nested Tcl login loop**).

**Milestone 2 residual fidelity items (deferred)**

- **Dictionary depth:** full Pick **`D`**-item data dictionary semantics; **`X`** execute-body behaviour; deeper/expanded **`Q`** behaviour beyond the current resolver walks.
- **Verb model:** executing verbs that exist only in **`VOC`** without a corresponding built-in Tcl handler.
- **MD model:** full MD dictionary authoring/resolution beyond **`MD,DEFDATA`**, including broader MD-derived session behaviour.
- **BASIC file semantics:** defaulting `OPEN`/file-variable behaviour from MD-derived defaults.
- **Session identity richness:** additional session fields such as **`@TTY`** / **`@PRIVILEGES`** and related policy/state.
- **User model:** distinct **`USERS`**-driven identities (separate from account) remain roadmap.

## Milestone 3 — ENGLISH core, DICT evolution & filesystem maturation

**Goal:** Introduce a minimal, Pick-authentic **ENGLISH** core (`LIST`, `SORT`, `COUNT`, `SELECT`) and use it to drive a stable **DICT** and record-access model for later processors.

**Milestone slices (delivery order)**

- **M3a — DICT + record API groundwork:** Land dictionary lookup, attribute-aware extraction primitives, and a stable record API used by Tcl/BASIC/ENGLISH.
- **M3b — ENGLISH query core:** Implement `LIST`, `COUNT`, and `SELECT` on top of the M3a primitives.
- **M3c — Ordering + list lifecycle:** Add `SORT`, active-list lifecycle commands, and resolver/debug ergonomics needed to operate ENGLISH predictably.

**ENGLISH subsystem (minimal core)**

- Implement **`LIST`**, **`SORT`**, **`COUNT`**, **`SELECT`** as an ENGLISH processor module.
- Route Tcl dispatch through VOC into ENGLISH (no parallel command path).
- No report-formatting layer yet: no headings, breaks, totals, pagination, or printer semantics.

**DICT model evolution**

- Support **A-type** attributes (attribute number and extraction rules).
- Support **S-type** synonyms (simple indirection).
- Support a minimal conversion subset (**`D`**, **`MD`**, **`MC`**) for numeric/date coercion.
- Define and document deterministic DICT lookup precedence (file name, `DICT` file, attribute fallback).

**Filesystem maturation**

- Attribute-aware record access for multi-attribute / multi-value extraction.
- Clean separation between raw record storage and DICT-driven interpretation.
- Stable record API shared by ENGLISH, BASIC, and future processors.
- Session-scoped active-list storage as the base for `SELECT`/`SSELECT` evolution.
- Detailed filesystem model/spec for this milestone is captured in **`docs/filesystem-m3.md`**.

**TCL integration**

- `LIST` / `SORT` / `COUNT` / `SELECT` routed through VOC and dispatched to ENGLISH.
- Minimal active-list operations: **`LIST-LIST`** and **`CLEAR-LIST`**.
- Resolver introspection improvements as needed for ENGLISH diagnostics.

**Processor architecture**

- ENGLISH implemented as a standalone module with explicit subcomponents:
  - Parser
  - Dictionary resolver
  - Planner
  - Executor
- Formatter remains deferred to a later milestone.
- This establishes the long-term processor pattern without requiring dynamic registration in M3.

**Non-goals (explicitly deferred)**

- Formatting/report layer: `HEADING`, `FOOTING`, `BREAK-ON`, `TOTAL`, `ID-SUPP`, pagination.
- Printer/report delivery semantics.
- Computed attributes (e.g. F/I-type and correlative expression support).
- Full `SSELECT` semantics (until sorting/list behaviour matures).
- SQL-like or extended ENGLISH feature surface.
- Concurrency / multi-user support.
- Dynamic processor registration.

## Milestone 4 — R83-flavoured fidelity pass

- Improved TCL ergonomics
- Optional quoting and tokenisation refinements
- VOC authoring from within the system
- BASIC and PROC polish
- Decision on long-term VOC format (canonical, structured, or hybrid)
