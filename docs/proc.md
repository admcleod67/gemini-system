# PROC language (host interpreter)

`PROC` scripts are plain text and run entirely in the host Tcl layer (no VM bytecode and no PROC compiler in this milestone).

## Source layout

- Interpreter implementation: `src/userland/proc/`
- Tcl entry command and script lookup: `src/userland/tcl/`
- Tests: `tests/proc/`

## Execution model

Execution is two-pass:

1. First pass records labels (`NAME:`) and validates duplicates.
2. Second pass executes statements line-by-line with label jumps and loop/control frames.

Scripts run through Tcl as:

```text
PROC <programName> [args...]
```

The script is resolved through VOC/filesystem (`F`/`Q`/`D`/`A` with fallback to logical file `PROC`), then interpreted.

## Variables and substitution

Variable handling is intentionally minimal and token-driven:

- Variables are strings only.
- Positional args are exposed as `P1`, `P2`, ... .
- Tokenization is whitespace-based (no quoting or escaping rules).
- Substitution is exact-token only (no partial replacement).

Substitution contract:

- The first token is always the command keyword and is never eligible for substitution.
- Only operand tokens (positions 2+) are eligible for substitution.
- A token is substituted only when it exactly matches a defined variable name.
- There is no dynamic command construction (variables cannot synthesize/replace the command keyword).

Session `@` names in PROC:

- After local variable lookup, tokens that exactly match `@USERNO`, `@ACCOUNT`, `@LOGNAME`, or `@DEFDATA` (any ASCII case) resolve from the same read-only session snapshot used by Tcl `GET`.
- Assignment or `INPUT` to those names is rejected as read-only.

## Supported statements (current)

- `DISPLAY <tokens...>`: print substituted operands joined by spaces.
- `INPUT <name>`: read one input line and assign to variable.
- `IF <lhs> = <rhs> THEN <statement> [ELSE <statement>]`: conditional execution (exact string equality after substitution).
- `GO <label>`: unconditional jump.
- `RETURN`: flat control transfer that exits the current PROC execution.
- `LOOP` / `REPEAT`: minimal loop construct.
- `EXIT`: leave the current loop.
- `EXITIF <lhs> = <rhs>`: conditional loop exit.
- `SELECT <file>`: populate session active list from file ids.
- `READNEXT <name>`: read next active-list id into a variable (empty string when exhausted).
- `TCL <command...>`: pass reconstructed, substituted operand string to Tcl dispatcher.
- `<name> = <tokens...>`: string assignment.
- `END`: terminate script successfully.

## R83 aliases and long-form canonical keywords

PROC accepts both modern long-form statement keywords and R83-compatible short aliases. Long forms are canonical for documentation, diagnostics, and internal command identity; short forms are compatibility aliases only.

Compatibility and parsing rules:

- Alias and long-form variants execute identical behavior.
- The first token is parsed as a structural command keyword (or assignment target shape) and is never substituted from variables.
- Alias support does not imply dynamic command construction; PROC remains token-driven, not macro-driven.
- Canonical examples: `DISPLAY`/`D`, `INPUT`/`I`, `GO`/`G`, `TCL`/`T`, `END`/`E`, `SELECT`/`S`, `READNEXT`/`RN`.

Labels:

- Defined as single-token lines ending with `:`, for example `MENU:`.
- Lookups are case-insensitive.

## Tcl bridge

`TCL ...` executes through the same Tcl command dispatcher used by interactive `TCL>` input.

- Tcl first-token VOC verb resolution (`V` / `X` / `Q` chain) still applies.
- Tcl operand expansion for session `@` names applies on the bridged line as normal.
- PROC and Tcl variable stores remain separate (PROC variables are not Tcl shell variables).

## Error behavior (current)

Representative runtime errors include:

- `Error: Duplicate label: <LABEL>`
- `Error: Unknown label: <label>`
- `Error: INPUT requires a variable name`
- `Error: IF requires IF <lhs> = <rhs> THEN GO <label>`
- `Error: Read-only system variable`
- `Error: Unknown PROC statement`
- `Error: EXIT outside LOOP`
- `Error: REPEAT without LOOP`

## Compatibility intent

PROC aims to stay a predictable, token-driven interpreter in the R83 spirit, not a macro language. That includes strict command-token treatment and literal, positional substitution behavior.

## See also

- [Developer shell (TCL)](tcl-shell.md) - host shell commands and `PROC` entry command.
- [Project milestones](milestones.md) - roadmap and fidelity goals.
