# BASIC shell

The BASIC shell is a mode inside the interactive shell focused on editing and persisting numbered BASIC source lines.

- Enter from Tcl mode with `BASIC` or `BASIC <name>`.
- Prompt is `BASIC> `.
- `EDIT <line>` enters editor submode with prompt `ED> `.
- Tcl host shell behavior is documented in [Developer shell (TCL)](tcl-shell.md).

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
| `RENUM` / `RENUMBER` | Renumber lines to `10,20,30,...` preserving order. |
| `NEW` | Clear all in-memory lines for the active BASIC program context. |
| `LOAD [name]` | Reload from disk using explicit name or active program name. Missing files load as empty. |
| `SAVE [name]` | Save using explicit name or active program name. |
| `QUIT` | Exit BASIC mode and return to `TCL>`. |

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
