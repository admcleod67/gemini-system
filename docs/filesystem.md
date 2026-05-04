# File system (host backend transition)

The filesystem layer provides a Pick-facing abstraction over storage for Tcl and BASIC runtime operations. During the transition phase it uses the host filesystem as backend storage.

This keeps shell/runtime semantics stable while allowing the backend to be replaced later by a true Pick hashed file implementation.

## Root directory

- Default root directory: `filesystem` (relative to process working directory).
- The root can be changed via `Shell::setFileSystemRoot(...)`.
- The root directory is created on-demand by write-side operations.
- Optional **bootstrap account** layout and default-root behaviour for executables are documented in [Gemini bootstrap](gemini-bootstrap.md).

## Transitional storage model

- Each logical Pick file is represented as a **directory**:
  - `<root>/<fileName>/`
- Each record is represented as a **file** inside that directory.
- Logical item-id = filename stem (extension hidden from Tcl).

## Filesystem API shape

The backend supports a handle-oriented flow:

- `openFile(name) -> FileHandle`
- `listRecords(handle)`
- `readRecord(handle, id)`
- `writeRecord(handle, id, record)`
- `deleteRecord(handle, id)`
- `createFile(name)`
- `deleteFile(name)`

Compatibility wrappers still expose current callers to:

- `read(fileName, recordName)`
- `write(fileName, record)`
- `listRecordNames(fileName)`

so Tcl commands and VM callbacks do not need backend-specific knowledge.

## Record extension policy

Extensions are backend-only and are not exposed to Tcl:

- `.bas` for BASIC-source style records (logical file `BP`)
- `.proc` for PROC-style records (logical files `PROC` or `PROCS`)
- `.item` default for generic Pick records

`LIST` returns logical item-ids (stems), not extension-bearing names.

## `.item` canonical content format

`.item` files represent a single Pick record in transitional storage:

- Plain text only.
- One attribute per line, in order.
- Newline is the attribute boundary.
- Empty attributes are represented by empty lines.
- No schema, typing, or structural metadata is stored.

This is forward-compatible with future import into true Pick storage by mapping newline-separated attributes to attribute marks (`0xFE`) without transformation.

## Minimal VOC support

Program-name resolution for `BASIC`, `RUN`, and `LIST-PROGRAMS` is VOC-backed:

- Logical file `VOC` is read as `.item` records.
- Supported entry types:
  - `F`, `D`, `A`: attribute 2 = logical file name (`D`/`A` are file-pointer aliases for resolution, not full Pick data-dictionary definitions).
  - `Q`: attribute 2 = synonym target key (recursive, cycle-protected).
  - `V`, `X`: attribute 2 = verb target for Tcl (`X` is alias-only; no execution of VOC-stored TCL). Verb chains: see [Developer shell (TCL)](tcl-shell.md#program-resolution-voc-backed).
- Lookup is case-insensitive.
- Program location fallback when no explicit entry resolves: `(BP, <program-name>)`.
- PROC script lookup uses the same VOC table but applies PROC-specific fallback `(PROC, <script-name>)` for unresolved script keys.

For this milestone, object code records are stored in the same resolved Pick file as source using key suffix `_OBJ`.

Account-level **default data file** (logical **`MD`** record **`DEFDATA`**) is documented in [Gemini bootstrap](gemini-bootstrap.md) and used by the Tcl shell for **`@DEFDATA`** and optional **`READ`** / **`WRITE`** shorthand; it is not part of the VOC program-resolution table.

## Name validation rules

File names and record names use the same validation:

- Non-empty.
- Allowed characters only: ASCII letters, digits, `_`, `-`.
- No spaces, dots, slashes, or other punctuation.

Invalid names fail with `Error: ...` messages from the shell.

## Tcl command behavior

### `CREATE-FILE <name>`

- Creates `<root>/<name>/`.
- Fails if file already exists.
- Extra arguments are currently ignored.

### `DELETE-FILE <name>`

- Deletes `<root>/<name>/` recursively.
- Fails if file does not exist.
- Extra arguments are currently ignored.

### `LIST-FILES`

- Lists logical file names (directory names), sorted.
- Output is:
  - `No files` when empty.
  - Otherwise:
    - `Files:`
    - one name per indented line.
- Takes no arguments.

### `LIST <file>`

- Lists record ids for `<file>`, sorted.
- Output is:
  - `No records` when empty.
  - Otherwise:
    - `Records:`
    - one record name per indented line.
- Requires exactly one filename argument.

### `READ <file> <record>`

- Prints record value followed by newline when present.
- Prints `No such record` when record is absent.
- Requires at least `<file> <record>`; extra arguments are currently ignored.

### `WRITE <file> <record> <value...>`

- Upserts record value in `<file>`.
- `<value...>` is the remaining tokens joined by single spaces.
- Requires at least three arguments after command name.

## Error behavior

Shell command handlers print filesystem failures as:

- `Error: <message>`

Representative messages include:

- `Error: Invalid file name`
- `Error: File already exists: <name>`
- `Error: File not found: <name>`
- `Error: Invalid record name`

## Ordering and persistence notes

- `LIST-FILES` sorts names lexicographically.
- `LIST <file>` sorts record names lexicographically.
- Writes persist via temp-file + rename (`.tmp`) to reduce partial-write risk.
- Reads and writes are case-sensitive with no normalization.
