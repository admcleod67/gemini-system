← [Project milestones index](../milestones.md)

## Milestone 6 — File‑Backed HELP System

This milestone replaces the current built‑in HELP topic table with a Pick‑authentic, file‑backed HELP subsystem. HELP topics become ordinary Pick records stored in a logical file (**HELP**) within each account, with fallback to system‑level HELP topics in SYSPROG. This milestone introduces no VM or BASIC changes and requires no filesystem model changes beyond normal record I/O. It strengthens the TCL command layer and aligns Gemini with classic Pick documentation workflows.

### Goals

- Replace hardcoded HELP topics with record‑based HELP files.
- Support account‑local HELP overrides with fallback to system HELP.
- Provide a clean, extensible foundation for future documentation features.
- Preserve Pick‑authentic behaviour: HELP is a TCL‑layer feature, not a BASIC or VM concern.
- Avoid introducing new filesystem primitives; rely entirely on the M3 structured record model.

---

### Scope

#### 1. HELP file model (Pick‑authentic)

- Introduce a logical file named **HELP** in each account.
- HELP topics stored as one record per topic; record ID = topic name (uppercase).
- Topic body stored as newline‑delimited text (standard `.item` format).
- No attribute semantics required; HELP records treated as raw text.

#### 2. HELP lookup chain

Implement a deterministic lookup order:

1. Account‑local HELP file — `<current-account>/HELP/<topic>`
2. System HELP file — `SYSPROG/HELP/<topic>`
3. Built‑in fallback topics — minimal set for bootstrapping (e.g. `HELP`, `HELP HELP`, `HELP LIST`)

This mirrors Pick behaviour: local overrides first, system defaults second.

#### 3. HELP command enhancements

- **`HELP <topic>`**: resolve topic via lookup chain; print record body verbatim; case‑insensitive topic matching.
- **`HELP`** (no arguments): print short usage summary; optionally list available topics (stretch).
- **Error behaviour**: unknown topic → `No help available for "<topic>".`

#### 4. Optional authoring commands (Stage 1 minimal)

- **`HELP-LIST`**: list available HELP topics in the current account.
- **`HELP-EDIT <topic>`**: open the topic in the line editor (`ED>`); create the topic if missing.

These commands are optional but strongly recommended for usability.

#### 5. Account bootstrap integration

- Ensure new accounts created via catalogue bootstrap include an empty HELP file.
- SYSPROG account ships with a minimal set of HELP topics (simple `.item` files):
  - `HELP`
  - `HELP HELP`
  - `HELP LIST`
  - `HELP TCL`
  - `HELP BASIC`
  - `HELP PROC`
  - `HELP ENGLISH`

#### 6. TCL command layer adjustments

- Replace built‑in HELP table with file‑backed lookup.
- Improve quoting and tokenisation around HELP arguments (depends on Milestone 5).
- Ensure HELP works identically inside PROC (`TCL HELP …`).

#### 7. Documentation updates

- Update `docs/tcl-shell.md` to describe file‑backed HELP behaviour.
- Add `docs/help-system.md` describing topic format and lookup rules.
- Update [`docs/milestones.md`](../milestones.md) and [`06-file-backed-help-system.md`](06-file-backed-help-system.md) for Milestone 6 delivery status.

### Milestone 6 delivery status (implemented)

- **`HelpTopics`** resolver in `src/userland/tcl/HelpTopics.cpp`: local `HELP` → catalogue `accounts/SYSPROG/HELP` (when catalogue root is set and roots differ) → builtins; PickFS name rules force **space → underscore** mapping for multi-word canonical keys (`HELP_BASIC.item` for **`HELP BASIC`**).
- **`HELP`**, **`HELP-LIST`**, **`HELP-EDIT`** on the Tcl surface; **`HELP`** no-args resolves topic **`HELP`** first, else a short static line; **`TCL HELP`** (PROC bridge) shares the same resolver.
- Committed bootstrap topics under **`gemini/accounts/SYSPROG/HELP/`**; **[`docs/help-system.md`](../help-system.md)** documents lookup, keys, and authoring; [`docs/tcl-shell.md`](../tcl-shell.md) and [`docs/gemini-bootstrap.md`](../gemini-bootstrap.md) updated.
- **`LoginService`** rejects `HELP-LIST` / `HELP-EDIT` as catalogue account names alongside other reserved tokens.

---

### Non‑Goals (Deferred)

- No VM or BASIC changes.
- No new filesystem primitives.
- No formatting layer (bold, headings, pagination).
- No hierarchical HELP topics.
- No cross‑references or hyperlinks.
- No automatic topic generation from command metadata.
- No PROC‑level HELP enhancements beyond TCL bridging.

These belong to later documentation/UX milestones.

---

### Rationale

The HELP subsystem is currently the last major TCL‑layer feature still implemented as a static, compiled‑in table. Moving HELP to a file‑backed model:

- aligns Gemini with Pick’s documentation conventions,
- enables per‑account customisation,
- removes hardcoded text from the binary,
- allows HELP topics to evolve without code changes,
- and prepares the system for richer documentation and onboarding features.

Milestone 6 is intentionally narrow and vertical: it touches only the TCL command layer and the account bootstrap, without affecting BASIC, VM, ENGLISH, or the filesystem model. This keeps the milestone low‑risk and cleanly scoped.

