← [Project milestones index](../milestones.md)

## Milestone 10 — Record Locking & Multi‑User Concurrency

Introduce Pick‑authentic record‑locking semantics and foundational multi‑user behaviour across the system. This milestone adds **`READU`**, **`WRITEU`**, and **`RELEASE`** operations, enforces lock ownership rules, and ensures safe concurrent access to shared files. It establishes a session‑level locking table, integrates lock checks into BASIC, PROC, and TCL file operations, and provides clear error and reporting behaviour for lock conflicts. This milestone completes the core concurrency model required for multi‑user Pick environments and prepares the system for future work on transaction‑style operations and distributed sessions.

### Goals

- Implement a **minimum viable concurrency model** for Pick‑authentic multi‑user behaviour.
- Provide **record‑level locking** with **session‑scoped** lock ownership.
- Integrate lock enforcement into all file‑accessing subsystems: **TCL**, **BASIC**, **PROC**, and the **filesystem** layer.
- Deliver **clean, deterministic lock‑release** semantics on session termination.
- Maintain stable, test‑locked conflict reporting across shells and languages.

### 1. Scope definition

This milestone focuses on record‑level locking, session‑scoped lock ownership, and integration into every subsystem that reads or writes Pick files. **Transactions are explicitly out of scope** (future milestone).

#### Core deliverables

- Session‑level lock table
- **`READU`**, **`WRITEU`**, **`RELEASE`** operations
- Lock conflict detection and error reporting
- Integration into **BASIC**, **PROC**, **TCL**
- Lock persistence across nested operations (same session)
- Clean, deterministic lock‑release semantics on session termination
- No transactions yet (future milestone)

---

### 2. Locking model specification

#### 2.1 Lock types

| Operation | Semantics |
|-----------|-----------|
| **`READU`** | Shared read with update intent (Pick‑authentic: effectively an **exclusive read lock**) |
| **`WRITEU`** | Exclusive write lock |
| **`RELEASE`** | Remove lock(s) held by the current session |

#### 2.2 Lock ownership rules

- Locks are owned by a **session**, not by a program or stack frame.
- A session may **re‑acquire** a lock it already holds (**idempotent**).
- A session may **not** acquire a lock held by another session.
- Locks persist until:
  - Explicit **`RELEASE`**
  - Implicit release on **session termination**
  - Implicit release on **file deletion** (rare case)

#### 2.3 Lock table structure

A simple, orthogonal in‑memory structure:

```text
LOCK_TABLE = {
    fileId: {
        recordId: {
            ownerSessionId,
            lockType,   // READU or WRITEU
            timestamp
        }
    }
}
```

Stored in memory; **no persistence** required for this milestone.

---

### 3. Integration points

#### 3.1 Filesystem layer

Add lock checks to:

- **`READ`**
- **`READU`**
- **`WRITE`**
- **`WRITEU`**
- **`DELETE`**
- **`MATREAD`** / **`MATWRITE`** (if applicable later)

The filesystem becomes the **single source of truth** for lock enforcement.

#### 3.2 TCL integration

Commands affected:

- **`READ`**
- **`READU`**
- **`WRITE`**
- **`WRITEU`**
- **`RELEASE`**
- **`DELETE`**

Behaviour:

- On lock conflict → TCL error: **`RECORD LOCKED`**
- Optional diagnostic: file, id, owner session

#### 3.3 BASIC integration

Compiler / runtime changes:

- **`READU`** / **`WRITEU`** statements map to new VM opcodes
- Runtime checks the lock table before performing file operations
- On conflict:
  - Set **`STATUS()`**
  - Branch to **`ON ERROR`** if defined
  - Otherwise raise a BASIC runtime error

#### 3.4 PROC integration

PROC file operations must respect locks:

- **`READU`**
- **`WRITEU`**
- **`RELEASE`**

Error behaviour: PROC‑style **`?LOCKED?`** or equivalent.

---

### 4. Error and conflict semantics

#### 4.1 Lock conflict conditions

A conflict occurs when:

- Session A requests **`READU`** or **`WRITEU`**
- Session B holds **`READU`** or **`WRITEU`** on the same record

#### 4.2 Error reporting

Pick‑authentic behaviour:

| Consumer | Behaviour |
|----------|-----------|
| **TCL** | **`RECORD LOCKED`** |
| **BASIC** | **`STATUS()`** set to non‑zero |
| **PROC** | **`?LOCKED?`** or equivalent |

#### 4.3 Optional diagnostics (Gemini‑specific enhancement)

Provide structured error metadata (optional but recommended for debugging):

- file
- record
- lock owner session
- lock type

---

### 5. Session lifecycle integration

#### 5.1 Session startup

- Create an empty lock table entry for the session.

#### 5.2 Session termination

- Release all locks owned by the session.
- Remove the session entry from the lock table.

#### 5.3 Session isolation

- Locks are never visible across sessions except via conflict errors.

---

### 6. VM / runtime additions

#### 6.1 New VM instructions

- **`READU`**
- **`WRITEU`**
- **`RELEASE`**

Each instruction:

1. Checks the lock table
2. Acquires or releases the lock
3. Delegates to existing **`READ`** / **`WRITE`** logic

#### 6.2 VM error model

Introduce a new VM error code:

- **`ERR_RECORD_LOCKED`**

---

### 7. Testing plan

#### 7.1 Unit tests

- Lock acquisition
- Lock re‑acquisition by the same session
- Lock conflict detection
- Lock release
- Session termination cleanup

