← [Project milestones index](../milestones.md)

## Milestone 15 — Cooperative Multi-Session Execution

Allow multiple sessions to coexist and make progress without true preemptive concurrency. Introduce a **cooperative scheduler** that switches sessions only at safe **I/O boundaries** and documented VM yield points. Paused sessions retain full VM and shell state. Apply simple fairness rules (for example round-robin or run-until-I/O). No preemption—preserving Pick authenticity and keeping the runtime model simple. *Status: planned.*
