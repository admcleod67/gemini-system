# Assembler shell (ASM)

The assembler shell is now the VM-level debugger environment.

- **Enter:** `ASM [programName]` from `TCL>`.
- **Prompt:** `ASM> `
- **Exit shell mode:** `QUIT`
- **Exit VM debugger context only:** `END`

This Stage 1 milestone intentionally introduces debugger ownership before introducing a disassembler or assembler syntax.

## Scope in Stage 1

- Owns VM-level debugging commands previously in TCL.
- Uses the same VM runtime and loaded bytecode image model.
- Leaves BASIC debugger user experience unchanged (same prompt/commands/source-line behavior).

## Commands

| Command | Description |
|---------|-------------|
| `RUN [name]` | `RUN name` uses the same VOC-backed resolution as Tcl **`RUN`**: load bytecode from **`<name>_OBJ`** in the resolved Pick file (or compile from source **`name`** if object is missing—see Tcl shell). Bare `RUN` resumes from breakpoint/suspended state in ASM. |
| `CONT` | Alias for bare `RUN` resume. |
| `STEP` | Single-step one VM instruction for the loaded program image. |
| `TRACE ON|OFF` | Toggle per-instruction tracing during `RUN`/`CONT`. |
| `BREAKPOINT n` | Set breakpoint at instruction index `n`. |
| `BREAKPOINTS` | List configured breakpoints. |
| `CLEAR-BREAKPOINT n` | Remove one breakpoint index. |
| `CLEAR-BREAKPOINTS` | Remove all breakpoint indices. |
| `DUMP-STACK` | Print VM stack contents. |
| `DUMP-PROGRAM` | Disassemble loaded VM program image. |
| `DUMP-LABELS` | Show loaded label to instruction index mappings. |
| `END` | Clear loaded program/debug context but remain in ASM mode. |
| `QUIT` | Return to TCL prompt. |

## Deferred follow-on stages

- **Stage 2:** Add disassembler-focused enhancements as bytecode readability needs grow.
- **Stage 3:** Add full assembler source syntax, parser, and emission workflow.

TCL remains intentionally slim and should not re-absorb VM-level debugger command ownership.
