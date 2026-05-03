# Gemini bootstrap layout (development)

This tree supports **Milestone 2** work: a host-backed account layout and catalogue files that future `LOGIN` / `LOGTO` will consume. It also provides an optional **default Pick filesystem root** for `gemini-system` and `gemini-cli`.

## Relationship to the Pick filesystem root

The transitional backend uses one **host directory** as the Pick filesystem root (see [File system](filesystem.md)). Logical files (`VOC`, `BP`, `MD`, …) are subdirectories of that root.

- **Legacy default:** `filesystem/` (relative to the process **current working directory**).
- **Bootstrap account root:** `gemini/accounts/SYSPROG/` — same role: it **is** the Pick root when selected, not a subdirectory inside `filesystem/`.

So `VOC` lives at `<Pick-root>/VOC/` (a directory of `.item` records), not at `filesystem/gemini/...`.

## Directory layout (repository)

Paths are relative to the repository (or to a copy placed next to the executable; see below).

| Path | Role |
|------|------|
| `gemini/ACCOUNTS.json` | Account catalogue (host JSON). |
| `gemini/USERS.json` | User catalogue (host JSON). |
| `gemini/accounts/SYSPROG/` | Pick filesystem root for the **SYSPROG** account: `VOC/`, `MD/`, `BP/`, `PROC/`. |

`MD/`, `BP/`, and `PROC/` may be empty except for placeholder files so Git tracks the directories.

## `ACCOUNTS.json` schema

Top-level object:

- **`accounts`** (array): each element:
  - **`name`** (string): account id (e.g. `SYSPROG`).
  - **`root`** (string): path to the account’s Pick root, **relative to the `gemini/` directory** (the parent of `ACCOUNTS.json`). Example: `accounts/SYSPROG` resolves to `gemini/accounts/SYSPROG/`.

## `USERS.json` schema

Top-level object:

- **`users`** (array): each element:
  - **`username`** (string): login id.
  - **`passwordHash`** (string): placeholder or real hash; not validated until `LOGIN` exists.
  - **`defaultAccount`** (string): must match an `accounts[].name`.
  - **`privileges`** (string): reserved for future use; may be empty.

## Default filesystem root in executables

`gemini-system` and `gemini-cli` call `applyDefaultFileSystemRoot` before the REPL runs. Resolution order:

1. If **`GEMINI_FILESYSTEM_ROOT`** is set and non-empty, its value is used as the Pick filesystem root (absolute or relative to **cwd**).
2. Else if **`gemini/accounts/SYSPROG/VOC`** exists relative to **cwd**, the Pick root is **`gemini/accounts/SYSPROG`** (relative to cwd).
3. Otherwise the shell keeps the built-in default (`filesystem/` relative to cwd).

So the marker path is **`cwd/gemini/accounts/SYSPROG/VOC`**, not under `filesystem/`. IDE run directories should be the same folder that contains either `filesystem/` (old flow) or `gemini/` (bootstrap flow).

## CMake copy

Building `gemini-system` or `gemini-cli` triggers a **`gemini-bootstrap-copy`** step that copies the entire `gemini/` tree from the source tree into **`CMAKE_CURRENT_BINARY_DIR`/gemini** (typically `<build>/src/gemini` when using the project’s `src/CMakeLists.txt`). That keeps `gemini/` next to the built binaries when the run **working directory** is set to that same directory (e.g. CLion default for targets defined under `src/`).

## VOC bootstrap records

Logical file `VOC` holds dictionary records as `.item` files (attribute-per-line). The committed fixture includes at least:

- **`BP`**: `F`-type pointer to file `BP` (program fallback behaviour matches tests).
- **`ED`**: `V`-type alias to **`EDIT`** (there is no `ED` Tcl verb; PROC TCL uses `resolveVerbName`).
- **`WHO`**, **`LOGIN`**, **`LOGTO`**, **`LOGOFF`**: `V`-type stubs for future work; top-level Tcl still dispatches builtins without VOC until Milestone 2 completes VOC-backed verbs.

## Mutable data

If you run with the repository’s `gemini/` as the active Pick root, **`WRITE` / `CREATE-FILE` can modify tracked files**. Prefer the CMake-copied tree under the build directory, a temp copy, or set **`GEMINI_FILESYSTEM_ROOT`** to a writable clone for experiments.
