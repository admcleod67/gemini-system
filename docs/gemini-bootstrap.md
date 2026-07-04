# Gemini bootstrap layout (development)

This tree supports catalogue-backed **account bootstrap** (Milestone **2** onward): a host-backed account layout and JSON catalogues used for **interactive login**, **`LOGTO`**, and **`LOGOFF`**. It also provides an optional **default Pick filesystem root** for `gemini-system` (including **`SYSPROG/VOC`** entries used by ENGLISH/Tcl).

## Relationship to the Pick filesystem root

The transitional backend uses one **host directory** as the Pick filesystem root (see [File system](filesystem.md)). Logical files (`VOC`, `BP`, `MD`, …) are subdirectories of that root.

- **Legacy default:** `filesystem/` (relative to the process **current working directory**).
- **Bootstrap account root:** `gemini/accounts/SYSPROG/` — same role: it **is** the Pick root when selected, not a subdirectory inside `filesystem/`.

So `VOC` lives at `<Pick-root>/VOC/` (a directory of `.item` records), not at `filesystem/gemini/...`.

## Catalogue root vs Pick root

- **Pick filesystem root** — where logical `VOC`, `BP`, … live (`GeminiSession::setFileSystemRoot` / `Shell::setFileSystemRoot`).
- **Gemini catalogue root** — the directory that contains **`ACCOUNTS.json`** (and optionally **`USERS.json`** for future or reporting use; it is **not** required for console logon). **`PickCore::LoginService`** (in **`gemini-core`**) reads **`ACCOUNTS.json`** for authentication; the Tcl shell only runs **after** a successful login has produced a **`PickCore::UserSession`**. The catalogue directory is **not** the same path as the Pick root.

When the bootstrap marker `cwd/gemini/accounts/SYSPROG/VOC` is present, `applyDefaultFileSystemRoot` sets **both** the Pick root to `cwd/gemini/accounts/SYSPROG` and the catalogue root to **`cwd/gemini`**.

If you set only **`GEMINI_FILESYSTEM_ROOT`**, you must also set **`GEMINI_CATALOG_ROOT`** to the directory containing the JSON catalogues so login can run.

## Directory layout (repository)

Paths are relative to the repository (or to a copy placed next to the executable; see below).

| Path | Role |
|------|------|
| `gemini/ACCOUNTS.json` | Account catalogue (host JSON). |
| `gemini/USERS.json` | User catalogue (host JSON). |
| `gemini/accounts/SYSPROG/` | Pick filesystem root for the **SYSPROG** account: `VOC/`, `MD/`, `BP/`, `PROC/`, **`HELP/`** (file-backed HELP topics; see [HELP system](help-system.md)). |

`MD/`, `BP/`, and `PROC/` may be empty except for placeholder files so Git tracks the directories.

### `MD/AUTO-LOGON` (optional)

If the bootstrap Pick root contains logical file **`MD`** with record **`AUTO-LOGON`** (`.item` body: first line = account name), **`main`** attempts account logon from that line **before** environment auto-logon and before the interactive **`LOGON PLEASE:`** loop. Missing **`MD`** or record is ignored (no error).

### `MD/DEFDATA` (optional)

If logical file **`MD`** contains record **`DEFDATA`**, the first non-empty line is treated as a **logical Pick file name** (same character rules as other logical files). After logon (or whenever the Pick root changes), the Tcl shell loads it into session state as read-only **`@DEFDATA`** and uses it for optional **`READ`** / **`WRITE`** shorthand (see [Developer shell](tcl-shell.md)). Missing record or invalid name is ignored.

## `ACCOUNTS.json` schema

Top-level object:

- **`accounts`** (array): each element:
  - **`name`** (string): account id (e.g. `SYSPROG`).
  - **`root`** (string): path to the account’s Pick root, **relative to the catalogue directory** (the parent of `ACCOUNTS.json`). Example: `accounts/SYSPROG` resolves to `gemini/accounts/SYSPROG/`.
  - **`passwordHash`** (optional string): if omitted, no password is required for that account at logon. If present and the value begins with **`dev-`**, the interactive logon phase does **not** read a password line for that account. Otherwise one additional line is read **without a `Password:` banner** and compared as plain text (development-only; not secure for production).

## `USERS.json` schema

Optional file (same shape as before). If present and parses, the cold-start **`PickCore::BootMonitor`** prints **`USERS ATTACHED`**. It is **not** used for the account-only console logon path today.

## Cold start (`PickCore::BootMonitor`)

