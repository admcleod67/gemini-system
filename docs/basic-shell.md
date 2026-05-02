# BASIC shell

The BASIC shell is a mode inside the interactive shell focused on editing and persisting numbered BASIC source lines, then invoking compile/run workflows.

- Enter from Tcl mode with `BASIC` or `BASIC <name>`.
- Prompt is `BASIC> `.
- `EDIT` opens the **system line editor** (same blocking **`ED> `** session as Tcl `EDIT` on the current program’s source record). See [TCL EDIT (line record editor)](tcl-shell.md#tcl-edit-line-record-editor) for **LIST**, **INSERT**, **REPLACE**, **DELETE**, **SAVE** / **FI**, **QUIT** / **Q**, and aliases.
- Tcl host shell behavior is documented in [Developer shell (TCL)](tcl-shell.md).
- BASIC language/compiler semantics are documented in [BASIC language](basic-language.md).

## Program naming and persistence

- BASIC source is in-memory until `SAVE`.
- `BASIC <name>` autoloads source from the **Pick filesystem** when a record for `<name>` exists at the location resolved from **VOC** ([Program resolution](tcl-shell.md#program-resolution-voc-backed)). The host **`programs/`** directory is for sample bytecode artifacts only—not where BASIC persists programs.
- If `<name>` does not exist, an empty in-memory program is started and no file is created until `SAVE` (or flush before `EDIT`).
- `LOAD` replaces the in-memory program from disk (using explicit name or current active name).
- `SAVE` writes only the in-memory BASIC **source** Pick record (`<name>` in the resolved logical file).
- `COMPILE` compiles in memory and, on success, writes **serialized bytecode text** to the **object** Pick record **`<name>_OBJ`** in that same logical file (see Tcl shell docs for VOC-backed keys).
- Failed `COMPILE` leaves any existing **`<name>_OBJ`** record unchanged.
- In unnamed sessions (`BASIC` without a name), `SAVE`, `COMPILE`, `RUN`, and `EDIT` fail with:
  - `No program name`

## BASIC commands

| Command | Description |
|---------|-------------|
| `<line> <text...>` | Insert or replace a BASIC line. |
| `<line>` | Delete a BASIC line. |
| `LIST` | List all lines in numeric order. |
| `EDIT` | Open the system line editor on the current program’s source record. Current in-memory lines are **flushed to disk** first; when the editor exits, the program is **reloaded from disk** (so **`SAVE`/`FI`** in the editor updates BASIC’s buffer; **`QUIT`/`Q`** leaves the buffer matching the last flush plus any file changes from the session). |
| `EDIT <line>` | Same as `EDIT`, but requires that **BASIC** source line number to exist; the editor prints that statement’s **physical record line** once for context before the first `ED> ` prompt. |
| `DELETE <line>` | Delete one line. |
| `DELETE <start-end>` | Delete an inclusive range. |
| `RENUM` / `RENUMBER` | Renumber lines to `10,20,30,...` preserving order, and rewrite supported control-flow targets (`GOTO`, `IF ... THEN ... [ELSE ...]`) when target lines exist. |
| `NEW` | Clear all in-memory lines for the active BASIC program context. |
| `LOAD [name]` | Reload from disk using explicit name or active program name. Missing files load as empty. |
| `SAVE [name]` | Save using explicit name or active program name. |
| `COMPILE` | Compile current source and persist **`<name>_OBJ`** (text bytecode) on success (requires active program name). |
| `RUN` | Always recompile then execute from memory; quiet on compile success. |
| `RUN (C` | Compile-only alias of `COMPILE` (reports compile status, writes **`<name>_OBJ`**, does not execute). |
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

On success, **`COMPILE`** and **`RUN (C)`** persist object code to Pick record **`<name>_OBJ`** under the same VOC-resolved logical file as the source. **`RUN`** always recompiles then executes from memory (it does not rely on a standalone host `programs/<name>.tbc` path for BASIC programs).

Language details for subset v1 (statements, expression rules, diagnostics, and file handling statements `OPEN`/`READ`/`WRITE`/`CLOSE`) are documented in [BASIC language](basic-language.md).

## Command validation notes

- `BASIC` with too many arguments: `BASIC takes at most one program name`
- `SAVE` with too many arguments: `SAVE takes at most one filename`
- `LOAD` with too many arguments: `LOAD takes at most one filename`
- malformed `DELETE` ranges (for example `20-10`): `DELETE requires a line number or range`
- `EDIT` with too many arguments: `EDIT takes no arguments or one line number`
- `EDIT` with a line number that does not exist: `No such line: <n>`
- `EDIT` with a non-numeric operand: `EDIT requires a line number`
