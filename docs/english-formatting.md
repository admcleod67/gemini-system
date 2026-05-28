# ENGLISH formatting layer

Companion to [docs/english.md](english.md). Tracks the Milestone 8 formatting clauses as they land.

For roadmap context see [Project milestones](milestones.md) and [Milestone 8 — ENGLISH Formatting Layer](milestones/08-english-formatting-layer.md).

## Architecture

The ENGLISH service runs a query in two halves:

1. **Executor** (`src/core/english/Executor.cpp`) scans, projects, and sorts records, producing a stream of `Row` values plus any verb-specific summary lines (e.g. `5 records selected` for `SELECT`, the count for `COUNT`).
2. **Formatter** (`src/core/english/Formatter.cpp`) lays out the row stream into the final `Result.lines` using three internal components:
   - **LayoutPlanner** — turns the input into a small event stream (`[heading?, total?, break?, row*, total?, trailing*]` when formatting clauses are set).
   - **Accumulator Engine** — maintains running totals for `TOTAL` fields (Stage 4).
   - **PageManager** — tracks the emitted-line count and the live page number; signals page boundaries (Stage 2).
   - **Renderer** — emits the final line vector, consulting `PageManager` to inject blank-line separators and re-emit the heading at each page boundary.

Queries without any formatting clauses produce **byte-identical** output to the pre‑M8 executor.

## `HEADING "<text>"`

Syntax: `HEADING` followed by a single quoted string. The clause may appear anywhere after the verb / filename and can be combined with `BY` / `WITH`. It is allowed on `LIST`, `SORT`, `SELECT`, and `COUNT`; only one `HEADING` clause is permitted per query.

When present, the rendered heading is emitted as the first output line. The remaining lines are unchanged: data rows for `LIST` / `SORT`, the count for `COUNT`, or `N records selected` for `SELECT`.

Examples:

```text
LIST CUSTOMERS NAME HEADING "Customer Report"
SORT CUSTOMERS BY NAME HEADING "Alphabetical"
COUNT CUSTOMERS HEADING "Summary"
SELECT CUSTOMERS HEADING "Active scope"
```

Diagnostics (stable, test-locked):

- `HEADING requires a quoted string` — emitted when the next token is missing or is a reserved structural keyword (`BY`, `BY-DSND`, `WITH`, `HEADING`).
- `HEADING can only appear once` — emitted when two `HEADING` clauses appear in the same query.

### Substitution tokens

Heading templates use Pick‑classic `@<token>` substitution, evaluated by the formatter at render time. The grammar is deliberately small and unambiguous; later stages extend the token table without changing the parser.

| Token | Stage 1 output |
| --- | --- |
| _plain text_ | passes through verbatim |
| `@@` | literal `@` (the rest of the template still scans for tokens) |
| `@DATE` | current date in `dd MMM yyyy` form — byte-equivalent to BASIC `OCONV(value, "D")` |
| `@TIME` | current time in `HH:MM:SS` form |
| `@PAGE` | current page number — incremented at each page boundary (see Pagination below) |
| `@<digits>` | first sub-value of attribute *n* from the **last emitted data row**; empty before any row and on the initial heading |
| `@<identifier>` | empty string for unknown identifiers (reserved for future tokens) |

Token names are case‑insensitive (`@date`, `@DATE`, `@Date` all match). A bare `@` not followed by `@`, a digit, or a letter is rendered as a literal `@`.

Example: `LIST CUSTOMERS NAME HEADING "Customers — @DATE — page @PAGE"` produces:

```text
Customers — 26 May 2026 — page 1
R1 ALICE
R2 BOB
…
```

## Pagination

When a query carries a `HEADING`, the formatter splits long output into pages of `PAGE-LENGTH` lines. The default page length is **24** (Pick terminal-class). Operator-configurable via the Tcl shell:

```text
SET PAGE-LENGTH 30      ; change for the current shell session
GET PAGE-LENGTH         ; print the current value
```

Validation: `PAGE-LENGTH` must be a positive integer; bad inputs emit `SET PAGE-LENGTH requires a positive integer` and leave the previous value unchanged. The setting is **not** a user variable — it does not appear in `LIST-VARS` and resets to 24 on logoff.

Rules:

- **One blank line separator** between pages, followed immediately by the re-rendered `HEADING`. No form feed, no banner.
- `@PAGE` resolves to the live page number every time it is rendered, so headings on page 2 say `"... 2"` etc.
- Heading lines, row lines, and verb-trailing lines (`5 records selected`, the count value) all count toward the page line budget. The blank separator does **not** count toward the new page.
- Pagination only fires for queries that have a `HEADING`. Without a heading the formatter emits the row stream unchanged — byte-identical to the pre‑M8 output.
- `PAGE-LENGTH` ≤ 0 (only reachable internally via the `EnglishRunOptions::pageLength` plumbing) disables pagination entirely.

Example: 12 records, `SET PAGE-LENGTH 5`, `LIST CUSTOMERS NAME HEADING "Page @PAGE"` produces:

```text
Page 1
R01 ...
R02 ...
R03 ...
R04 ...

Page 2
R05 ...
R06 ...
R07 ...
R08 ...

Page 3
R09 ...
R10 ...
R11 ...
R12 ...
```

## `BREAK-ON <field>`

