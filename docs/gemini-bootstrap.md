# Gemini bootstrap layout (development)

This tree supports **Milestone 2** work: a host-backed account layout and catalogue files used for **interactive login**, **`LOGTO`**, and **`LOGOFF`**. It also provides an optional **default Pick filesystem root** for `gemini-system` and `gemini-cli`.

## Relationship to the Pick filesystem root

The transitional backend uses one **host directory** as the Pick filesystem root (see [File system](filesystem.md)). Logical files (`VOC`, `BP`, `MD`, …) are subdirectories of that root.

- **Legacy default:** `filesystem/` (relative to the process **current working directory**).
- **Bootstrap account root:** `gemini/accounts/SYSPROG/` — same role: it **is** the Pick root when selected, not a subdirectory inside `filesystem/`.

So `VOC` lives at `<Pick-root>/VOC/` (a directory of `.item` records), not at `filesystem/gemini/...`.

## Catalogue root vs Pick root

- **Pick filesystem root** — where logical `VOC`, `BP`, … live (`setFileSystemRoot` / `ShellSession`).
- **Gemini catalogue root** — the directory that contains **`ACCOUNTS.json`** and **`USERS.json`** (typically **`cwd/gemini`**). **`PickCore::LoginService`** (in **`gemini-core`**) reads these files; the Tcl shell only runs **after** a successful login has produced a **`PickCore::UserSession`**. The catalogue directory is **not** the same path as the Pick root.

When the bootstrap marker `cwd/gemini/accounts/SYSPROG/VOC` is present, `applyDefaultFileSystemRoot` sets **both** the Pick root to `cwd/gemini/accounts/SYSPROG` and the catalogue root to **`cwd/gemini`**.

If you set only **`GEMINI_FILESYSTEM_ROOT`**, you must also set **`GEMINI_CATALOG_ROOT`** to the directory containing the JSON catalogues so login can run.

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
  - **`root`** (string): path to the account’s Pick root, **relative to the catalogue directory** (the parent of `ACCOUNTS.json`). Example: `accounts/SYSPROG` resolves to `gemini/accounts/SYSPROG/`.

## `USERS.json` schema

Top-level object:

- **`users`** (array): each element:
  - **`username`** (string): login id.
  - **`passwordHash`** (string): if this value begins with **`dev-`**, the interactive login phase **does not prompt for a password** (development placeholder). Otherwise the user must enter a password line, compared to this field as plain text (not secure; replace for real deployments).
  - **`defaultAccount`** (string): must match an `accounts[].name`.
  - **`privileges`** (string): reserved for future use; may be empty.

## Interactive login (R83-style, core boot stage)

When a **catalogue root** is configured, **`gemini-system`** / **`gemini-cli`** run **`PickCore::LoginService`** in the host **`main`** loop **before** the Tcl REPL:

1. The host prints **`LOGON PLEASE:`** and **`Enter your username (EOF to exit).`**, then a **`Username:`** prompt. This is **not** Tcl: the user must type **one username token only** (same character rules as Pick file names; reserved words such as `LOGIN`, `QUIT`, `HELP`, … are rejected).
2. If the user’s `passwordHash` does **not** start with `dev-`, a **`Password:`** prompt appears and one line is read as the password.
3. On success, **`PickShell::Shell::attachUserSession`** applies the resulting **`PickCore::UserSession`**: default account Pick root, VM/Tcl reset, **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, and only then the **`TCL>`** REPL runs via **`Shell::runTclRepl()`**.
4. **EOF / Ctrl-D** during the login phase exits the host (no `QUIT` keyword in login phase).

**`LOGTO account`** and **`LOGOFF`** remain **Tcl commands** after login. **`LOGOFF`** clears the session and **`runTclRepl()`** returns to **`main`**, which **re-enters** the core login service (same pattern a future port monitor would use when a channel is released).

**`WHO`** prints `port username account` when logged in (port is `0` for now). When not logged in (e.g. tests without a catalogue), **`WHO`** prints **`0 - -`**.

## `GEMINI_AUTO_LOGIN`

If **`GEMINI_AUTO_LOGIN`** is set to a **username** and a catalogue root is configured, **`main`** calls **`PickCore::LoginService::tryAutoLoginFromEnv`** (with **`stderr`** for failure messages) before the console login prompts—equivalent to a successful username/password flow for a `dev-` style hash. If that fails, the usual **`LOGON PLEASE:`** interactive phase runs. On each new login cycle after **`LOGOFF`**, **`tryAutoLoginFromEnv`** is attempted again when the variable is still set.

The **smoke** test sets **`GEMINI_AUTO_LOGIN=allan`** so `echo QUIT | gemini-system` still works when the bootstrap tree is present.

## Default filesystem root in executables

`gemini-system` and `gemini-cli` call `applyDefaultFileSystemRoot` (which uses **`PickCore::resolveDefaultHostPaths`**) before the login / REPL loop. Pick root resolution order:

1. If **`GEMINI_FILESYSTEM_ROOT`** is set and non-empty, its value is used as the Pick filesystem root. If **`GEMINI_CATALOG_ROOT`** is also set, it becomes the catalogue root.
2. Else if **`gemini/accounts/SYSPROG/VOC`** exists relative to **cwd**, the Pick root is **`gemini/accounts/SYSPROG`** and the catalogue root is **`gemini/`** (relative to cwd).
3. Otherwise the shell keeps the built-in default (`filesystem/` relative to cwd) and **no catalogue root** — **no login phase**, Tcl starts immediately (used by most unit tests).

## CMake copy

Building `gemini-system` or `gemini-cli` triggers a **`gemini-bootstrap-copy`** step that copies the entire `gemini/` tree from the source tree into **`CMAKE_CURRENT_BINARY_DIR`/gemini** (typically `<build>/src/gemini` when using the project’s `src/CMakeLists.txt`). That keeps `gemini/` next to the built binaries when the run **working directory** is set to that same directory (e.g. CLion default for targets defined under `src/`).

## VOC bootstrap records

Logical file `VOC` holds dictionary records as `.item` files (attribute-per-line). The committed fixture includes at least:

- **`BP`**: `F`-type pointer to file `BP` (program fallback behaviour matches tests).
- **`ED`**: `V`-type alias to **`EDIT`** (PROC TCL uses `resolveVerbName`).
- **`WHO`**, **`LOGIN`**, **`LOGTO`**, **`LOGOFF`**: `V`-type stubs in VOC; **`LOGTO`** / **`LOGOFF`** are implemented as Tcl builtins when logged in. Top-level Tcl does not resolve arbitrary verbs through VOC yet.

## Mutable data

If you run with the repository’s `gemini/` as the active Pick root, **`WRITE` / `CREATE-FILE` can modify tracked files**. Prefer the CMake-copied tree under the build directory, a temp copy, or set **`GEMINI_FILESYSTEM_ROOT`** to a writable clone for experiments.

## JSON dependency

The host catalogues are parsed with **nlohmann/json**, declared once at the **top-level** `CMakeLists.txt` via `FetchContent` and linked from **`gemini-core`** (shared by **`gemini-tcl`** through the core library).
