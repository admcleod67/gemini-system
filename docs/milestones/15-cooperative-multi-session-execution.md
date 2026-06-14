← [Project milestones index](../milestones.md)

## Milestone 15 — Cooperative Multi-Session Execution

Allow multiple sessions to coexist and make progress without true preemptive concurrency. Introduce a **cooperative scheduler** that switches sessions only at safe **I/O boundaries** and documented VM yield points. Paused sessions retain full VM and shell state. Apply simple fairness rules (for example round-robin or run-until-I/O). No preemption—preserving Pick authenticity and keeping the runtime model simple. *Status: planned.*

### Scheduler model

- **Cooperative only:** the scheduler switches sessions at documented yield points; no preemption, no threads fighting over VM reentrancy.
- **Paused sessions** retain full VM stack, shell state, and lock bindings ([**Milestone 10**](10-record-locking-multi-user.md)).
- **Fairness:** simple round-robin or run-until-I/O—Pick-authentic simplicity over OS-style time slicing.

### Yield points (initial list)

Cooperative scheduling requires an explicit enumeration; without it, M15 becomes open-ended. Initial yield boundaries:

- Tcl REPL read (**`Shell::runTclRepl`** input wait)
- BASIC **`INPUT`** / **`INPUT_STR`** (and PROC equivalents that block on input)
- Blocking file I/O (if any remain at this layer)
- Optional explicit **`SLEEP`** or future yield opcode
- VM debug **`STEP`** / **`CONT`** in assembler mode

### Grounding

- Completes the progression: M13 (session objects, serial run) → M14 (multiple consoles, serial run) → **M15 (multiple sessions make progress)**.
- Reuses M12 session I/O channels as the natural switch boundary—sessions block on I/O, scheduler picks the next runnable session.
