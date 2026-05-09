# Developer shell (TCL)

The Tcl shell is the host REPL for system commands, BASIC/PROC entry, and filesystem commands.

When a **Gemini catalogue root** is configured, **`main`** runs **`PickCore::BootMonitor`** (**`SYSTEM READY`** is followed by one blank **`stdout`** line), then **`PickCore::LoginService::runCatalogLogin`**. **`LOGON PLEASE:`** and the account share one line (**interactive**: prefix flushed, then **`getline`**; **`MD`/env auto**: echoed account **`endl`** so there is no stdin Return). Successful logon emits **exactly one** flushed newline before **`Shell::runTclRepl()`** (Pick/Mentor blank line **before** Tcl — same boundary for both flows; auto adds an extra `\n` when echoing the account). The Tcl banner has **no** leading newline — see [`gemini-bootstrap.md`](gemini-bootstrap.md). After **`LOGOFF`**, only interactive logon is used (**no auto-login**). The Tcl layer receives a **`PickCore::UserSession`** via **`Shell::attachUserSession`** and then runs **`Shell::runTclRepl()`** only (no nested login loop inside the shell).

- **Prompt:** `TCL> `

Input is tokenized by whitespace (filenames with spaces are not supported by the tokenizer).

## Tcl command verb (first token)

