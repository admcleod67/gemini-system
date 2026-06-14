← [Project milestones index](../milestones.md)

## Milestone 16 — Standalone Edition / Application Mode

Build the single-session **application edition** on top of the multi-session architecture rather than a separate codebase. Provide a **standalone wrapper** that launches the daemon in-process, creates one session, and binds the console directly (bypassing IPC for speed). Preserve the same interactive experience as today’s **`gemini-system`**. Optionally fall back to standalone when no system service is running. Package as a clean **Gemini Application Edition** deliverable. *Status: planned.*
