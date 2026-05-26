# ENGLISH formatting layer

Companion to [docs/english.md](english.md). Tracks the Milestone 8 formatting clauses as they land.

For roadmap context see [Project milestones](milestones.md) and [Milestone 8 — ENGLISH Formatting Layer](milestones/08-english-formatting-layer.md).

## Architecture

The ENGLISH service runs a query in two halves:

1. **Executor** (`src/core/english/Executor.cpp`) scans, projects, and sorts records, producing a stream of `Row` values plus any verb-specific summary lines (e.g. `5 records selected` for `SELECT`, the count for `COUNT`).
2. **Formatter** (`src/core/english/Formatter.cpp`) lays out the row stream into the final `Result.lines` using three internal components:
   - **LayoutPlanner** — turns the input into a small event stream (`[heading?, row*, trailing*]`).
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
| `@<digits>` | empty string (reserved for attribute substitution, lands with `BREAK-ON` / `TOTAL` in Stages 3‑4) |
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

## Non‑goals (deferred)

Out of current Stage scope (see the [milestone document](milestones/08-english-formatting-layer.md) for the staging):

- `BREAK-ON`, `TOTAL`, attribute substitution `@n` (Stages 3‑4).
- `ID-SUPP`, `FOOTING`, HELP integration (Stages 5‑6).
- Per-query page-length modifiers (e.g. classic Pick `(P30)`) and printer-class defaults (60-line pages) — deferred beyond Milestone 8.
