# BASIC shell

The BASIC shell is a mode inside the interactive shell focused on editing and persisting numbered BASIC source lines, then invoking compile/run workflows.

- Enter from Tcl mode with `BASIC` or `BASIC <name>`.
- Prompt is `BASIC> `.
- `EDIT <line>` enters editor submode with prompt `ED> `.
- Tcl host shell behavior is documented in [Developer shell (TCL)](tcl-shell.md).
- BASIC language/compiler semantics are documented in [BASIC language](basic-language.md).

## Program naming and persistence

- BASIC source is in-memory until `SAVE`.
- `BASIC <name>` autoloads from the programs root when `<name>` exists.
- If `<name>` does not exist, an empty in-memory program is started and no file is created.
- `LOAD` replaces the in-memory program from disk (using explicit name or current active name).
- `SAVE` writes the in-memory program to disk.
- In unnamed sessions (`BASIC` without a name), bare `SAVE` fails with:
  - `No program name specified`

## BASIC commands

| Command | Description |
|---------|-------------|
| `<line> <text...>` | Insert or replace a BASIC line. |
| `<line>` | Delete a BASIC line. |
| `LIST` | List all lines in numeric order. |
| `EDIT <line>` | Enter `ED>` for an existing line. |
| `DELETE <line>` | Delete one line. |
| `DELETE <start-end>` | Delete an inclusive range. |
| `RENUM` / `RENUMBER` | Renumber lines to `10,20,30,...` preserving order, and rewrite supported control-flow targets (`GOTO`, `IF ... THEN ... [ELSE ...]`) when target lines exist. |
| `NEW` | Clear all in-memory lines for the active BASIC program context. |
| `LOAD [name]` | Reload from disk using explicit name or active program name. Missing files load as empty. |
| `SAVE [name]` | Save using explicit name or active program name. |
| `COMPILE` | Compile-only check for BASIC language subset v1; then discard compiled output. |
| `RUN` | Always recompile then execute from memory; quiet on compile success. |
| `RUN (C` | Compile-only alias of `COMPILE` (reports compile status, does not execute). |
| `QUIT` | Exit BASIC mode and return to `TCL>`. |

When renumbering, targets that do not resolve to an existing pre-renumber line are left unchanged.

## Compile and run behavior (v1)

Successful `COMPILE` and `RUN (C)` output:

```text
Compiled successfully.
Instructions: <n>
Labels: <m>
```

Failure output:

```text
Error on line <basicLine>: <message>
Compilation failed.
```

Compiled programs are in-memory only; no compiled `.tbc` artifact is written.

Language details for subset v1 (statements, expression rules, diagnostics, and file handling statements `OPEN`/`READ`/`WRITE`/`CLOSE`) are documented in [BASIC language](basic-language.md).

## Editor submode (`ED>`)

Editor mode is line-scoped and entered via `EDIT <line>`.

| Command | Description |
|---------|-------------|
| `C/old/new/` | Replace first occurrence of `old` with `new` on the selected line. |
| `FI` | Finish editing and return to `BASIC>`. |

## Command validation notes

- `BASIC` with too many arguments: `BASIC takes at most one program name`
- `SAVE` with too many arguments: `SAVE takes at most one filename`
- `LOAD` with too many arguments: `LOAD takes at most one filename`
- malformed `DELETE` ranges (for example `20-10`): `DELETE requires a line number or range`
- malformed editor substitute command: `Invalid edit command`
