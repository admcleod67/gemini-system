# Developer shell (TCL)

The Tcl shell is the host REPL for system commands, BASIC/PROC entry, and filesystem commands.

When a **Gemini catalogue root** is configured, **`main`** creates a **`PickShell::GeminiSession`** (`GeminiSession::create()`), runs **`PickCore::BootMonitor`** on process **`stdout`** (**`SYSTEM READY`** is followed by one blank **`stdout`** line), then **`PickCore::LoginService::runCatalogLogin`** on the session’s input/output channels. **`LOGON PLEASE:`** and the account share one line (**interactive**: prefix flushed, then **`getline`**; **`MD`/env auto**: echoed account **`endl`** so there is no stdin Return). Successful logon emits **exactly one** flushed newline before **`Shell::runTclRepl()`** (Pick/Mentor blank line **before** Tcl — same boundary for both flows; auto adds an extra `\n` when echoing the account). The Tcl banner has **no** leading newline — see [`gemini-bootstrap.md`](gemini-bootstrap.md). After **`LOGOFF`**, only interactive logon is used (**no auto-login**). On success, **`main`** calls **`GeminiSession::attach()`** with the **`PickCore::UserSession`** from login ( **`Shell::attachUserSession`** is a thin delegate), then **`Shell::runTclRepl()`** only (no nested login loop inside the shell).

- **Prompt:** `TCL> `

Input uses Tcl shell tokenization with quoted strings and backslash escapes:

- Double quotes group tokens that include spaces.
- Quoted empty tokens (`""`) are preserved.
- Backslash escapes the next character.
- Malformed quoting/escaping yields an `Error: ...` line.

## Tcl command verb (first token)