After `Runtime` and default roots are prepared, **`PickCore::BootMonitor::runColdStart`** prints a short milestone sequence (version line, init, memory line as **`(host n/a)`** until a real policy exists, filesystem / **`MD`** / catalogue probes, **`PORT MANAGER: READY`** when the daemon supplies a port manager, **`SYSTEM READY`**) **followed by one blank line**. Each milestone line reflects an actual check where possible; see [`src/core/boot/BootMonitor.cpp`](../src/core/boot/BootMonitor.cpp).

## Interactive login (account-only, core boot stage)

When a **catalogue root** is configured, **`gemini-system`** calls **`PickCore::LoginService::runCatalogLogin`** after **`SYSTEM READY`** and **before** the Tcl REPL. The boot monitor never starts Tcl — **`main`** does, only after **`runCatalogLogin`** returns a **`UserSession`**.

### Login → Tcl **`stdout`** contract (Pick-style cadence)

Target layout (cold start after **`SYSTEM READY`** + blank line):

```text
LOGON PLEASE: SYSPROG

Gemini/TCL Developer Shell
```

- **Interactive**: **`LOGON PLEASE: `** is flushed with **no newline**; the operator completes the **same programmatic line**. **`getline`** reads the token (the terminal handles Return separately). Authentication may read a silent password line. On success, login emits **exactly one** flushed newline — one **blank programmatic line** before the Tcl banner. **`Shell::runTclRepl()`** writes **no** leading newline (`Gemini/TCL Developer Shell`).
- **`MD`/env auto-logon** (cold start **only**): there is **no keyed Return** after the echoed account — **`stdout`** prints **`LOGON PLEASE: `**, the resolved account identifier, then **`std::endl`** (terminates the logon echo line — same outcome as pressing Return after typing the account). Optional silent password read, then **`authenticateAccount`**. On success, login emits the **same** single flushed newline as interactive (another blank programmatic line **before** the Tcl banner).

Programmatic **`stdout`** for auto-login is therefore **`LOGON PLEASE: `** plus the resolved account, **`'\n'`** from **`std::endl`**, then **`'\n'`** on success (**two** `\n`) before **`Gemini/…`**; interactive login still puts only **one** programmatic **`'\n'`** on success (**`println`** boundary), matching the terminal because the typed account and Return came from **`stdin`**, not from **`stdout`**.

1. **Port cold start only** (`CatalogLoginPhase::ColdStartPortInit`): **`MD,AUTO-LOGON`** then **`GEMINI_AUTO_LOGON`** / deprecated **`GEMINI_AUTO_LOGIN`** are evaluated exactly once after boot. Outputs **`LOGON PLEASE:`** plus resolved account **`endl`**; optional silent password read; **`authenticateAccount`** path; success uses the **same** Tcl boundary (**`println`** = one flushed newline) as interactive (**`authenticateAccount`** + **`printlnLoginToTclBoundary`** in code). If authentication fails, prints **`Login incorrect`** (R83 / UniData style) and falls through to interactive logon.
2. **After `LOGOFF`** (`CatalogLoginPhase::InteractiveOnly`), **`runCatalogLogin` does not** read **`AUTO-LOGON`** or environment auto-logon variables — same interactive **`LOGON PLEASE: `** behaviour only.
3. **Interactive** (no `Username:` helper): **`LOGON PLEASE: `** is printed with **no** newline; the operator types the **single account id** token and Return (same name rules as Pick file names; reserved words rejected). Any failed attempt prints **`Login incorrect`** and re-prompts. **Not** Tcl.
4. On success, **`GeminiSession::attach()`** applies **`PickCore::UserSession`**: Pick root for that account, VM/Tcl reset, **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**. (`Shell::attachUserSession` delegates to the session.) For account-only logon, **`username`** and **`accountName`** are the same string until a distinct user model exists.
5. **EOF / Ctrl-D** during the login phase exits the host (no `QUIT` keyword in login phase).

**`LOGTO account`** and **`LOGOFF`** remain **Tcl commands** after login. **`LOGOFF`** clears the session and **`runTclRepl()`** returns to **`main`**, which repeats **`runCatalogLogin`** in **`InteractiveOnly`** phase (no **`MD`** / env auto-logon).

**`WHO`** prints `port username account` when logged in (port is `0` for now). With account-only logon, **`username`** and **`account`** match. When not logged in (e.g. tests without a catalogue), **`WHO`** prints **`0 - -`**.

**Breaking change:** logon is **account-based** (`ACCOUNTS.json` only). The previous **`USERS.json` username + defaultAccount** logon path has been removed.

