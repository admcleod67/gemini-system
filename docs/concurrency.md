# Concurrency and record locking

Gemini implements a **minimum viable multi-user concurrency model** for single-host, multi-session Pick operation. Record-level locks are session-scoped, enforced centrally in the filesystem layer, and surfaced with consumer-specific error reporting.

## Lock table

An in-memory [`LockTable`](../src/core/locking/LockTable.h) maps `(fileId, recordId)` to:

- owning session id
- lock type (`READU` or `WRITEU`)
- acquisition timestamp

Locks are **not persisted** across process restarts.

## Filesystem enforcement

[`FileSystem`](../src/core/filesystem/FileSystem.h) is the single enforcement point when a session lock context is bound (`setLockContext`):

| Operation | Lock behaviour |
|-----------|----------------|
| `readU` / `writeU` | Acquire lock for current session, then perform I/O |
| `read` / `write` / `deleteRecord` | Do not acquire; fail if another session holds the lock |
| `releaseRecord` | Drop lock held by current session (no-op if not held) |

Lock conflicts throw `FileSystemError` with a stable **`RECORD LOCKED`** substring (see [`describeLockConflict`](../src/core/locking/LockTable.cpp)).

## Session lifecycle

After catalogue login, [`GeminiSession`](../src/userland/tcl/GeminiSession.cpp) **`attach()`** binds the shared lock table and a unique session id to the account filesystem handle.

Locks are released when:

- the session executes explicit **`RELEASE`** (Tcl / BASIC / PROC)
- **`LOGOFF`** or session termination runs (all locks for that session)
- the locked record is deleted (implicit release)

Without login, shells use independent filesystem handles with no cross-shell lock context.

## Consumer error mapping

| Consumer | Lock conflict behaviour |
|----------|-------------------------|
| **Tcl** | `Error: RECORD LOCKED: …` on stdout |
| **BASIC** | `STATUS()` set to `1`; `ON ERROR GOTO` when active; else `ERR_RECORD_LOCKED` runtime error |
| **PROC** (native) | `PROCERR` variable set to **`?LOCKED?`**; script continues |
| **PROC** (`TCL READU …`) | Tcl-style `Error: RECORD LOCKED: …` (bridge does not set `PROCERR`) |

Plain **`READ`** / **`WRITE`** on all consumers respect foreign locks when logged in.

## VM opcodes

Lock-aware VM instructions delegate to the same filesystem APIs. See [VM](vm.md) for `READ_REC_U`, `WRITE_REC_U`, `RELEASE_REC`, and `SET_ON_ERROR_HANDLER`.

## Explicitly out of scope

- Transaction semantics
- Distributed locking across hosts
- Lock timeouts and deadlock detection
- Lock escalation and file-level locks

These belong to later milestones.

## See also

- [Session model](session.md) — `GeminiSession` lifecycle and I/O
- [Milestone 10 — Record locking](milestones/10-record-locking-multi-user.md)
- [Tcl shell locking commands](tcl-shell.md)
- [BASIC READU / WRITEU](basic-language.md)
- [PROC locking statements](proc.md)
