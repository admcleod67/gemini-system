# Developer shell (TCL)

The Tcl shell is the host REPL for VM/debug/filesystem commands.

- **Prompt:** `TCL> `

Input is tokenized by whitespace (filenames with spaces are not supported by the tokenizer).

## Source layout

- Tcl host shell and VM/filesystem command integration: `src/userland/tcl/`
- BASIC line buffer + BASIC/ED command interpreter: `src/userland/basic/`
- BASIC compiler is implemented in `src/userland/basic/BasicCompiler.*`.
- BASIC mode command semantics are documented in [BASIC shell](basic-shell.md); language/compiler semantics are documented in [BASIC language](basic-language.md).

## Programs directory

**`RUN`** resolves relative paths against the **programs root** (default directory name **`programs`**, relative to the process current working directory unless changed in code via **`Shell::setProgramsRoot`**).

**`LIST-PROGRAMS`** lists **`.tbc`** files in that root.

## Filesystem directory

Filesystem commands use a separate root (default directory name **`filesystem`**, relative to the process current working directory unless changed in code via **`Shell::setFileSystemRoot`**).

Each Pick file is stored as JSON:

```json
{
  "name": "FILENAME",
  "records": {
    "KEY1": "raw record string",
    "KEY2": "another record"
  }
}
```

## TCL commands

| Command | Description |
|---------|-------------|
| **`HELP`** | Short command list (same information as this table, in the binary). |
| **`VERSION`** | Title, project version string, and build date. |
| **`QUIT`** | Exit the shell; clears loaded program state in the VM, shell-held bytecode metadata, and **shell variables**. |
| **`ECHO`** … | Echo remaining tokens, **space-separated**. Within each token, scan left to right: **`$$`** becomes a single **`$`**; **`$Name`** ( **`Name`** = letters, digits, underscore, at least one character) is replaced by that variable’s value, or nothing if unset; any other **`$`** is echoed literally. |
| **`SET`** *name* *word…* | Set a **shell variable**. Names are **case-insensitive**; the canonical name is **ASCII uppercase** (what **`LIST-VARS`** shows). The value is the remaining tokens joined with single spaces; **`SET`** *name* alone sets an empty string. Variable names must be non-empty. |
| **`GET`** *name* | Print the variable’s value and a newline. If unset: **`No such variable:`** *name* (as you typed it). |
| **`LIST-VARS`** | List variable names in **ASCII uppercase** (sorted), each on its own line under a **`Variables:`** header. If none: **`No variables`**. Takes no arguments. |
| **`UNSET`** *name* | Remove *name*. If it was not set: **`No such variable`**. |
| **`BASIC`** [*name*] | Enter BASIC mode. See [BASIC shell](basic-shell.md) for BASIC/ED commands and persistence rules. |
| **`RUN`** *file* | Parse *file*, prune invalid breakpoints (see below), load into the VM, then execute (with trace/breakpoints as configured). |
| **`RUN`** | **Resume** after a breakpoint: no filename, only when execution is **suspended** at a breakpoint; continues until the next breakpoint, **`HALT`**, or end of program. |
| **`LIST-PROGRAMS`** | List `.tbc` files under the programs root. |
| **`CREATE-FILE`** *name* | Create a Pick file in the filesystem root. Creates JSON with matching `name` and empty `records`. |
| **`DELETE-FILE`** *name* | Delete a Pick file from the filesystem root. |
| **`LIST-FILES`** | List Pick file names from the filesystem root (sorted). If none: **`No files`**. Takes no arguments. |
| **`LIST`** *file* | List record names (keys only) for *file*, sorted, under a **`Records:`** header. If none: **`No records`**. |
| **`READ`** *file* *record-name* | Print record value if present; if missing record: **`No such record`**. |
| **`WRITE`** *file* *record-name* *value…* | Upsert record value. Value is remaining tokens joined with single spaces. |
| **`DUMP-STACK`** | Print the VM stack (uses the command output stream). |
| **`TRACE ON`** / **`TRACE OFF`** | Enable or disable per-instruction tracing during **`RUN`** / resume loops. Does not require a loaded program. |
| **`STEP`** | Single-step **one** instruction if a program is loaded; prints the same line format as trace for that step. |
| **`BREAKPOINT`** *n* | Record a breakpoint at instruction index *n* (non-negative integer). May be set **before** any **`RUN`**. Label-based breakpoints are not supported. |
| **`BREAKPOINTS`** | List breakpoints (sorted). If none: **`No breakpoints`**. |
| **`CLEAR-BREAKPOINT`** *n* | Remove breakpoint *n*; if missing: **`No such breakpoint`**. |
| **`CLEAR-BREAKPOINTS`** | Remove all breakpoints. |
| **`DUMP-PROGRAM`** | Disassemble the **currently loaded** program (same mnemonic style as trace). |
| **`DUMP-LABELS`** | Print **`label -> index`** for the loaded program, sorted by label name. If no labels: **`No labels`**. |

