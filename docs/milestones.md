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

## Milestone 4 — BASIC language fidelity & filesystem maturation

Milestone 4 deepens the **BASIC** subsystem into a more Pick-authentic language, building on the foundation laid by
Milestones 1–3. It expands BASIC’s file semantics, numeric/string behaviour, and multi-value awareness, and introduces
filesystem enhancements required to support those features.

Milestone 4 is intentionally **BASIC-focused**: Tcl/VOC ergonomics, ENGLISH formatting, and PROC polish are deferred to
Milestone 5.

### Goals

- Deliver a substantially more complete Pick BASIC, including attribute-level and multi-value file operations.
- Mature the filesystem beyond M3 to support cursor state, attribute-level writes, and multi-value mutation.
- Introduce floating-point numeric semantics and improved coercion rules.
- Preserve architectural cleanliness: BASIC drives filesystem evolution, not the other way around.
- Maintain backward compatibility with existing BASIC programs and VM behaviour.

### Scope

### Stage 1 status

- Implemented: `READNEXT`, `READV`, `WRITEV` statement/opcode/runtime paths.
- Implemented: `READNEXT` cursor semantics (reset on `OPEN`, invalidated on `WRITE`/`WRITEV`, lexicographic record order).
- Implemented: no BASIC file-creation opcode/statement; missing-file behavior follows `ELSE`/raise contract.
- Implemented: filesystem helpers for attribute/subvalue mutation while preserving surrounding record data.

### Stage 2 status

- Implemented: floating-point literal parsing in BASIC expressions (decimal + scientific forms).
- Implemented: VM value/opcode support for float constants and mixed int/float arithmetic.
- Implemented: numeric-prefix string coercion for arithmetic (`\"\" -> 0`, `"12ABC" -> 12` style).
- Implemented: clean float rendering through `PRINT` value output path.

### Stage 3 status

- Implemented: BASIC expression-level angle-bracket reads (`REC<attr>` / `REC<attr,valueIndex>`) with parser disambiguation from `<` comparison.
- Implemented: dedicated VM extraction opcode path backed by `StructuredRecord` and `RecordAttribute` (no duplicate multi-value parsing rules).
- Implemented: shared multi-value join helper reused by runtime write paths to keep BASIC/ENGLISH/DICT splitting consistent.
- Implemented: documentation clarifying that `DIM` arrays and record multi-values are distinct models (no implicit aliasing).

### Stage 4 status

#### 1. BASIC file semantics (Pick-authentic core)

- **`READNEXT`**
  - Add per-file cursor state.
  - Cursor resets on `OPEN`.
  - Cursor invalidated on `WRITE`.
  - Stable iteration order (lexicographic record ID).
- **`READV` / `WRITEV`**
  - Attribute-level read/write using the M3 structured attribute model.
  - Support multi-value indexing (e.g. `READV var FROM F, id, attr, valueIndex`).
  - Preserve all other attributes and values on write.
- **Improved `ELSE` semantics**
  - Align `OPEN`, `READ`, `WRITE`, `READNEXT`, `READV`, `WRITEV` with Pick BASIC’s `ELSE` behaviour.
  - File-missing policy: BASIC does not implicitly create missing logical files; when a required logical file is missing, the operation routes to the `ELSE` clause or raises (depending on the existing BASIC ELSE design).
  - Inline statement or line target; single-statement rule preserved.
  - File creation remains available only via Tcl (for example, `CREATE-FILE`), not via BASIC statements/opcodes.
- **Optional Pick-style angle-bracket syntax**
  - Implemented: `REC<attr>` and `REC<attr,value>` as syntactic sugar for multi-value reads (`READV`).
  - Compiler disambiguation prevents collisions with `<` comparisons.

#### 2. Numeric & string semantics

- **Floating-point literals**
  - Add decimal literals (`3.14`, `0.5`, `1E3`).
  - Extend expression parser and VM arithmetic.
- **Floating-point arithmetic**
  - Add VM opcodes or extend existing arithmetic to support double precision.
  - Preserve integer coercion rules for `%` variables.
- **Improved coercion rules**
  - String → numeric coercion matches Pick semantics (`\"\"` → `0`, non-numeric prefix allowed).
  - Mixed-type comparisons raise runtime errors (existing rule preserved).
- **Minimal `PRINT` formatting**
  - Support `PRINT USING` (optional stretch).
  - At minimum, ensure floats print cleanly.

#### 3. Multi-value awareness

- Structured multi-value access
  - BASIC expressions can read multi-values via `READV` or angle-bracket syntax.
  - Multi-value writes preserve surrounding values.
- Array interoperability
  - Clarify semantics between `DIM` arrays and multi-value attributes (remain distinct).
- Consistent multi-value splitting
  - Use the same splitter as ENGLISH and DICT (M3 contract).

#### 4. Filesystem enhancements (driven by BASIC)

- **File cursor state**
  - Implemented: cursor state is stored on `PickFS::FileSystem::FileHandle`.
  - `OPEN` resets cursor state; `READNEXT` advances it; `WRITE`/`WRITEV` invalidate it by resetting the cursor.
- **Attribute-level write API**
  - Implemented: filesystem can update a single attribute while preserving the surrounding record data.
- **Multi-value write API**
  - Implemented: filesystem can update a specific subvalue (by value index) while preserving sibling multi-values.
- **Improved error taxonomy**
  - Implemented: classified runtime IO failures (missing file/record/cursor exhaustion, plus attribute/subvalue presence).
  - Presence-sensitive `READV`/`READV_TRY`:
    - `READV` throws `ATTRIBUTE.NOT.FOUND` / `SUBVALUE.NOT.FOUND`
    - `READV ... ELSE` treats missing attribute/subvalue as failure (`flag=0`) and routes to `ELSE`
- **Optional DICT-aware helpers**
  - Allow BASIC to resolve attribute names via DICT (stretch goal).

#### 5. BASIC language polish

- Prompted `INPUT`
  - Implemented: `INPUT \"Prompt\", var` in addition to `INPUT var`.
  - Implemented: compiler lowers prompted form to `PRINT` (no newline) + `INPUT_STR`.
- `CHAIN` (minimal)
  - Implemented: `CHAIN <programExpr>` transfers control to another BASIC program.
  - Implemented: no parameter passing in this milestone.
- Improved diagnostics
  - Implemented: precise compile-time diagnostics for malformed file statements and prompted-INPUT/CHAIN forms.
  - Implemented: classified runtime wording for file/control-flow failures remains stable under regression tests.
- R83-authentic behaviour refinements
  - Implemented: tightened `FOR`/`NEXT` mismatch messaging and `GOSUB`/`RETURN` runtime wording.
  - Implemented: `STOP`/`END` remain equivalent halt semantics, including implicit-end behavior.

### Non-goals (deferred)

- Tcl tokenisation and quoting improvements
- VOC authoring commands (CREATE-VOC, DELETE-VOC, etc.)
- ENGLISH `WITH`-clause evaluation
- ENGLISH formatting (`HEADING`, `BREAK-ON`, `TOTAL`, pagination)
- PROC language enhancements
- Correlative execution (F/I-type DICT items)
- Record locking (`READU`, `WRITEU`, `RELEASE`)
- Multi-user concurrency
- Report writer subsystem
- Full BASIC `MAT` operations (future milestone)

## Milestone 5 — R83-flavoured fidelity pass

- Improved TCL ergonomics
- Optional quoting and tokenisation refinements
- VOC authoring from within the system
- BASIC and PROC polish
- Decision on long-term VOC format (canonical, structured, or hybrid)
