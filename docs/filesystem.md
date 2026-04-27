# File system (current implementation)

The current file system is a lightweight, JSON-backed store used by Tcl commands (`CREATE-FILE`, `DELETE-FILE`, `LIST-FILES`, `LIST`, `READ`, `WRITE`) and by BASIC file statements at runtime.

It is intentionally simple and is not yet a full Pick account/file dictionary implementation.

## Root directory

- Default root directory: `filesystem` (relative to process working directory).
- The root can be changed in code via `Shell::setFileSystemRoot(...)`.
- The root directory is created on demand by write-side operations.

## Storage model

Each logical file is stored as a single JSON file named:

- `<fileName>.json`

Example:

```json
{
  "name": "BP",
  "records": {
    "ID1": "first value",
    "ID2": "second value"
  }
}
```

`name` must match the logical filename used to open/read/write/list that file.

## Name validation rules

File names and record names use the same validation:

- Non-empty.
- Allowed characters only: ASCII letters, digits, `_`, `-`.
- No spaces, dots, slashes, or other punctuation.

Invalid names fail with `Error: ...` messages from the shell.

## Tcl command behavior

### `CREATE-FILE <name>`

- Creates `<name>.json` with empty `records`.
- Fails if file already exists.
- Extra arguments are currently ignored.

### `DELETE-FILE <name>`

- Deletes `<name>.json`.
- Fails if file does not exist.
- Extra arguments are currently ignored.

### `LIST-FILES`

- Lists logical file names, sorted.
- Output is:
  - `No files` when empty.
  - Otherwise:
    - `Files:`
    - one name per indented line.
- Takes no arguments.

### `LIST <file>`

- Lists record names for `<file>`, sorted.
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
- `Error: Invalid JSON in <path>: <reason>`
- `Error: File name mismatch in <path>`

## JSON parsing/validation rules

On read/list operations, JSON files are parsed and validated strictly:

- Root must be an object.
- Only `name` and `records` fields are allowed.
- Both fields are required.
- `records` must be an object of string keys to string values.
- Duplicate `name` or `records` fields are rejected.
- Trailing content after the JSON object is rejected.

If a file is malformed, commands fail with `Error: Invalid JSON in ...`.

## Ordering and persistence notes

- `LIST-FILES` sorts names lexicographically.
- `LIST <file>` sorts record names lexicographically.
- `WRITE` persists via temp-file + rename (`.tmp`) to reduce partial-write risk.
- Reads and writes are case-sensitive with no normalization.

## Scope note

This is the implemented behavior today. It models a practical JSON-backed record store for shell and BASIC features, not a full historical Pick filesystem/account model.