Unknown first token: **`Unknown command: …`**.

## BASIC mode

Tcl host and BASIC editor responsibilities are now separated:

- Tcl shell behavior is described in this page.
- BASIC/ED command set and persistence behavior are documented in [BASIC shell](basic-shell.md).
- BASIC compiler/language subset details are documented in [BASIC language](basic-language.md).

## Shell variables

Variables are **string key/value** pairs held by the shell (not the VM stack). Names are **case-insensitive**; the shell stores and lists them in **ASCII uppercase** (see **`LIST-VARS`**). Because input is whitespace-tokenized with no quoting, values **cannot contain spaces** unless you express them as multiple tokens (which are joined with single spaces), same idea as **`ECHO`**.

**`ECHO`:** each argument token is scanned for **`$$`** (escaped dollar), **`$Name`** substitution, and literal **`$`**. There is **no** expression evaluation. Unset **`$Name`** expands to an empty segment.

Arity errors: **`SET`** / **`GET`** / **`UNSET`** without a name print a **`… requires a variable name`** line. Extra tokens on **`LIST-VARS`** print **`LIST-VARS takes no arguments`**. Filesystem arity errors follow the same pattern (for example, **`LIST requires a filename`**, **`READ requires a file and record name`**, **`WRITE requires a file, record name, and value`**).

## Loaded program and messages

These commands require a **program image** in the shell **and** a non-empty program in the VM (after a successful **`RUN`** parse/load, including when paused on a breakpoint or finished at **`HALT`**):

- **`STEP`**, **`DUMP-PROGRAM`**, **`DUMP-LABELS`**

Otherwise they print exactly:

**`No program loaded`**

**`STEP`** when the IP is already past the end (finished run): **`Program finished`**.

## Trace and breakpoints

- **Trace:** when **`TRACE ON`**, before each **`step()`**, the shell prints one line via **`formatInstructionLine`** (0-based IP, mnemonic, operands, optional **`(line L)`**). **`HALT`** is included as a line immediately before it executes.
- **Breakpoints:** checked **before** executing the instruction at the current IP. On hit, the shell prints **`Breakpoint hit at`** *ip* and **does not** advance the IP until you **`STEP`**, **`RUN`** (resume), or clear breakpoints.
- **Resume:** bare **`RUN`** sets a one-shot skip so the instruction at the stopped-at PC is not treated as a breakpoint again immediately, allowing progress after a hit.

## Breakpoint validation

On each successful **`RUN`** *file*, breakpoint indices **`>= program.size()`** are **removed** from the set and a single line is printed:

**`Removed invalid breakpoint(s):`** *indices…* (sorted).

## Errors

Parse/runtime failures during **`RUN`** print **`Error:`** followed by the exception message. Filesystem command failures also print **`Error:`** with a specific reason (for example missing file or invalid JSON schema). The VM’s output stream is cleared back to default after **`RUN`** attempts.

## See also

- [Bytecode VM](vm.md) — opcode reference and parser/runtime details.
