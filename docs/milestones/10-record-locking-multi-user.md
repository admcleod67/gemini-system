← [Project milestones index](../milestones.md)

## Milestone 10 — Record Locking & Multi‑User Concurrency

Introduce Pick‑authentic record‑locking semantics and foundational multi‑user behaviour across the system. This milestone adds READU, WRITEU, and RELEASE operations, enforces lock ownership rules, and ensures safe concurrent access to shared files. It establishes the session‑level locking table, integrates lock checks into BASIC, PROC, and TCL file operations, and provides clear error/reporting behaviour for lock conflicts. This milestone completes the core concurrency model required for multi‑user Pick environments and prepares the system for future work on transaction‑style operations and distributed sessions.

