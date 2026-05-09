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

## TCL shell

- **VOC first-token verb resolution** — `Implemented`
  - `V`/`X` with `Q` chaining before built-in dispatch.
- **Tokenizer/quoting** — `Implemented`
  - Tcl command tokenization supports quoted strings, escaped characters, and preserved quoted empty tokens.
- **Session `@` names** — `Implemented`
  - `@USERNO`, `@ACCOUNT`, `@LOGNAME`, `@DEFDATA` resolved from session state.
- **`$` substitution scope** — `Intentional deviation`
  - `$` expansion is intentionally scoped to `ECHO` in current milestone delivery, not applied shell-wide.

## VOC behavior

- **Resolver types in active subset** — `Implemented`
  - `F`, `Q`, `V`, `D`, `A`, `X` handled for current command/program/script flows.
- **Full dictionary semantics parity** — `Partial`
  - Some classic Pick dictionary depth and execute-body behaviors are deferred by milestone.

## ENGLISH

- **Core query verbs** — `Implemented`
  - `LIST`, `SORT`, `COUNT`, `SELECT` core pipeline is in place.
- **Report-writer/formatting surface** — `Deferred`
  - `HEADING`, `BREAK-ON`, `TOTAL`, pagination, and related features remain deferred.

## BASIC

- **Core execution/compiler model** — `Implemented`
  - Runtime/compiler pipeline and key language behavior are in-tree.
- **Full legacy surface** — `Partial`
  - Some advanced legacy areas (for example full MAT family and other deferred features) remain roadmap items.

## Filesystem/session model

- **Pick-style logical files and record APIs** — `Implemented`
  - Logical file/record operations and session-scoped active list behavior are available.
- **Multi-user locking/concurrency parity** — `Deferred`
  - `READU`/`WRITEU`/`RELEASE` and full concurrency model remain deferred.

## Interpretation notes

- Compatibility in Gemini is milestone-driven: implemented behavior is documented as normative for current delivery, while deeper legacy parity is tracked explicitly in roadmap docs.
- For detailed command semantics, use subsystem docs (`proc.md`, `tcl-shell.md`, `english.md`, `basic-language.md`) and milestones as the source of truth.
