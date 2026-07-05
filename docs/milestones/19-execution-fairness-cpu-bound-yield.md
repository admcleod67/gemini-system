← [Project milestones index](../milestones.md)

## Milestone 19 — Execution Fairness: CPU-Bound Cooperative Yield

Extend [**Milestone 15**](15-cooperative-multi-session-execution.md) cooperative scheduling so sessions blocked in **CPU-bound** interpreter work (not only at I/O waits) periodically release the execution token. Preserve the single-interpreter-stack invariant: still no preemptive threading or parallel VM stacks. *Status: planned (post–Version 1.0).*

Depends on [**Milestone 18**](18-version-1-gemini-system-service.md) (Version 1.0 release). Do not start before v1.0 is tagged — M18 is a stabilization capstone; this milestone introduces new scheduler/VM behaviour.

---

### 1. Problem

M15 yields at **I/O boundaries** only (login reads, `TCL>` line input, BASIC `INPUT`, debugger prompt waits). A session running a long **CPU-bound** BASIC program holds the execution token until the run finishes, hits a breakpoint, errors, or reaches an `INPUT`.

**Observed case:** nested loops with no input — outer `FOR I=0 TO 65535`, inner `GOSUB` subroutine with `FOR J=0 TO 65536`, optional `PRINT` on each outer iteration. Other attached consoles cannot reach `TCL>` or run commands until the program completes (~billions of VM steps).

Round-robin in [`CooperativeSessionRunner`](../../src/core/daemon/CooperativeSessionRunner.cpp) applies only among sessions that have **yielded** and are `Runnable`. A session inside [`Shell::runBasicUntilStop`](../../src/userland/tcl/Shell.cpp) never yields, so others starve.

This is **documented M15 non-goal behaviour**, not a regression. Version 1.0 ships with I/O responsiveness at prompts; CPU fairness under load is post-1.0.

---

### 2. Purpose and rationale

Operators expect multi-session **responsiveness** not only at idle prompts but also when another terminal runs a runaway or long batch job — within Pick-authentic constraints (one interpreter stack, cooperative model).

M19 adds **voluntary yield inside long-running VM work** so other sessions can acquire the token and make progress, without OS time slices or pthread-per-session execution.

---

### 3. Scope

#### 3.1 Opcode-budget yield (v1 of M19)

- In [`Shell::runBasicUntilStop`](../../src/userland/tcl/Shell.cpp) and/or [`VmDebugService::stepRuntime`](../../src/userland/assembler/VmDebugService.cpp), after every **N** VM steps (configurable constant or daemon setting):
  - `release` execution token (or equivalent yield API on [`GeminiSessionHost`](../../src/userland/tcl/GeminiSessionHost.h))
  - `acquire` again before continuing — round-robin applies
- Same pattern for PROC/Tcl long runs if they hold the token without hitting I/O (audit call sites).
- Session VM state, lock bindings, and program counter **unchanged** across yield — only scheduler token moves.

#### 3.2 Operator cancel / BREAK (stretch)

- Interrupt a running BASIC (or debugger) program from the **same** session (Ctrl-C or `BREAK` command) or from **admin** surface ([`KILLSESSION`](17-service-integration-deployment.md) in M17).
- Wire through existing [`Runtime::clearInterrupt`](../../src/core/vm/Runtime.cpp) / interrupt flag where present.
- Cross-session cancel from another console is optional stretch; document security implications.

#### 3.3 Output backpressure yield (optional)

- If session output queue / IPC flush blocks on full socket buffer for extended period, yield token so other sessions run (helps heavy `PRINT` floods; does not fix pure CPU inner loops with no output).

---

### 4. Non-goals

- Preemptive OS threading or pthread-per-session VM
- Parallel interpreter stacks
- Mid-instruction preemption (yield between opcodes/steps only)
- Changes to Tcl/BASIC/PROC **language semantics** beyond documented interrupt/cancel behaviour
- Priority queues, account quotas, or fair-share CPU scheduling
- Yield inside [`LockTable`](../../src/core/locking/LockTable.h) critical sections in ways that violate M10 lock ownership rules

---

### 5. Compatibility expectations

- **`gemini-system`** (`maxSessions = 1`): opcode-budget yield is a no-op or degenerates to immediate re-acquire — operator-visible behaviour unchanged.
- M15 I/O yield points unchanged.
- Lock bindings retained across yield (same as M15 `WaitingForInput`).

---

### 6. Deliverables (anticipated)

**Code:**

| Area | Path |
|------|------|
| VM step loop yield hook | [`Shell.cpp`](../../src/userland/tcl/Shell.cpp) `runBasicUntilStop` |
| Step execution | [`VmDebugService`](../../src/userland/assembler/VmDebugService.cpp) |
| Scheduler API (if extended) | [`CooperativeSessionRunner`](../../src/core/daemon/CooperativeSessionRunner.h), [`GeminiSessionHost`](../../src/userland/tcl/GeminiSessionHost.h) |
| Interrupt / BREAK | [`Runtime`](../../src/core/vm/Runtime.cpp), [`Shell`](../../src/userland/tcl/Shell.cpp) |

**Tests:**

- Unit: two sessions — session A in tight BASIC loop with opcode budget; session B runs `WHO` within bounded wall time
- Integration: two-console `gemini-daemon` smoke mirroring the nested-`FOR`/`GOSUB` repro case
- Regression: M15 I/O yield tests unchanged

**Docs:**

- Update [`docs/daemon.md`](../daemon.md) — remove or narrow v1.0 CPU starvation limitation; document yield budget and BREAK
- Update [`docs/console.md`](../console.md) — operator expectations under CPU load

---

### 7. Suggested implementation stages

1. **Opcode-budget yield** — constant N in `runBasicUntilStop`; two-session unit + integration test. **Minimum viable M19.**
2. **BREAK / interrupt** — same-session cancel; document Ctrl-C policy.
3. **Output backpressure yield** — if IPC output stall reproduces starvation with `PRINT`-heavy loops.
4. **Docs + closes M19** — daemon/console operator notes; full test suite green.

---

### 8. Example repro (for tests)

```basic
10 FOR I=0 TO 65535
20 PRINT I
25 GOSUB 100
30 NEXT I
40 END
100 FOR J=0 TO 65536
110 NEXT J
120 RETURN
```

Run from console A; console B should reach `TCL>` and execute `WHO` within a bounded timeout once opcode-budget yield is implemented.