The first token of each interactive Tcl line is resolved through logical file **`VOC`**: **`V`**- and **`X`**-type entries map a dictionary name to another verb name, with **`Q`** indirection, the same way as the PROC **`TCL ...`** bridge (see [Program resolution](#program-resolution-voc-backed)). The resolved verb is then canonicalized to **ASCII uppercase** for dispatch to the built-in command set. If there is no applicable VOC chain, the token is still uppercased for lookup. Operand expansion for **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** runs **after** this step so aliases do not break **`SET`**, **`GET`**, **`UNSET`**, **`RUN`**, or **`PROC`** argument rules.

## Source layout

- Tcl host shell and session state: `src/userland/tcl/` — [`GeminiSession`](../src/userland/tcl/GeminiSession.h) owns runtime, shell, account binding, and I/O; [`Shell`](../src/userland/tcl/Shell.h) is the interpreter front-end (VOC resolution lives in [`src/core/voc/`](../src/core/voc/))
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
| **`HELP`** [*topic-operands…*] | File-backed help: operands (if any) define a **topic**; see [HELP system](help-system.md). No-arg form resolves topic **`HELP`** from the `HELP` logical file (then SYSPROG, then builtins), else a short built-in usage line. **`HELP-LIST`** lists account-local topic names. **`HELP-EDIT`** *topic…* opens the line editor on that topic (creates **`HELP`** file if needed). Multi-word topics use underscores in the underlying record id (`HELP BASIC` ↔ `HELP_BASIC`). **`HELP COMMANDS`** (two tokens) or **`HELP HELP COMMANDS`** generates a sorted list of Tcl verbs plus VOC spellings that dispatch to Tcl (logical file **`HELP`** record **`HELP_COMMANDS`** overrides if present). Unknown topics: `No help available for "<topic>".` |
| **`HELP-LIST`** | Lists canonical HELP topic names from the current account’s logical file **`HELP`** (sorted), or `No HELP topics` when missing/empty. Takes no arguments. |
| **`HELP-EDIT`** *topic-operands…* | Same topic rules as **`HELP`**; runs **`ED>`** on the resolved record in **`HELP`** (creates the logical file first if absent). |
| **`VERSION`** | Title, project version string, and build date. |
| **`SYSTEM`** | Gemini introspection extension: bare **`SYSTEM`** prints line-oriented system/environment summary (title, version, build date, session identity, Pick root/account context, catalogue root context). **`SYSTEM LANGUAGES`** lists boot-registered language namespaces (id, name, version, function count), module boot summary, and any boot load failures. **`SYSTEM LANGUAGES VERBOSE`** adds per-function-slot arity lines. **`SYSTEM STATUS`** aliases top-level **`STATUS`** (SYSPROG only). **`SYSTEM SHUTDOWN`** aliases top-level **`SHUTDOWN`** (SYSPROG only). Unknown subcommands (for example **`SYSTEM FOO`**) report **`SYSTEM: unknown subcommand "FOO"`**. |
| **`ABOUT`** | Literal alias of bare **`SYSTEM`** (no subcommands). |
| **`SHOW-MODULES`** | Alias of **`SYSTEM LANGUAGES`** — lists loaded language namespaces and boot module diagnostics. Takes no arguments. |
| **`WHO`** | Print `port username account` when logged in with a Gemini catalogue (account-only logon uses the same string for username and account until a user model exists); otherwise **`0 - -`**. |
| **`LISTSESSIONS`** | **SYSPROG** only. Whitespace-separated table of all daemon sessions: `PORT BOUND USER ACCOUNT STATE` (header), then one row per port sorted ascending. Bound console is `yes`/`no`; logged-out identity is `- -`; state is `Runnable` / `Running` / `WaitingForInput`. Non-SYSPROG (or not logged in): `LISTSESSIONS requires SYSPROG`. |
| **`STATUS`** | **SYSPROG** only. Three lines: `Gemini System <version>`, `sessions=N maxSessions=M`, `socket=<path|none>`. Non-SYSPROG: `STATUS requires SYSPROG`. Same output via **`SYSTEM STATUS`**. |
| **`KILLSESSION`** *port* | **SYSPROG** only. Destroy another session by port: unbind its console if attached, retire scheduler state, release locks, free the port. Cannot target the calling session (`KILLSESSION: cannot kill current session` — use **`SHUTDOWN`**). Unknown port: `KILLSESSION: session not found`. Contrast: **`LOGOFF`** ends login only; **`QUIT`** ends the REPL/worker without destroying the session object for re-attach. |
| **`SHUTDOWN`** | **SYSPROG** only. Stop the whole system: same graceful teardown as daemon **SIGTERM** / IPC **`ShutdownRequest`** (detach all, destroy sessions, unlink socket, exit). On embedded **`gemini-system`**, exits the process after teardown. Prints `Shutting down` and ends the issuing REPL. Same via **`SYSTEM SHUTDOWN`**. Not the same as **`QUIT`** (one session) or **`KILLSESSION`** (one other port). |
| **`QUIT`** | Exit the shell; clears loaded program state in the VM, shell-held bytecode metadata, and **shell variables**. |
| **`ECHO`** … | Echo remaining tokens, **space-separated**. Known session tokens **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** (case-insensitive) expand from the session before **`$`** rules. Within each token, scan left to right: **`$$`** becomes a single **`$`**; **`$Name`** ( **`Name`** = letters, digits, underscore, **`@`**, at least one character) is replaced by that shell variable’s value, or—if **`Name`** is one of the four **`@`** system names—by the session value; otherwise nothing if unset; any other **`$`** is echoed literally. `$` substitution remains ECHO-scoped in this milestone. |
| **`SET`** *name* *word…* | Set a **shell variable**. Names are **case-insensitive**; the canonical name is **ASCII uppercase** (what **`LIST-VARS`** shows). The value is the remaining tokens joined with single spaces; **`SET`** *name* alone sets an empty string. Quoted/escaped input participates in tokenization before this join step. Variable names must be non-empty. Targets **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** are rejected (**`Read-only system variable`**). Value tokens may include those **`@`** names; they expand from the session when used as operands (not as the **`SET`** target name). |
| **`SET PAGE-LENGTH`** *n* | Typed system setting — configures the ENGLISH report formatter's lines per page (M8 Stage 2). Requires a positive integer; bad input emits **`SET PAGE-LENGTH requires a positive integer`** and leaves the previous value. Default = **24**, resets on **`LOGOFF`**. Read via **`GET PAGE-LENGTH`** (prints the integer). Not stored in the env map, so it does **not** appear in **`LIST-VARS`**. See [ENGLISH formatting](english-formatting.md). |
| **`GET`** *name* | Print the variable’s value and a newline. **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** read from the session (not from the shell variable map). Other names use the shell map; if unset: **`No such variable:`** *name* (as you typed it). |
| **`LIST-VARS`** | List shell variable names in **ASCII uppercase** (sorted), each on its own line under a **`Variables:`** header. When logged in with a Gemini session, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@USERNO`** are included once (they are not stored as ordinary shell variables). When **`MD,DEFDATA`** defines a default data file, **`@DEFDATA`** is included once. If none: **`No variables`**. Takes no arguments. |
| **`UNSET`** *name* | Remove *name* from the shell map. The four session **`@`** names cannot be unset (**`Read-only system variable`**). If it was not set: **`No such variable`**. |
| **`BASIC`** [*name*] | Enter BASIC mode. See [BASIC shell](basic-shell.md) for BASIC/ED commands and persistence rules. |
| **`ASM`** [*programName*] | Enter ASM mode (instruction-level debugger shell). Optional name immediately executes `RUN <programName>` after entering ASM. See [Assembler shell](assembler-shell.md). |
| **`RUN`** *programName* | Run by program name only (no extension). Tcl resolves `(file,key)` via VOC, prunes invalid breakpoints, and executes object record `<key>_OBJ`. If object is missing, Tcl compiles source record `<key>` and writes `<key>_OBJ` in the same resolved file before running. |
| **`PROC`** *programName* [*args...*] | Resolve script via VOC/filesystem (`F/Q/D/A` with fallback to `PROC` file), then execute with the host PROC interpreter. Positional args map to `P1`, `P2`, ... . Full language semantics: [PROC language](proc.md). |
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
| **`RESOLVE-FIELD`** *data-file* *token* | ENGLISH/DICT diagnostic: prints `DICT-<data-file>` naming, field kind (A/F/I or unknown), F/I definitions, **`Validity: INVALID`** and parse errors for broken DICT rows, resolved attribute index when applicable, and `D` / `MD` / `MC` hint. See [DICT dictionary items](dict.md). |
| **`DEFINE-FIELD`** *dict-file* *field-name* *attribute-number* | Non–R83 helper: writes a minimal type-**`A`** item into **`DICT`** / **`DICT-<file>`** (four Tcl tokens total). **`dict-file`** must exist (**`CREATE-FILE`** first). Example: **`DEFINE-FIELD DICT NAME 1`**. F/I items require **`EDIT DICT`** — see [DICT dictionary items](dict.md). |
| **`LIST-DICT`** *dict-file* | Deterministic DICT listing. Output is one line per item, sorted by item-id: `<item-id> <type> VALID\|INVALID`. `<type>` is `A`, `S`, `F`, `I`, or `INVALID`. Requires the dictionary logical file to exist. |
| **`CREATE-VOC`** *item-id* *type* *target...* | Create/update a `VOC` record. `type` must be one of `A/D/F/Q/V/X` (case-insensitive input, canonical uppercase stored). The record is written in canonical attribute-per-line format (`<type>` then one line per target token), and resolver cache is invalidated. |
| **`DELETE-VOC`** *item-id* | Remove one `VOC` entry by key and invalidate resolver cache. Missing entries return a filesystem `Error: ...` line (same style as other file-backed Tcl commands). |
| **`LIST-VOC`** | Deterministic VOC listing. Output is one line per item, sorted by item-id: `<item-id> <type>`. `<type>` is canonical single-letter type; malformed or unknown first attributes are reported as `INVALID`. Takes no arguments. |
| **`READ`** *file* *record-name* \| **`READ`** *record-name* | Print record value if present; if missing record: **`No such record`**. Two-token form is allowed only when **`MD,DEFDATA`** sets a default logical file (see [Gemini bootstrap](gemini-bootstrap.md)). When logged in, fails with **`Error: RECORD LOCKED: …`** if another session holds a lock on the record. |
| **`READU`** *file* *record-name* \| **`READU`** *record-name* | Acquire a **READU** lock then read (same output as **`READ`**). Same **`MD,DEFDATA`** shorthand as **`READ`**. |
| **`WRITE`** *file* *record-name* *value…* \| **`WRITE`** *record-name* *value* | Upsert record value. Value is remaining tokens joined with single spaces. Two-token value form (**`WRITE`** *record* *value*) is allowed only when **`MD,DEFDATA`** is set and *value* is a **single** token; use the full three-argument form for multi-word values. When logged in, fails with **`Error: RECORD LOCKED: …`** if another session holds the lock. |
| **`WRITEU`** *file* *record-name* *value…* \| **`WRITEU`** *record-name* *value* | Acquire a **WRITEU** lock then write (same behaviour as **`WRITE`**). Same **`MD,DEFDATA`** shorthand as **`WRITE`**. |
| **`RELEASE`** *file* *record-name* \| **`RELEASE`** *record-name* | Release a record lock held by this session. Silent when no lock is held. Same **`MD,DEFDATA`** shorthand as **`READ`**. |
| **`EDIT`** *file* *record-name* \| **`EDIT`** *programName* | Open a **blocking** line-oriented record editor (prompt **`ED> `**) on an in-memory copy of the record. Shorthand *programName* resolves the **source** record `(file, key)` via VOC (same as `BASIC` / `RUN` source), never `_OBJ`. Missing record starts with an empty buffer. See [TCL EDIT](#tcl-edit-line-record-editor) below. |
| **`DUMP-STACK`** | Print the VM stack (uses the command output stream). |
| **`LOGTO`** *account* | After catalogue login: switch Pick root to another account from **`ACCOUNTS.json`** (requires **`VOC/`** under the resolved path). |
| **`LOGOFF`** | After catalogue login: clear session and return control to **`main`** so the host can run the core **LOGON** phase again (not an inner Tcl loop). |

Unknown first token: **`Unknown command: …`**.

### ENGLISH report formatting

`LIST`, `SORT`, `SELECT`, and `COUNT` accept optional formatting clauses on the same line as the query (see [ENGLISH formatting](english-formatting.md)):

| Clause | Summary |
| --- | --- |
| **`HEADING "text"`** | First output line; re-printed at each page break when paginated. `@DATE`, `@TIME`, `@PAGE`, `@<n>`, `@@`. |
| **`FOOTING "text"`** | Footer once at end of report without `HEADING`; per-page footer when `HEADING` + pagination. Same `@` tokens. |
| **`BREAK-ON`** *field* | 79-hyphen break line when the field value changes. |
| **`TOTAL`** *field* | `TOTAL field: value` at break boundaries and end of report. |
| **`ID-SUPP`** | Data rows omit record ids (fields only). |

Page length defaults to **24** lines and is session-scoped: **`SET PAGE-LENGTH`** *n* (positive integer), **`GET PAGE-LENGTH`**, resets on **`LOGOFF`**. Pagination applies only when a **`HEADING`** is present.

Example:

```text
SET PAGE-LENGTH 30
LIST CUSTOMERS NAME CITY HEADING "Customer list — @DATE" FOOTING "Page @PAGE — confidential"
```

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

`PROC` runs host-interpreted scripts (no VM bytecode) via `PROC <programName> [args...]`. Script lookup and Tcl-bridge behavior are documented here; the full PROC language contract (execution model, substitution rules, statements, and error behavior) lives in [PROC language](proc.md). Native PROC lock statements report conflicts via **`PROCERR = ?LOCKED?`**; bridged **`TCL READU`** uses Tcl-style **`RECORD LOCKED`** errors instead (see [Concurrency](concurrency.md)).

## Shell variables

Variables are **string key/value** pairs held by the shell (not the VM stack). Names are **case-insensitive**; the shell stores and lists them in **ASCII uppercase** (see **`LIST-VARS`**). Input tokenization supports quoted strings and escapes, and command handlers that join remaining tokens (for example `SET` and `WRITE`) still normalize joins to single spaces.

### Session **`@`** names (read-only)

**`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and **`@DEFDATA`** are **session fields**, not entries in the Tcl environment map. They follow the same canonicalization as other names (ASCII uppercase for letters). Catalogue identity values come from logon (**`LOGTO`** updates the session; **`LOGOFF`** clears them; **`QUIT`** clears shell variables but leaves the Pick root and **`@DEFDATA`** until the root changes). **`@DEFDATA`** is the logical file name from **`MD,DEFDATA`** when that record is valid, otherwise empty. For Tcl commands, operand tokens (all arguments after the verb except the variable name slot on **`SET`** / **`GET`** / **`UNSET`**, and the program name on **`RUN`** / **`PROC`**) that are exactly those four names expand to their current string values; other **`@FOO`** tokens are left unchanged. **`GET`** reads the session fields directly (when not logged in, **`@ACCOUNT`** / **`@LOGNAME`** are typically empty and **`@USERNO`** defaults to **`0`**). The active list (**IDs plus the logical `activeListSourceFile` from `SELECT`**) is session-scoped state: **`LIST-LIST`**, **`CLEAR-LIST`**, **`LOGOFF`**, **`QUIT`** (session reset), and changing the Tcl **filesystem root** clear it ([ENGLISH](english.md)).

**`ECHO`:** each argument token is scanned for **`$$`** (escaped dollar), **`$Name`** substitution, and literal **`$`**. There is **no** expression evaluation. Unset **`$Name`** expands to an empty segment.

Arity errors: **`SET`** / **`GET`** / **`UNSET`** without a name print a **`… requires a variable name`** line. Extra tokens on **`LIST-VARS`** print **`LIST-VARS takes no arguments`**. **`HELP`** accepts zero or more topic operands (**`HELP`** *topic…*). **`HELP-LIST`**/**`HELP-EDIT`** follow the usual arity diagnostics (`HELP-LIST takes no arguments`, `HELP-EDIT requires a topic`, …). Filesystem arity errors follow the same pattern (for example, **`LIST requires a filename`**, **`READ`** / **`WRITE`** messages that include the optional **`MD DEFDATA`** shorthand when the default is not set).

Instruction-level debugger controls (`STEP`, `CONT`, `TRACE`, breakpoint management, and program/label dumps) now live in [Assembler shell](assembler-shell.md).

## Errors

Parse/runtime failures during **`RUN`** print **`Error:`** followed by the exception message. Filesystem command failures also print **`Error:`** with a specific reason (for example missing file or invalid record/file name). Login/bootstrap diagnostics now distinguish missing versus malformed catalogue states (`ACCOUNTS.json not found` vs `Invalid ACCOUNTS.json`) and surface account-root/VOC/MD attachment status in boot diagnostics. The VM’s output stream is cleared back to default after **`RUN`** attempts.

## See also

- [HELP system](help-system.md) — file-backed HELP topics, lookup chain, `HELP-LIST` / `HELP-EDIT`.
- [Bytecode VM](vm.md) — opcode reference and parser/runtime details.
- [Concurrency and record locking](concurrency.md) — session lock table and Tcl/BASIC/PROC behaviour.
- [Assembler shell](assembler-shell.md) — instruction-level debugger shell.
