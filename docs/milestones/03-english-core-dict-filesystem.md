← [Project milestones index](../milestones.md)

## Milestone 3 — ENGLISH core, DICT evolution & filesystem maturation

**Goal:** Introduce a minimal, Pick-authentic **ENGLISH** core (`LIST`, `SORT`, `COUNT`, `SELECT`) and use it to drive a stable **DICT** and record-access model for later processors.

**Milestone slices (delivery order)**

- **M3a — DICT + record API groundwork:** Land dictionary lookup, attribute-aware extraction primitives, and a stable record API used by Tcl/BASIC/ENGLISH.
- **M3b — ENGLISH query core:** Implement `LIST`, `COUNT`, and `SELECT` on top of the M3a primitives.
- **M3c — Ordering + list lifecycle:** Add `SORT`, active-list lifecycle commands, and resolver/debug ergonomics needed to operate ENGLISH predictably.

**ENGLISH subsystem (minimal core)**

- Implement **`LIST`**, **`SORT`**, **`COUNT`**, **`SELECT`** as an ENGLISH processor module.
- Tcl first-token synonym resolution (**`V`/`X`/`Q`**) reads **`VOC`**; verbs dispatch to compiled-in handlers (see **Milestone 3 delivery status** below — VOC execute-body processors remain non-goals for M3).
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

- `LIST` / `SORT` / `COUNT` / `SELECT` exposed as Tcl built-ins after **`resolveVerbName`**; catalogue **`VOC`** may carry **`V`** entries matching those verbs for Pick-authentic vocabulary.
- Minimal active-list operations: **`LIST-LIST`** and **`CLEAR-LIST`**.
- Resolver introspection via **`RESOLVE-FIELD`** (and hard errors on unknown ENGLISH fields in the executor).
- Minimal **`DEFINE-FIELD`** Tcl helper for type-**`A`** DICT authoring (**`WRITE`** semantics unchanged—single string, full replace). Full Pick **ED**/`FI`/`Q`-style record editing deferred (see Milestone **4** polish / later milestone).

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

### Milestone 3 delivery status (closed)

Slices **M3a/M3b/M3c** are implemented in-tree and summarized in **`docs/filesystem-m3.md`**.

- **ENGLISH subsystem:** standalone module (`LIST`, `SORT`, `COUNT`, `SELECT`) with **`DICT-<file>`** / **`DICT`** precedence, stable **`SORT`**, active-list helpers, **`RESOLVE-FIELD`** and **`DEFINE-FIELD`** tooling — see **`docs/english.md`** and **`docs/tcl-shell.md`**.
- **TCL + VOC:** first-token **`V`** / **`X`** / **`Q`** resolution through **`VOC`** (**`PickVoc::VocResolver::resolveVerbName`** in **`src/userland/tcl/Shell.cpp`**) precedes uppercase dispatch into the **built-in** command map. **`LIST`/`SORT`/`COUNT`/`SELECT`** handlers are compiled-in (Pick-style **catalogue VOC** entries ship for **`SYSPROG`** for visibility). VOC records that invoke arbitrary processor bodies (**dynamic processor registration**) remain deferred as above — this is deliberate for M3.
- **Residual / later milestones:** full Pick **`SSELECT`**, report formatting, correlate execution, VOC-authored ENGLISH invocation without built-ins — see **Non-goals**.

