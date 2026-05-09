# File-backed HELP (Milestone 6)

HELP is implemented in the Tcl layer only. Topics are normal Pick **records** in a logical file named **`HELP`** under the current account Pick root. Record bodies are newline text (`.item` structured storage); HELP printing uses the raw body.

## Lookup order

1. **`HELP`** on the **current account** Pick filesystem.
2. **`HELP`** on **`SYSPROG`** — only when a Gemini **catalogue root** is configured **and** `accounts/SYSPROG` on that catalogue is **not** the same host path as the session Pick root (avoids double-read when already logged into SYSPROG).
3. **Built-in** topic strings — minimal bootstrap when no files exist or a topic is missing.

## Canonical topic keys

Operands after **`HELP`** are joined with ASCII spaces for the operator-facing **display** string (preserving typed casing for errors).

Lookup uses a **canonical key**: trim runs of whitespace, ASCII **uppercase**.

- **One operand:** the token is resolved as a VOC **verb** first (same as other Tcl verbs), then uppercased. Example: VOC maps **`HGET`** → **`GET`**, so **`HELP HGET`** shows help for **`GET`**.
- **Two or more operands:** no VOC pass on the full string; the joined string is trimmed and uppercased. Example: **`HELP HELP BASIC`** → canonical **`HELP BASIC`**.

Unknown topics:

```text
No help available for "<display>".
```

(newline-terminated, with the display join as typed).

### Record ids vs canonical keys (`HELP` logical file)

[`PickFS::Catalog::isValidName`](../src/core/filesystem/Catalog.cpp) permits only `[A-Za-z0-9_-]`. Spaces are **not** allowed in PickFS record ids, so canonical keys that contain spaces are stored using a single, deterministic encoding for this subsystem:

- **Storage record id:** every ASCII **space** in the canonical key is replaced with **`_`** (underscore). Example: **`HELP BASIC`** → record id **`HELP_BASIC`** → host file `HELP/HELP_BASIC.item`.
- **`HELP-LIST`** lists topics by **canonical** strings (underscores mapped back to spaces). **Limitation:** a topic id that intentionally used underscores may be displayed with spaces; avoid ambiguous ids if that matters.

## `HELP` with no arguments

Resolves canonical topic **`HELP`** through the **same chain** as other topics (`HELP` file local → SYSPROG → builtins). If still missing, the shell prints one short built-in summary line pointing at **`HELP-LIST`** / `docs/tcl-shell.md`.

## `HELP COMMANDS` (dynamic catalogue)

Canonical topic **`HELP COMMANDS`** (PickFS storage id **`HELP_COMMANDS`** when stored as an ordinary record) normally has **no static body**.

- **`HELP COMMANDS`** (two tokens — second word case-insensitive **`COMMANDS`**) resolves to **`HELP COMMANDS`** even though a single VOC operand would usually be noun-resolved separately.
- Alternatively: **`HELP HELP COMMANDS`** (multi-operand join → **`HELP COMMANDS`**).
- Lookup order unchanged: **`HELP`**/HELP_COMMANDS **`HELP`** file (local → SYSPROG) overrides; if neither supplies a **`HELP_COMMANDS`** record, the shell **generates** a topic body: introductory lines from the milestone text, then sorted **canonical Tcl verb names** (every built-in Tcl command keyword) plus any **VOC item id** whose **`resolveVerbName`** chains to one of those Tcl verbs (synonyms shown as extra names).
- Operators use **`HELP <verb>`** (or VOC spellings documented in VOC) for per-command usage detail.

Because **`HELP COMMANDS`** is generated from **`tclCommands_` + VOC** for the active session, it differs by account VOC contents and Pick host build/version.

## Authoring commands

- **`HELP-LIST`** — lists canonical topic names from the **account-local** logical file **`HELP`** only (sorted). If the **`HELP`** file does not exist or has no readable records: `No HELP topics`. Dynamic topics such as **`HELP COMMANDS`** are **not** HELP-file records and do not appear here.
- **`HELP-EDIT`** *topic-operands…* — same canonicalisation as **`HELP`**; ensures logical file **`HELP`** exists; opens **`ED>`** line editor on the mapped record id.

PROC uses the Tcl bridge (**`TCL HELP …`**); resolution is identical to interactive **`HELP`**.

## Bootstrap

Committed **`gemini/accounts/SYSPROG/HELP/`** ships minimal topics (`HELP`, `LIST`, `PROC`, …). New accounts created from the Gemini layout should include an **`HELP`** logical file directory when operators need account-local overrides (often empty initially).