Syntax: `BREAK-ON` followed by a single field token (DICT-resolved the same way as projection fields). The clause may appear anywhere after the verb / filename and can be combined with `HEADING`, projection fields, and `BY` on `SORT`. Only one `BREAK-ON` clause is permitted per query.

When present, the formatter compares each row’s resolved break-field value (first sub-value of the attribute) against the previous row in **output order** (after `SORT`). When the value changes, a **break line** is emitted immediately before the row that starts the new group. There is no break line before the first row.

Examples:

```text
LIST CUSTOMERS NAME CITY BREAK-ON CITY
SORT CUSTOMERS NAME CITY BREAK-ON CITY BY CITY
```

Diagnostics (stable, test-locked):

- `BREAK-ON requires a field` — emitted when the next token is missing or is a reserved structural keyword (`BY`, `BY-DSND`, `WITH`, `HEADING`, `BREAK-ON`, `TOTAL`).
- `BREAK-ON can only appear once` — emitted when two `BREAK-ON` clauses appear in the same query.

Unknown break fields fail at execute time with the same `Unknown ENGLISH field "…"` message as projection fields.

### Break-line format

A single line of **79 literal hyphen (`-`) characters** — no spaces, corners, or field text on the break line itself. This is the minimal Pick/ACCESS style; richer printer conventions are deferred.

```text
-------------------------------------------------------------------------------
R3 CAROL PARIS
```

For readable groups, use `SORT … BY` the same field as `BREAK-ON` so records with the same break value appear consecutively.

Break lines count toward `PAGE-LENGTH` when `HEADING` pagination is active (same as data rows).

Queries without `BREAK-ON` are byte-identical to pre–Stage 3 output.

## `TOTAL <field>`

Syntax: `TOTAL` followed by a single field token (DICT-resolved the same way as projection fields). The clause may appear anywhere after the verb / filename and can be combined with `HEADING`, `BREAK-ON`, projection fields, and `BY` on `SORT`. Only one `TOTAL` clause is permitted per query.

The executor reads each row’s total-field cell (first sub-value) and parses it numerically (same rules as sort numeric coercion: strip commas/`$`, prefix `stod`). Empty or non-numeric cells contribute **0**.

When present, the formatter emits:

- **Subtotal** at each `BREAK-ON` boundary — sum of the group that just ended, **before** the hyphen break line
- **Grand total** once after all data rows, **before** verb trailing lines (`5 records selected`, the `COUNT` value)

Examples:

```text
LIST CUSTOMERS NAME AMOUNT TOTAL AMOUNT
SORT CUSTOMERS NAME CITY AMOUNT BREAK-ON CITY TOTAL AMOUNT BY CITY
```

Diagnostics (stable, test-locked):

- `TOTAL requires a field` — emitted when the next token is missing or is a reserved structural keyword (`BY`, `BY-DSND`, `WITH`, `HEADING`, `BREAK-ON`, `TOTAL`).
- `TOTAL can only appear once` — emitted when two `TOTAL` clauses appear in the same query.

Unknown total fields fail at execute time with the same `Unknown ENGLISH field "…"` message as projection fields.

### Total-line format

A single plain-text line:

```text
TOTAL AMOUNT: 1500
```

Shape: literal `TOTAL`, one space, field token from the clause, colon, one space, accumulated value. Integer sums render without a fractional part; non-integer sums use default `to_string` precision. No padding or column alignment.

With `BREAK-ON` and `TOTAL` together, a typical group change looks like:

```text
R2 BOB LONDON 50
TOTAL AMOUNT: 150
-------------------------------------------------------------------------------
R3 CAROL PARIS 200
…
TOTAL AMOUNT: 350
```

The final line is the **grand total** for the entire report. Total lines count toward `PAGE-LENGTH` when `HEADING` pagination is active.

Queries without `TOTAL` are byte-identical to pre–Stage 4 output.

## `ID-SUPP`

Syntax: `ID-SUPP` alone (no argument). The clause may appear anywhere after the verb / filename and can be combined with other formatting clauses. Only one `ID-SUPP` per query.

When present, formatted **data rows** omit the record id prefix. Projection fields are emitted space-separated only.

| Situation | Default output | With `ID-SUPP` |
| --- | --- | --- |
| Row with projection fields | `R1 ALICE LONDON` | `ALICE LONDON` |
| Row with no projection fields | `R1` | empty line |

`HEADING`, break lines, `TOTAL` lines, and verb trailing lines (`5 records selected`, the `COUNT` value) are unchanged. `SELECT` still returns full record ids in the active list (`Result.selectedIds`); only printed data rows are affected.

Diagnostics (stable, test-locked):

- `ID-SUPP can only appear once` — emitted when two `ID-SUPP` clauses appear in the same query.

Examples:

```text
LIST CUSTOMERS NAME CITY ID-SUPP
SORT CUSTOMERS NAME ID-SUPP BY NAME
```

Queries without `ID-SUPP` are byte-identical to pre–Stage 5 output.

## Non‑goals (deferred)

Out of current Stage scope (see the [milestone document](milestones/08-english-formatting-layer.md) for the staging):

- `FOOTING` and HELP/tcl-shell polish (Stage 6).
- Per-query page-length modifiers (e.g. classic Pick `(P30)`) and printer-class defaults (60-line pages) — deferred beyond Milestone 8.
