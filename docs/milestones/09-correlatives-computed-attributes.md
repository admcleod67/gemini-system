← [Project milestones index](../milestones.md)

## Milestone 9 — Correlatives & Computed Attributes

Milestone 9 completes the DICT model begun in Milestones 2 and 3 by introducing computed attributes — Pick‑authentic **F**‑type and **I**‑type dictionary items — and the supporting execution engine required to evaluate them. This milestone enables ENGLISH, BASIC, and TCL to access derived fields, transformations, and attribute‑level expressions consistently with classic Pick behaviour. No VM changes are required; all computation is performed in a DICT‑layer evaluator integrated with the existing record and attribute model.

This milestone significantly expands the expressive power of ENGLISH queries and BASIC file access, enabling real‑world DICT‑driven applications.

### Goals

- Implement Pick‑authentic **F**‑type and **I**‑type DICT items.
- Add a DICT‑level correlative execution engine that evaluates computed attributes at runtime.
- Integrate computed attributes into:
  - ENGLISH field resolution
  - BASIC **`READV`** / angle‑bracket syntax
  - TCL **`RESOLVE-FIELD`** and DICT tooling
- Preserve the existing DICT lookup precedence and record API from Milestones 2–4.
- Avoid VM or filesystem changes; computed attributes are evaluated entirely in userland.
- Maintain deterministic, testable behaviour aligned with classic Pick semantics.

### Scope

#### 1. DICT item types (Pick‑authentic)

Implement the following DICT item types:

- **A‑type** (already implemented in M3) — direct attribute extraction.
- **S‑type** (already implemented in M3) — synonym / indirection.
- **F‑type** — correlative expressions that transform attribute values using Pick’s F‑correlative syntax.

  Examples:

  - `F;2;3` (extract attribute 2, value 3)
  - `F;2;L` (leftmost value)
  - `F;2;R` (rightmost value)
  - `F;2;3;CONV;...` (conversion pipeline)

- **I‑type** — computed attributes expressed as Pick BASIC‑like expressions.

  Examples:

  - `I;A + B`
  - `I;IF A > 10 THEN "HIGH" ELSE "LOW"`
  - `I;OCONV(DATE(), "D2/")`

DICT items are parsed and stored as structured definitions, not raw strings.

#### 2. Correlative execution engine

Introduce a dedicated **CorrelativeEvaluator** module with:

- **Parser** — parses F‑type and I‑type expressions into an internal AST.
- **Execution engine** — evaluates correlatives against a `StructuredRecord` using:
  - attribute extraction
  - multi‑value indexing
  - string / numeric coercion rules
  - conversion tables (ICONV / OCONV subset from M7)
  - conditional logic (for I‑types)
- **Error handling** — stable, Pick‑authentic wording for:
  - missing attributes
  - invalid correlatives
  - type mismatches
  - division by zero
  - invalid conversion codes

The evaluator is pure userland and does not modify VM semantics.

#### 3. ENGLISH integration

Computed attributes become first‑class fields in ENGLISH:

- `LIST CUSTOMERS NAME BALANCE` — where **BALANCE** is an I‑type.
- `SORT ORDERS BY TOTAL` — where **TOTAL** is an F‑type.
- `SELECT SALES WITH PROFIT > 1000` — where **PROFIT** is computed.

ENGLISH field resolution is extended to:

1. Check DICT for A / S / F / I types.
2. Evaluate F / I types via the correlative engine.
3. Use the result in sorting, selection, and output.

Unknown fields continue to produce hard errors.

#### 4. BASIC integration

BASIC gains access to computed attributes through:

- **`READV`** *var* **FROM** *F*, *id*, `"FIELDNAME"`
- **`REC<"FIELDNAME">`** (stretch goal)
- DICT‑aware attribute resolution (optional but recommended)

Computed attributes are evaluated at runtime and returned as strings or numerics according to coercion rules. No new BASIC opcodes are introduced.

#### 5. TCL integration

Extend TCL DICT tooling:

- **`RESOLVE-FIELD`** *\<file\>* *\<field\>* — now resolves and displays F / I‑type definitions.
- **`DEFINE-FIELD`** — gains minimal support for creating F‑type and I‑type items (optional stretch).
- **`LIST-DICT`** — shows item type (A, S, F, I) and validity.

TCL remains the primary authoring surface for DICT items.

#### 6. Conversion table integration

Milestone 7 introduced minimal ICONV / OCONV support. Milestone 9 extends this by:

- Allowing F‑type correlatives to invoke conversion codes.
- Allowing I‑type expressions to call **`ICONV()`** and **`OCONV()`**.
- Documenting the supported conversion codes and rejecting unsupported ones with stable errors.

No new conversion codes are required for this milestone.

#### 7. Documentation

Update:

