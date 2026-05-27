← [Project milestones index](../milestones.md)

## Milestone 8 — ENGLISH Formatting Layer

Milestone 8 extends the ENGLISH processor with Pick‑authentic report‑formatting capabilities: headings, breaks, totals, pagination, and controlled output layout. This milestone introduces the first true reporting layer on top of the existing LIST/SORT/SELECT engine delivered in Milestone 3. No VM or filesystem changes are required; all work is confined to the ENGLISH planner/executor and a new formatting subsystem.

The goal is to make ENGLISH suitable for operator‑facing reports, not just raw data extraction.

### Goals

- Add a Pick‑authentic formatting layer to ENGLISH, including headings, breaks, totals, and pagination.
- Preserve the existing ENGLISH parser and executor; formatting is layered on top of the M3 engine.
- Maintain strict separation between:
  - query semantics (selection, sorting, field resolution)
  - formatting semantics (layout, grouping, totals)
- Avoid VM, BASIC, or filesystem changes; formatting is a pure ENGLISH concern.
- Produce stable, predictable output suitable for terminal display and later printer/report subsystems.

### Scope

#### 1. Formatting directives (Pick‑authentic core)

Implement the following ENGLISH clauses:

- **`HEADING`** — Static or dynamic heading text printed at the top of each page. Support Pick‑style substitution tokens (e.g., date/time, page number).
- **`FOOTING`** (optional stretch) — Footer text printed at the bottom of each page.
- **`BREAK-ON <field>`** — Group records by the specified field. Emit a break line when the field value changes.
- **`TOTAL <field>`** — Accumulate numeric totals for the specified field. Emit totals at break boundaries and/or at end of report.
- **`ID-SUPP`** — Suppress record IDs in output.
- **`PAGE` / pagination** — Page length defaults to a fixed terminal height (configurable). Emit headings on each new page.

Formatting directives are parsed as part of the ENGLISH command but executed by the new formatting layer.

#### 2. Formatting subsystem architecture

Introduce a dedicated ENGLISH Formatter module with the following components:

- **Layout Planner** — Consumes the executor’s row stream and formatting directives. Determines break boundaries, totals, and page boundaries.
- **Accumulator Engine** — Maintains running totals for fields declared in **`TOTAL`**.
- **Page Manager** — Tracks line counts, triggers page breaks, and re‑emits headings.
- **Renderer** — Emits final formatted lines to the terminal. No printer semantics yet (deferred to later milestones).

This subsystem is strictly layered on top of the existing ENGLISH executor.

#### 3. Parser extensions

Extend the ENGLISH parser to recognise:

- **`HEADING "text"`**
- **`BREAK-ON <field>`**
- **`TOTAL <field>`**
- **`ID-SUPP`**
- **`PAGE`**
- **`FOOTING "text"`** (stretch)

Parser changes are additive and do not modify existing M3 grammar.

#### 4. Output behaviour

- Deterministic, stable formatting across runs.
- Consistent spacing, indentation, and alignment.
- Break lines include the break field and its value.
- Totals appear at:
  - each break boundary
  - end of report
- Page breaks re‑emit headings.
- No printer control codes; plain text only.

#### 5. TCL integration

- Formatting directives are available through the existing ENGLISH entry points (**`LIST`**, **`SORT`**, **`SELECT`**).
- No new TCL commands required.
- **`HELP LIST`**, **`HELP SORT`**, and **`HELP SELECT`** updated to include formatting clauses (file‑backed HELP from M6).

#### 6. Documentation

Update:

- [docs/english.md](../english.md) — formatting syntax, examples, and behaviour.
- [docs/tcl-shell.md](../tcl-shell.md) — ENGLISH usage examples with formatting.
- [docs/milestones.md](../milestones.md) — milestone index entry.

Add:

- `docs/english-formatting.md` — detailed formatting subsystem description.

### Non‑Goals (Deferred)

- Printer/report writer subsystem (printer control codes, spooler integration).
- Advanced formatting:
  - **`DET-SUPP`**
  - **`COL-HDR-SUPP`**
  - multi‑column layouts
  - computed break expressions
- SQL‑like extensions.
- Dynamic processor registration (Milestone 11+).
- Multi‑user or locking semantics (Milestone 10).
- Correlative execution (Milestone 9).

### Rationale

ENGLISH today (post‑M3) provides a solid query engine but lacks the formatting features required for real‑world reporting. Milestone 8 fills this gap by adding a Pick‑authentic formatting layer without altering the VM, filesystem, or BASIC runtime. This milestone is intentionally narrow and vertical: it enhances ENGLISH output while keeping the underlying data and execution model stable.

With Milestone 8 complete, Gemini gains its first operator‑grade reporting capability, enabling meaningful business‑style output and preparing the system for later printer/report writer milestones.

### Delivery plan

Implementation is sequenced into vertical stages, each one shippable end‑to‑end with tests locked in before the next stage starts. Detailed per‑stage plans live in `~/.cursor/plans/m8_stage_*.plan.md` during implementation; the status of each stage is summarised back into this section as it lands (matching the M4 / M5 precedent).

- **Stage 1 — Formatter foundation + `HEADING`**: introduce the formatter module skeleton (Layout Planner, Page Manager, Renderer), wire a no‑op row pipeline through it, and ship `HEADING "text"` end‑to‑end including Pick‑style substitution tokens (date / time / page number). Establishes the test idiom (stable substrings, golden output) for later stages. *Status: implemented.* Ships `HEADING` on `LIST` / `SORT` / `SELECT` / `COUNT` with `@DATE` / `@TIME` / `@PAGE` substitution and `@@` escape; `@<digits>` and unknown `@<identifier>` parse cleanly but render empty (reserved for Stages 3‑4). Page number is fixed at 1; pagination is deferred to Stage 2.
- **Stage 2 — Pagination**: Page Manager goes from skeleton to active — line‑count tracking, configurable terminal height, page breaks that re‑emit `HEADING`. *Status: implemented.* Ships dynamic `@PAGE`, one blank-line separator between pages, and operator-configurable page length via `SET PAGE-LENGTH n` (positive integer, default 24, session-scoped, resets on `LOGOFF`). Pagination is gated on a `HEADING` clause being present; queries without `HEADING` remain byte-identical to Stage 1 output. Per-query modifiers and printer-class defaults stay deferred.
- **Stage 3 — `BREAK-ON <field>`**: Layout Planner gains break detection. Parser learns `BREAK-ON`. Break‑line shape and wording are locked. *Status: implemented.* Ships `BREAK-ON <field>` with DICT-resolved per-row break keys, hyphen-only break lines (`kReportWidth = 79`), and break detection on value change in output order; integrates with Stage 2 pagination.
- **Stage 4 — `TOTAL <field>`**: Accumulator Engine goes from skeleton to active; totals emit at break boundaries (from Stage 3) and at end of report. *Status: implemented.* Ships `TOTAL <field>` with DICT-resolved numeric addends, subtotal-before-break / grand-total-after-rows ordering, locked line shape `TOTAL <field>: <value>`, and `@<digits>` heading substitution from the last emitted row.
- **Stage 5 — `ID-SUPP` + HELP integration**: small directive plus operator‑facing HELP updates for `LIST` / `SORT` / `SELECT` via the M6 file‑backed HELP. *Status: pending.*
- **Stage 6 — `FOOTING` (stretch) + docs polish**: the spec's stretch directive, plus `docs/english.md`, `docs/tcl-shell.md`, and the new `docs/english-formatting.md`. *Status: pending.*

Only Stage 6's exit criteria should claim "Closes Milestone 8".