#### 7.2 Integration tests

- TCL scripts performing concurrent operations
- BASIC programs using **`READU`** / **`WRITEU`**
- PROC scripts interacting with locked records

#### 7.3 Multi‑session simulation

- Two simulated sessions performing conflicting operations
- Ensure deterministic behaviour

---

### 8. Documentation deliverables

- Updated TCL command reference ([docs/tcl-shell.md](../tcl-shell.md))
- BASIC **`READU`** / **`WRITEU`** semantics ([docs/basic-language.md](../basic-language.md))
- PROC locking behaviour ([docs/proc.md](../proc.md))
- Developer documentation for lock table and VM opcodes ([docs/vm.md](../vm.md))
- Concurrency model overview (for future distributed sessions)

---

### 9. Out of scope (explicitly)

To preserve minimalism and avoid feature creep:

- No transaction semantics
- No distributed locking
- No lock timeouts
- No deadlock detection
- No lock escalation
- No file‑level locks

These belong to later milestones.

---

### 10. Milestone completion criteria

Milestone 10 is complete when:

- [ ] All lock operations are implemented
- [ ] All file operations respect locks
- [ ] BASIC, PROC, and TCL behave consistently
- [ ] Multi‑session tests pass
- [ ] Documentation is updated
- [ ] Concurrency model is stable and predictable

---

### Delivery plan

Implementation is sequenced into vertical stages. Each stage ships a test‑locked slice before the next starts. Introduce the in‑memory **lock table** first; wire **filesystem** enforcement and **session lifecycle** second; then surface **`READU`** / **`WRITEU`** / **`RELEASE`** through Tcl, BASIC/VM, and PROC in that order so all consumers call the same enforcement layer. Detailed per‑stage plans may live in `~/.cursor/plans/m10_stage_*.plan.md` during implementation; status is summarised here as each stage lands (matching the M4 / M5 / M8 / M9 precedent).

- **Stage 1 — Lock table foundation**: in‑memory lock table (`fileId` → `recordId` → owner, lock type, timestamp); acquire, release, idempotent re‑acquire by the same session; conflict detection when another session holds **`READU`** or **`WRITEU`**. Unit tests only — no filesystem, Tcl, BASIC, or PROC integration yet. Lock stable errors for conflicts. *Status: implemented.* Ships **`PickCore::Locking::LockTable`**, **`LockTypes`**, **`describeLockConflict`** (stable **`RECORD LOCKED`** substring), and **`tests/core/test_LockTable.cpp`**.

- **Stage 2 — Session scope + filesystem enforcement**: attach lock table to session identity; **`LOGOFF`** / session termination releases all locks held by the session; implicit release on file deletion. Filesystem becomes the single enforcement point: **`readU`** / **`writeU`** acquire then delegate to existing I/O; plain **`read`** / **`write`** / **`deleteRecord`** do not acquire but fail when another session holds the lock. Multi‑session simulation tests at the filesystem layer. *Status: implemented.* Ships **`LockRegistry`**, optional **`FileSystem`** lock context (**`setLockContext`** / **`readU`** / **`writeU`** / **`releaseRecord`**), **`ShellSession`** session binding and **`LOGOFF`** / **`QUIT`** cleanup, **`tests/core/test_FileSystemLocking.cpp`**, and **`tests/tcl/test_ShellSessionLocks.cpp`**.

- **Stage 3 — Tcl integration**: Tcl commands **`READU`**, **`WRITEU`**, **`RELEASE`**; existing **`READ`** / **`WRITE`** / **`DELETE-VOC`** route through lock‑aware filesystem paths when a session is logged in. On conflict → **`Error: RECORD LOCKED: …`** (stable, test‑locked). *Status: implemented.* Ships **`cmdReadU`** / **`cmdWriteU`** / **`cmdRelease`** in [`Shell.cpp`](src/userland/tcl/Shell.cpp), shared file/record operand parsing, HelpTopics entries, and **`tests/tcl/test_ShellLocking.cpp`**.

- **Stage 4 — VM + BASIC**: new VM opcodes **`READU`**, **`WRITEU`**, **`RELEASE`**; BASIC statement parser and bytecode emitter; runtime sets **`STATUS()`** and routes to **`ON ERROR`** when defined, otherwise raises with **`ERR_RECORD_LOCKED`**. Compiler/runtime tests; no PROC changes yet. *Status: pending.*

- **Stage 5 — PROC + docs — closes M10**: PROC **`READU`** / **`WRITEU`** / **`RELEASE`** with **`?LOCKED?`** (or equivalent stable token); integration tests across Tcl, BASIC, and PROC. Update [docs/tcl-shell.md](../tcl-shell.md), [docs/basic-language.md](../basic-language.md), [docs/proc.md](../proc.md), [docs/vm.md](../vm.md); add concurrency model overview. **`MATREAD`** / **`MATWRITE`** remain deferred unless trivially wired. **Closes Milestone 10.** *Status: pending.*

Only Stage 5's exit criteria should claim "Closes Milestone 10".

---

### Rationale

Record locking is the foundation of safe multi‑user Pick operation: without it, concurrent sessions can corrupt shared data silently. By centralising enforcement in the filesystem layer and surfacing consistent errors through TCL, BASIC, and PROC, Gemini gains a predictable concurrency model that matches operator expectations from classic Pick systems while remaining simple enough to test and reason about in a single‑host, multi‑session environment.