- [docs/dict.md](../dict.md) — full DICT item type reference.
- [docs/english.md](../english.md) — computed attribute usage.
- [docs/basic-file-io.md](../basic-file-io.md) — DICT‑aware reads.
- [docs/tcl-shell.md](../tcl-shell.md) — DICT authoring and introspection.
- [docs/milestones.md](../milestones.md) — milestone index entry.

Add:

- [docs/correlatives.md](../correlatives.md) — syntax, examples, evaluator behaviour.

### Non‑Goals (Deferred)

- Full conversion table (all ICONV / OCONV codes).
- Complex F‑correlative pipelines beyond the minimal Pick subset.
- Multi‑attribute computed fields (e.g. joins).
- SQL‑like expressions.
- Dynamic processor registration (Milestone 11).
- Performance optimisation of correlative evaluation.
- Multi‑user locking semantics (Milestone 10).

### Rationale

Computed attributes are a defining feature of Pick systems: they allow DICTs to encode business logic, derived fields, and transformations without modifying application code. Milestone 9 completes the DICT model by adding F‑ and I‑type items and a robust correlative evaluator. This enables ENGLISH, BASIC, and TCL to operate on meaningful, derived data rather than raw attributes.

By keeping the evaluator in userland and avoiding VM changes, this milestone preserves Gemini’s architectural cleanliness while dramatically expanding its expressive power.

Milestone 9 positions Gemini for real‑world DICT‑driven applications and prepares the system for later reporting, multi‑user, and multi‑language milestones.

### Delivery plan

Implementation is sequenced into vertical stages. Each stage ships a test‑locked slice before the next starts. The **CorrelativeEvaluator** (parse → AST → evaluate against `StructuredRecord`) is introduced first; ENGLISH, BASIC, and TCL call a single DICT‑layer evaluation entry point so behaviour stays consistent across consumers. Detailed per‑stage plans may live in `~/.cursor/plans/m9_stage_*.plan.md` during implementation; status is summarised here as each stage lands (matching the M4 / M5 / M8 precedent).

- **Stage 1 — DICT model + F‑type foundation**: parse F‑type DICT records into structured definitions (classic attrs 1–3: type `F`, source attribute, selector tail); introduce **CorrelativeEvaluator** with **DictFItemParser**; evaluate minimal F correlatives in isolation (value index, `L`, `R`; conversion tail stored but not executed). Lock stable errors for invalid / missing correlatives. Extend **DictionaryResolver** to recognise F items (A/S precedence unchanged). Unit tests on `StructuredRecord` only — no ENGLISH/BASIC/TCL integration yet. *Status: implemented.*

- **Stage 2 — F‑type in ENGLISH + `RESOLVE-FIELD`**: wire evaluator into ENGLISH field resolution for projection and `SORT BY` keys; `LIST` / `SORT` / `SELECT` / `COUNT` treat F‑type tokens as first‑class fields. Extend **`RESOLVE-FIELD`** to display F‑type definitions and evaluation hints. Unknown fields remain hard errors. Queries without F/I items remain byte‑identical to pre‑M9 output. *Status: pending.*

- **Stage 3 — F‑type conversion (`CONV`)**: F correlatives invoke the Milestone 7 ICONV/OCONV subset (`F;…;CONV;…`); document supported codes in [docs/correlatives.md](../correlatives.md) (initial stub acceptable). Reject unsupported conversion codes with stable, test‑locked messages. No new conversion codes beyond M7. *Status: pending.*

- **Stage 4 — I‑type core**: I‑type parser and AST; expression evaluator (arithmetic, field references, coercion). Integrate I‑types into ENGLISH projection and `SORT BY`. Lock errors for type mismatch and division by zero. `IF` / function calls deferred to Stage 5. *Status: pending.*

- **Stage 5 — I‑type advanced + BASIC `READV`**: `IF … THEN … ELSE …` and **`ICONV()`** / **`OCONV()`** in I‑expressions; reuse M7 conversion tables. BASIC **`READV`** *var* **FROM** *file*, *id*, `"FIELDNAME"` evaluates F/I fields via the same DICT evaluator (no new VM opcodes). **`REC<"FIELDNAME">`** remains an optional stretch within this stage. *Status: pending.*

- **Stage 6 — TCL authoring, `WITH` (if ready), docs — closes M9**: **`LIST-DICT`** (type A/S/F/I and validity); **`RESOLVE-FIELD`** polish for I‑types; optional minimal **`DEFINE-FIELD`** for F/I (stretch). ENGLISH **`WITH`** predicates on computed fields only when selection evaluation is implemented — otherwise document as follow‑up and keep the existing WITH absorption stub. Add [docs/correlatives.md](../correlatives.md) and update [docs/english.md](../english.md), [docs/tcl-shell.md](../tcl-shell.md), planned [docs/dict.md](../dict.md) / [docs/basic-file-io.md](../basic-file-io.md), and the milestone index. **Closes Milestone 9.**

Only Stage 6's exit criteria should claim "Closes Milestone 9".
