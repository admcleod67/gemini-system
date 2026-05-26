# ENGLISH formatting layer

Companion to [docs/english.md](english.md). Tracks the Milestone 8 formatting clauses as they land.

For roadmap context see [Project milestones](milestones.md) and [Milestone 8 — ENGLISH Formatting Layer](milestones/08-english-formatting-layer.md).

## Architecture (Stage 1)

The ENGLISH service runs a query in two halves:

1. **Executor** (`src/core/english/Executor.cpp`) scans, projects, and sorts records, producing a stream of `Row` values plus any verb-specific summary lines (e.g. `5 records selected` for `SELECT`, the count for `COUNT`).
2. **Formatter** (`src/core/english/Formatter.cpp`) lays out the row stream into the final `Result.lines`. Stage 1 introduces three internal skeletons that grow with later stages:
   - **LayoutPlanner** — turns the input into a small event stream (`[heading?, row*, trailing*]`).
   - **PageManager** — owns the current page number (fixed at 1 in Stage 1; pagination lands in Stage 2).
   - **Renderer** — emits the line vector that becomes `Result.lines`.

Queries without any formatting clauses produce **byte-identical** output to the pre‑Stage 1 executor.

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
| `@PAGE` | current page number (fixed at `1` in Stage 1; live once pagination ships in Stage 2) |
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

## Non‑goals (deferred)

Out of Stage 1 scope (see the [milestone document](milestones/08-english-formatting-layer.md) for the staging):

- Pagination, real `@PAGE` advancement, line‑count tracking (Stage 2).
- `BREAK-ON`, `TOTAL`, attribute substitution `@n` (Stages 3‑4).
- `ID-SUPP`, `FOOTING`, HELP integration (Stages 5‑6).
