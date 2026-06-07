# Gemini compatibility (R83/Pick)

This page summarizes where Gemini currently aligns with classic R83/Pick behavior, where support is partial, and where behavior is intentionally different.

Status labels:

- `Implemented`: available now with intended semantics.
- `Partial`: core behavior exists, but not full legacy surface.
- `Deferred`: planned but not in current milestone delivery.
- `Intentional deviation`: deliberate Gemini behavior difference.

## PROC

- **Keyword model** — `Implemented`
  - Long-form names are canonical; one-letter R83 forms are accepted as aliases.
- **Substitution contract** — `Implemented`
  - First token is structural command token and is never substituted.
  - Operand-only exact-match substitution; no quoting/escaping/partial replacement.
- **Control flow** — `Implemented`
  - `IF/THEN/ELSE`, `GO`, `RETURN` (flat script exit), labels, and `LOOP/REPEAT/EXIT/EXITIF`.
- **SELECT/READNEXT** — `Partial`
  - Native PROC statements are present; Stage 1 scope is simple `SELECT <file>` + active-list iteration.
  - Advanced selection expressions are deferred.
- **READU/WRITEU/RELEASE** — `Implemented`
  - Native PROC lock statements with `PROCERR = ?LOCKED?` on conflict; `RELEASE` drops session locks.

## TCL shell

- **VOC first-token verb resolution** — `Implemented`
  - `V`/`X` with `Q` chaining before built-in dispatch.
- **HELP surface** — `Implemented`
  - `HELP` supports no-arg listing, command lookup, and topic forms (`PROC`, `TCL`, `VOC`).
  - Resolution precedence is topic-first, then command help; unknown lookups emit stable `No help available`.
- **SYSTEM/ABOUT introspection** — `Intentional deviation`
  - `SYSTEM`/`ABOUT` are Gemini extensions for structured environment introspection.
  - `VERSION` remains the original short identity output.
- **Tokenizer/quoting** — `Implemented`
  - Tcl command tokenization supports quoted strings, escaped characters, and preserved quoted empty tokens.
- **Session `@` names** — `Implemented`
  - `@USERNO`, `@ACCOUNT`, `@LOGNAME`, `@DEFDATA` resolved from session state.
- **`$` substitution scope** — `Intentional deviation`
  - `$` expansion is intentionally scoped to `ECHO` in current milestone delivery, not applied shell-wide.

## Bootstrap/login diagnostics

- **Catalogue/account diagnostics** — `Implemented`
  - Login/boot diagnostics distinguish missing vs malformed `ACCOUNTS.json`.
  - Boot/login now surface account root, `VOC`, and `MD` attachment issues with explicit diagnostics.

## VOC behavior

- **Resolver types in active subset** — `Implemented`
  - `F`, `Q`, `V`, `D`, `A`, `X` handled for current command/program/script flows.
- **VOC authoring/introspection commands** — `Implemented`
  - `CREATE-VOC`, `DELETE-VOC`, and `LIST-VOC` are available in Tcl.
  - `CREATE-VOC` enforces `A/D/F/Q/V/X` and writes canonical attribute-per-line records.
  - `LIST-VOC` is deterministic and emits `<item-id> <type>` with `INVALID` for malformed entries.
- **Full dictionary semantics parity** — `Partial`
  - Some classic Pick dictionary depth and execute-body behaviors are deferred by milestone.

## ENGLISH

- **Core query verbs** — `Implemented`
  - `LIST`, `SORT`, `COUNT`, `SELECT` core pipeline is in place.
- **Report-writer/formatting surface** — `Implemented`
  - `HEADING`, `FOOTING`, `BREAK-ON`, `TOTAL`, `ID-SUPP`, and `SET PAGE-LENGTH` pagination are available on `LIST` / `SORT` / `SELECT` / `COUNT`.

## BASIC

- **Core execution/compiler model** — `Implemented`
  - Runtime/compiler pipeline and key language behavior are in-tree.
- **Full legacy surface** — `Partial`
  - Some advanced legacy areas (for example full MAT family and other deferred features) remain roadmap items.

## Filesystem/session model

- **Pick-style logical files and record APIs** — `Implemented`
  - Logical file/record operations and session-scoped active list behavior are available.
- **Multi-user locking/concurrency parity** — `Partial`
  - Session-scoped `READU`/`WRITEU`/`RELEASE` across Tcl, BASIC, and PROC; no distributed locking or timeouts.
  - See [Concurrency and record locking](concurrency.md).

## Interpretation notes

- Compatibility in Gemini is milestone-driven: implemented behavior is documented as normative for current delivery, while deeper legacy parity is tracked explicitly in roadmap docs.
- For detailed command semantics, use subsystem docs (`proc.md`, `tcl-shell.md`, `english.md`, `basic-language.md`) and milestones as the source of truth.