## `GEMINI_AUTO_LOGON` and `GEMINI_AUTO_LOGIN`

These variables apply **only** on the **first** catalogue **`runCatalogLogin`** after process start (**port initialisation / cold start**), not after **`LOGOFF`**.

If **`GEMINI_AUTO_LOGON`** is set to an **account name** and a catalogue root is configured, **`runCatalogLogin`** uses it on cold start whenever **`MD,AUTO-LOGON`** does not yield a valid account (with **`stderr`** on failure **after** the visible **`LOGON PLEASE: …`** line, when auth fails). **`GEMINI_AUTO_LOGIN`** is accepted as a **deprecated alias** (same value: account name).

The **smoke** test sets **`GEMINI_AUTO_LOGON=SYSPROG`** so `echo QUIT | gemini-system` still works when the bootstrap tree is present (and cold start may also succeed via committed **`MD/AUTO-LOGON`** alone when the copied tree is used).

## Default filesystem root in executables

`gemini-system` calls `applyDefaultFileSystemRoot` (which uses **`PickCore::resolveDefaultHostPaths`**) before **`BootMonitor`** and the login / REPL loop. Pick root resolution order:

1. If **`GEMINI_FILESYSTEM_ROOT`** is set and non-empty, its value is used as the Pick filesystem root. If **`GEMINI_CATALOG_ROOT`** is also set, it becomes the catalogue root.
2. Else if **`gemini/accounts/SYSPROG/VOC`** exists relative to **cwd**, the Pick root is **`gemini/accounts/SYSPROG`** and the catalogue root is **`gemini/`** (relative to cwd).
3. Otherwise the shell keeps the built-in default (`filesystem/` relative to cwd) and **no catalogue root** — **no login phase**, Tcl starts immediately (used by most unit tests).

## CMake copy

Building `gemini-system` triggers a **`gemini-bootstrap-copy`** step that copies the entire `gemini/` tree from the source tree into **`CMAKE_CURRENT_BINARY_DIR`/gemini** (typically `<build>/src/gemini` when using the project’s `src/CMakeLists.txt`). That keeps `gemini/` next to the built binaries when the run **working directory** is set to that same directory (e.g. CLion default for targets defined under `src/`). The build also copies language modules (**`libgemini-module-basic`**, **`-pascal`**, **`-comal`**, **`-cobol`**, **`-stub`**, or platform equivalents) into **`gemini/modules/`** under that same tree.

## Language modules (Milestone 11)

At cold start, **`BootMonitor`** scans **`gemini/modules/`** for shared libraries (`.dylib` / `.so` / `.dll`) and loads each via **`register_language`**. Override the directory with **`GEMINI_MODULES_PATH`**. When a catalogue root is configured, the default modules path is **`<catalogue-root>/modules`** (i.e. `gemini/modules/` next to `ACCOUNTS.json`). Failed modules are logged during boot and do not abort startup.

After loading, boot emits **`MODULES: N loaded, M failed (K attempted)`**. Inspect registered namespaces and retained boot failures at the Tcl prompt with **`SYSTEM LANGUAGES`** or **`SHOW-MODULES`** (see [Developer shell](tcl-shell.md)).

See **[Language module ABI](language-modules.md)** and **[Bytecode contract](bytecode.md)** for the module entry point, namespace IDs, and **`CALL_FUNC`** encoding.

## VOC bootstrap records

Logical file `VOC` holds dictionary records as `.item` files (attribute-per-line). The committed fixture includes at least:

- **`BP`**: `F`-type pointer to file `BP` (program fallback behaviour matches tests).
- **`ED`**: `V`-type alias to **`EDIT`** (interactive Tcl and PROC **`TCL`** both resolve the first token through **`VOC`** via `resolveVerbName`).
- **`WHO`**, **`LOGIN`**, **`LOGTO`**, **`LOGOFF`**: `V`-type stubs in VOC; **`LOGTO`** / **`LOGOFF`** are implemented as Tcl builtins when logged in. Other dictionary names can map to those builtins with matching **`V`** entries.

## Mutable data

If you run with the repository’s `gemini/` as the active Pick root, **`WRITE` / `CREATE-FILE` can modify tracked files**. Prefer the CMake-copied tree under the build directory, a temp copy, or set **`GEMINI_FILESYSTEM_ROOT`** to a writable clone for experiments.

## JSON dependency

The host catalogues are parsed with **nlohmann/json**, declared once at the **top-level** `CMakeLists.txt` via `FetchContent` and linked from **`gemini-core`** (shared by **`gemini-tcl`** through the core library).