The first token of each interactive Tcl line is resolved through logical file **`VOC`**: **`V`**- and **`X`**-type entries map a dictionary name to another verb name, with **`Q`** indirection, the same way as the PROC **`TCL ...`** bridge (see [Program resolution](#program-resolution-voc-backed)). The resolved verb is then canonicalized to **ASCII uppercase** for dispatch to the built-in command set. If there is no applicable VOC chain, the token is still uppercased for lookup. Operand expansion for **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** runs **after** this step so aliases do not break **`SET`**, **`GET`**, **`UNSET`**, **`RUN`**, or **`PROC`** argument rules.

## Source layout

- Tcl host shell and filesystem command integration: `src/userland/tcl/` (VOC resolution lives in [`src/core/voc/`](../src/core/voc/), used by [`ShellSession`](../src/userland/tcl/ShellSession.h))
- BASIC line buffer + BASIC command interpreter (`EDIT` delegates to the system line editor in `src/userland/tcl/LineRecordEditor.*`): `src/userland/basic/`
- PROC interpreter (host token interpreter): `src/userland/proc/` (language semantics: [PROC language](proc.md))
- BASIC compiler is implemented in `src/userland/basic/BasicCompiler.*`.
- BASIC mode command semantics are documented in [BASIC shell](basic-shell.md); language/compiler semantics are documented in [BASIC language](basic-language.md).
- ASM debugger mode command semantics are documented in [Assembler shell](assembler-shell.md).

## Program resolution (VOC-backed)

`BASIC`, `RUN`, `EDIT` (shorthand form), and `LIST-PROGRAMS` resolve program names through logical file `VOC` (Pick records), not by scanning host program-path directories.

- Supported VOC entry types for this milestone: `F`, `Q`, `V`, `D`, `A`, `X` (Gemini subset — see below).
- **`F`**, **`D`**, **`A`**: attribute 2 = logical Pick file name; **`D`**/**`A`** are treated like **`F`** for program and PROC script file resolution (not full Pick data-dictionary **`D`** semantics).
- **`Q`**: attribute 2 = next VOC key (recursive for file resolution; also followed for verb resolution).
- **`V`**, **`X`**: attribute 2 = target verb name for Tcl’s first-token resolution; **`X`** does **not** execute multi-line TCL stored in the VOC body (alias-only, same role as **`V`** for dispatch).
- **`resolveVerbName`** follows **`Q`**/**`V`**/**`X`** chains with cycle detection and a hop limit (64); landing on **`F`**/**`D`**/**`A`** stops with the last resolved verb string.
- Resolution for program names follows case-insensitive lookup with Q-chain recursion and BP fallback.
- Resolved location is a `(file, key)` pair used for record I/O via filesystem API.
- Source record key = `<programName>`.
- Object record key = `<programName>_OBJ` in the same resolved Pick file.

`PROC` is also filesystem/VOC-backed:

- script lookup for `PROC <name>` uses `F`/`Q`/`D`/`A` VOC resolution with fallback to `(PROC, <name>)`.
- `V` and `X` entries are not used for script lookup.
- Interactive Tcl and PROC `TCL ...` bridge lines share the same first-token path: **`V`** and **`X`** entries in `VOC` map the verb (including multi-hop through **`Q`**/**`V`**/**`X`**) before builtin dispatch; subsequent tokens follow normal Tcl operand and command rules.

## Filesystem directory

Filesystem commands use a separate root (default directory name **`filesystem`**, relative to the process current working directory unless changed in code via **`Shell::setFileSystemRoot`**).

For the full current behavior (validation rules, storage model, VOC behavior, error modes, ordering, and persistence details), see [File system](filesystem.md).
For ENGLISH command forms and worked query examples, see [ENGLISH query core](english.md).

## TCL commands

| Command | Description |
|---------|-------------|
| **`HELP`** | Short command list (same information as this table, in the binary). |
| **`VERSION`** | Title, project version string, and build date. |
| **`WHO`** | Print `port username account` when logged in with a Gemini catalogue (account-only logon uses the same string for username and account until a user model exists); otherwise **`0 - -`**. |
| **`QUIT`** | Exit the shell; clears loaded program state in the VM, shell-held bytecode metadata, and **shell variables**. |
| **`ECHO`** … | Echo remaining tokens, **space-separated**. Known session tokens **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** (case-insensitive) expand from the session before **`$`** rules. Within each token, scan left to right: **`$$`** becomes a single **`$`**; **`$Name`** ( **`Name`** = letters, digits, underscore, **`@`**, at least one character) is replaced by that shell variable’s value, or—if **`Name`** is one of the four **`@`** system names—by the session value; otherwise nothing if unset; any other **`$`** is echoed literally. |
| **`SET`** *name* *word…* | Set a **shell variable**. Names are **case-insensitive**; the canonical name is **ASCII uppercase** (what **`LIST-VARS`** shows). The value is the remaining tokens joined with single spaces; **`SET`** *name* alone sets an empty string. Variable names must be non-empty. Targets **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** are rejected (**`Read-only system variable`**). Value tokens may include those **`@`** names; they expand from the session when used as operands (not as the **`SET`** target name). |
| **`GET`** *name* | Print the variable’s value and a newline. **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** read from the session (not from the shell variable map). Other names use the shell map; if unset: **`No such variable:`** *name* (as you typed it). |
| **`LIST-VARS`** | List shell variable names in **ASCII uppercase** (sorted), each on its own line under a **`Variables:`** header. When logged in with a Gemini session, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@USERNO`** are included once (they are not stored as ordinary shell variables). When **`MD,DEFDATA`** defines a default data file, **`@DEFDATA`** is included once. If none: **`No variables`**. Takes no arguments. |
| **`UNSET`** *name* | Remove *name* from the shell map. The four session **`@`** names cannot be unset (**`Read-only system variable`**). If it was not set: **`No such variable`**. |
| **`BASIC`** [*name*] | Enter BASIC mode. See [BASIC shell](basic-shell.md) for BASIC/ED commands and persistence rules. |
| **`ASM`** [*programName*] | Enter ASM mode (instruction-level debugger shell). Optional name immediately executes `RUN <programName>` after entering ASM. See [Assembler shell](assembler-shell.md). |
| **`RUN`** *programName* | Run by program name only (no extension). Tcl resolves `(file,key)` via VOC, prunes invalid breakpoints, and executes object record `<key>_OBJ`. If object is missing, Tcl compiles source record `<key>` and writes `<key>_OBJ` in the same resolved file before running. |
| **`PROC`** *programName* [*args...*] | Resolve script via VOC/filesystem (`F/Q` with fallback to `PROC` file), then execute with the host PROC interpreter. Positional args map to `P1`, `P2`, ... . Full language semantics: [PROC language](proc.md). |
| **`LIST-PROGRAMS`** | List logical program names from VOC-resolved program files (records in those files, excluding `_OBJ` records), deduplicated and sorted. |
| **`CREATE-FILE`** *name* | Create a Pick file in the filesystem root. Creates logical file directory. |
| **`DELETE-FILE`** *name* | Delete a Pick file from the filesystem root. |
| **`LIST-FILES`** | List Pick file names from the filesystem root (sorted). If none: **`No files`**. Takes no arguments. |
| **`LIST`** … | **Contextual dispatch.** Legacy `LIST <file>` (two tokens, `<file>` an existing logical file) lists record keys. ENGLISH-shaped lines (additional tokens; `WITH` / `BY` keywords; two-token projection when `<file>` token is **not** an existing logical file and an active list exists) route to [ENGLISH](english.md). `BY` on `LIST` is an error — use **`SORT`**. |
| **`COUNT`** … | **`COUNT <file>`** counts the logical file on ENGLISH semantics. **`COUNT`** alone counts the active list IDs when **`SELECT`** has populated the list (see [ENGLISH](english.md)). Otherwise: **`COUNT requires filename or active-list scope`**. |
| **`SELECT`** *file* [*field...*] | ENGLISH: scans *file*, saves record IDs plus **source logical file name** for implicit ENGLISH scope, prints `N records selected`. See [ENGLISH](english.md). |
| **`SORT`** … | ENGLISH-shaped **`SORT`** uses the **`LIST`** pipeline plus stable ordering (`BY`, `BY-DSND`; default sort by **item-id** when there is no `BY`). **`WITH`** clause tokens are absorbed (selection not evaluated yet). Lines that **do not** match ENGLISH-shaped rules are reserved for future legacy Tcl **SORT**; the shell prints **`SORT: ENGLISH syntax not detected; legacy Tcl SORT is not implemented`**. Detail: [ENGLISH](english.md). |
| **`LIST-LIST`** | Print current session active list IDs under `Active list:`. If no active list exists: `No active list`. Takes no arguments. |
| **`CLEAR-LIST`** | Clear current session active list and print `Active list cleared`. Takes no arguments. |
| **`RESOLVE-FIELD`** *data-file* *token* | ENGLISH/DICT diagnostic: prints `DICT-<data-file>` naming, resolved attribute index (if any), and `D` / `MD` / `MC` hint. See [ENGLISH](english.md). |
| **`DEFINE-FIELD`** *dict-file* *field-name* *attribute-number* | Non–R83 helper: writes a minimal type-**`A`** item into **`DICT`** / **`DICT-<file>`** (four Tcl tokens total). **`dict-file`** must exist (**`CREATE-FILE`** first). Example: **`DEFINE-FIELD DICT NAME 1`**. Leaves **`WRITE`** as single-string full-replace; see [ENGLISH](english.md). |
| **`READ`** *file* *record-name* \| **`READ`** *record-name* | Print record value if present; if missing record: **`No such record`**. Two-token form is allowed only when **`MD,DEFDATA`** sets a default logical file (see [Gemini bootstrap](gemini-bootstrap.md)). |
| **`WRITE`** *file* *record-name* *value…* \| **`WRITE`** *record-name* *value* | Upsert record value. Value is remaining tokens joined with single spaces. Two-token value form (**`WRITE`** *record* *value*) is allowed only when **`MD,DEFDATA`** is set and *value* is a **single** token; use the full three-argument form for multi-word values. |
| **`EDIT`** *file* *record-name* \| **`EDIT`** *programName* | Open a **blocking** line-oriented record editor (prompt **`ED> `**) on an in-memory copy of the record. Shorthand *programName* resolves the **source** record `(file, key)` via VOC (same as `BASIC` / `RUN` source), never `_OBJ`. Missing record starts with an empty buffer. See [TCL EDIT](#tcl-edit-line-record-editor) below. |
| **`DUMP-STACK`** | Print the VM stack (uses the command output stream). |
| **`LOGTO`** *account* | After catalogue login: switch Pick root to another account from **`ACCOUNTS.json`** (requires **`VOC/`** under the resolved path). |
| **`LOGOFF`** | After catalogue login: clear session and return control to **`main`** so the host can run the core **LOGON** phase again (not an inner Tcl loop). |

Unknown first token: **`Unknown command: …`**.

## TCL EDIT (line record editor)

`EDIT` is a **Tcl-level** session (not BASIC mode). **BASIC EDIT** uses the same **`LineRecordEditor`** implementation via an internal host callback (flush current source, run the editor, reload from disk); see [BASIC shell](basic-shell.md).

- **Entry:** `EDIT <file> <record>` or `EDIT <programName>` (program name without extension, same rule as **`RUN`**).
- **Input:** Commands are **case-insensitive**; friendly verbs and **R83-style aliases** invoke the same behaviour: **LIST** (**L**), **INSERT** (**I**), **REPLACE** (**R**), **DELETE** (**D**), **SAVE** (**FI**), **QUIT** (**Q**), **HELP**.
- **LIST:** No arguments lists all physical lines (1-based line numbers). Optional **`LIST`** *n* or **`LIST`** *start-end* for a range.
- **INSERT:** Optional line number *n* (insert **after** line *n*; omit to append at end). Then type body lines; a line containing **only** `.` (after trimming ends) ends insertion. Body lines are stored verbatim (no command parsing inside the block).
- **REPLACE:** Requires *n*, then the same **`.`**-terminated multiline block; replaces the single existing line *n* with one or more new lines (at least one line required before `.`).
- **DELETE:** Requires *n*; deletes one physical line.
- **SAVE / FI:** Writes the buffer back to the same `(file, record)` opened for editing (creates the file if missing, like other writes). **`VOC`** writes invalidate the in-memory VOC cache.
- **QUIT / Q:** Leave the editor **without** writing.

## BASIC mode

- Tcl shell behavior is described in this page.
- BASIC command set and persistence (including **`EDIT`** delegating to the TCL line editor) are documented in [BASIC shell](basic-shell.md).
- BASIC compiler/language subset details are documented in [BASIC language](basic-language.md).

## PROC mode

`PROC` runs host-interpreted scripts (no VM bytecode) via `PROC <programName> [args...]`. Script lookup and Tcl-bridge behavior are documented here; the full PROC language contract (execution model, substitution rules, statements, and error behavior) lives in [PROC language](proc.md).

## Shell variables

Variables are **string key/value** pairs held by the shell (not the VM stack). Names are **case-insensitive**; the shell stores and lists them in **ASCII uppercase** (see **`LIST-VARS`**). Because input is whitespace-tokenized with no quoting, values **cannot contain spaces** unless you express them as multiple tokens (which are joined with single spaces), same idea as **`ECHO`**.

### Session **`@`** names (read-only)

**`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** are **session fields**, not entries in the Tcl environment map. They follow the same canonicalization as other names (ASCII uppercase for letters). Catalogue identity values come from logon (**`LOGTO`** updates the session; **`LOGOFF`** clears them; **`QUIT`** clears shell variables but leaves the Pick root and **`@DEFDATA`** until the root changes). **`@DEFDATA`** is the logical file name from **`MD,DEFDATA`** when that record is valid, otherwise empty. For Tcl commands, operand tokens (all arguments after the verb except the variable name slot on **`SET`** / **`GET`** / **`UNSET`**, and the program name on **`RUN`** / **`PROC`**) that are exactly those four names expand to their current string values; other **`@FOO`** tokens are left unchanged. **`GET`** reads the session fields directly (when not logged in, **`@ACCOUNT`** / **`@LOGNAME`** are typically empty and **`@USERNO`** defaults to **`0`**). The active list (**IDs plus the logical `activeListSourceFile` from `SELECT`**) is session-scoped state: **`LIST-LIST`**, **`CLEAR-LIST`**, **`LOGOFF`**, **`QUIT`** (session reset), and changing the Tcl **filesystem root** clear it ([ENGLISH](english.md)).

**`ECHO`:** each argument token is scanned for **`$$`** (escaped dollar), **`$Name`** substitution, and literal **`$`**. There is **no** expression evaluation. Unset **`$Name`** expands to an empty segment.

Arity errors: **`SET`** / **`GET`** / **`UNSET`** without a name print a **`… requires a variable name`** line. Extra tokens on **`LIST-VARS`** print **`LIST-VARS takes no arguments`**. Filesystem arity errors follow the same pattern (for example, **`LIST requires a filename`**, **`READ`** / **`WRITE`** messages that include the optional **`MD DEFDATA`** shorthand when the default is not set).

Instruction-level debugger controls (`STEP`, `CONT`, `TRACE`, breakpoint management, and program/label dumps) now live in [Assembler shell](assembler-shell.md).

## Errors

Parse/runtime failures during **`RUN`** print **`Error:`** followed by the exception message. Filesystem command failures also print **`Error:`** with a specific reason (for example missing file or invalid record/file name). The VM’s output stream is cleared back to default after **`RUN`** attempts.

## See also

- [Bytecode VM](vm.md) — opcode reference and parser/runtime details.
- [Assembler shell](assembler-shell.md) — instruction-level debugger shell.
