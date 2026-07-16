# Bytecode virtual machine

The VM executes a linear sequence of **instructions** with a single **operand stack** whose values are either **32-bit `int`** or **`std::string`**, and an **instruction pointer (IP)** into the program.

Implementation lives under `src/core/vm/` (`Runtime`, `Parser`, `InstructionPrint`). For C++ code outside that tree, the supported include surface is **`#include <pickvm/core.hpp>`** (see `include/pickvm/core.hpp`).

## Text bytecode (`.tbc`)

Programs are **newline-separated** lines of text. Rules:

- **Blank lines** and lines whose first non-whitespace character is **`#`** are ignored, except optional metadata header lines like **`#BASICLINES:`**.
- **`label:`** — optional label at the start of a line (before the opcode). Labels map to the **instruction index** of the next emitted opcode on that logical line (a label-only line does not emit an instruction).
- **`OPCODE`** and optional **operand** follow; operands are opcode-specific.
- **`PUSH_STR`** string literals may be written in **double quotes**; surrounding quotes are stripped and common escapes (`\\`, `\"`, `\n`, `\r`, `\t`) are decoded.

When present, the optional `#BASICLINES:` metadata header provides source-line mapping. Otherwise, the parser records **1-based physical source line numbers** per instruction (for diagnostics and optional `(line N)` suffixes in dumps/trace).
For non-`.tbc` loaders (for example, handwritten instruction vectors), source-line metadata is optional.

## Opcodes

| Text | Meaning |
|------|---------|
| `HALT` | Stop execution. |
| `PUSH_INT n` | Push integer `n`. |
| `PUSH_STR "text"` | Push string (quotes optional in source; see parser). |
| `ADD` | Pop `b`, pop `a`, coerce both to int (strings → 0 on failure), push `a + b`. |
| `SUB` | Pop `b`, pop `a`, coerce both to int (strings → 0 on failure), push `a - b`. |
| `MUL` | Pop `b`, pop `a`, coerce both to int (strings → 0 on failure), push `a * b`. |
| `DIV` | Pop `b`, pop `a`, coerce both to int (strings → 0 on failure), push `a / b` (truncates toward zero). Throws `DIV: divide by zero` when `b` is zero. |
| `EQ` | Pop `b`, pop `a`, push `1` if `a == b`, else `0`. Both values must be the same type (int/int or string/string); mixed types throw. |
| `NE` | Pop `b`, pop `a`, push `1` if `a != b`, else `0`. Both values must be the same type; mixed types throw. |
| `LT` | Pop `b`, pop `a`, push `1` if `a < b`, else `0`. Both values must be the same type; mixed types throw. |
| `LE` | Pop `b`, pop `a`, push `1` if `a <= b`, else `0`. Both values must be the same type; mixed types throw. |
| `GT` | Pop `b`, pop `a`, push `1` if `a > b`, else `0`. Both values must be the same type; mixed types throw. |
| `GE` | Pop `b`, pop `a`, push `1` if `a >= b`, else `0`. Both values must be the same type; mixed types throw. |
| `CONCAT` | Pop `b`, pop `a`, push string concatenation `a + b`. |
| `DUP` | Duplicate top of stack. |
| `DROP` | Pop and discard top. |
| `PRINT_INT` | Pop int, write decimal to the runtime output stream (no line ending). Available for handwritten `.tbc`; the BASIC compiler emits `PRINT_VAL` instead. |
| `PRINT_STR` | Pop string, write to the runtime output stream (no line ending). Available for handwritten `.tbc`; the BASIC compiler emits `PRINT_VAL` instead. |
| `PRINT_VAL` | Pop a `Value` (int or string), write it to the runtime output stream (no line ending). Emitted by the BASIC compiler for all `PRINT` statements. |
| `PRINT_EOL` | Write end-of-line to the runtime output stream. |
| `INPUT_INT` | Read one input line, parse as int, and push it. Throws `INPUT_INT: end of input` on EOF and `INPUT_INT: invalid integer input` on parse failure. Available for handwritten `.tbc`; the BASIC compiler emits `INPUT_STR` instead. |
| `INPUT_STR` | Read one input line as a raw string (trimmed), and push it. Emitted by the BASIC compiler for all `INPUT` statements. |
| `COERCE_INT` | Pop a `Value`, convert to int (`strtol`; empty or non-numeric string → 0), push the resulting int. Emitted by the BASIC compiler after `INPUT_STR` for `%`-suffix variables and after expressions assigned to `%`-suffix variables. |
| `CALL n` | Push `ip+1` onto the call stack, then set IP to `n`. Used to implement `GOSUB`. |
| `RETURN` | Pop the top address from the call stack and set IP to it. Throws `"RETURN without GOSUB"` if the call stack is empty. |
| `FOR_SETUP name` | Pop `step` (top of stack), then pop `limit`; validate that `step ≠ 0` (throws `"FOR: STEP cannot be zero"`); push a loop frame `{varName=name, limit, step, bodyIP=ip+1}` onto the for-stack; fall through to the body. Used to implement `FOR`. |
| `FOR_NEXT name` | Increment variable `name` by the step stored in the top for-frame; if the termination condition is met (`step > 0 && var > limit` or `step < 0 && var < limit`) pop the frame and fall through; otherwise jump back to `bodyIP`. Throws `"NEXT without FOR"` if the for-stack is empty; throws `"FOR/NEXT variable mismatch"` if the frame's variable name does not match `name`. Used to implement `NEXT`. |
| `DIM_ARRAY name` | Pop int `n`; allocate (or re-allocate) a 1-based array of `n` elements under key `name`, each initialised to `""`. Throws `"DIM: size must be >= 1"` if `n < 1`. |
| `LOAD_ARR name` | Pop int index `i` (1-based); push `arrays[name][i-1]`. Throws `"Array not dimensioned: <name>"` if the array does not exist; throws `"Array index out of bounds: <name>"` if `i` is out of range. |
| `STORE_ARR name` | Pop int index `i` (top), then pop value; store value at `arrays[name][i-1]`. Same error conditions as `LOAD_ARR`. |
| `JUMP label|index` | Set IP to resolved target (`label` or numeric instruction index). |
| `JZ label|index` | Pop int; if it is **zero**, jump to resolved target (`label` or numeric index); otherwise fall through. |
| `STORE_VAR name` | Pop value and store it by case-insensitive variable `name` (runtime canonicalizes to uppercase). |
| `LOAD_VAR name` | Push value stored for case-insensitive variable `name`; throws `Undefined variable: <NAME>` if missing. |
| `CLEAR_VARS` | Remove all scalar variables and all array allocations. Stack, IP, call stack, and for-stack are untouched. |
| `ABS_INT` | Pop a `Value`, coerce to int, push `abs(value)`. |
| `SGN_INT` | Pop a `Value`, coerce to int, push `-1`, `0`, or `1` according to sign. |
| `SEQ_STR` | Pop a `Value`; if it is an int, stringify it first (`std::to_string`). Push the ASCII value of the first character of the resulting string, or `0` if the string is empty. |
| `OPEN_FILE name` | Pop file name expression value (stringifies ints), verify that file exists via runtime file backend, and bind it to file variable `name`. Throws on failure. |
| `OPEN_FILE_TRY name` | Same as `OPEN_FILE`, but pushes success flag (`1` success, `0` failure) instead of throwing on missing file. |
| `READ_REC name` | Pop record id expression value (stringifies ints), read record from currently open file variable `name`, and push record value. Throws on unopened handle or missing record. |
| `READ_REC_TRY name` | Try form of `READ_REC`: pushes record value (or `""`) and then success flag (`1`/`0`) instead of throwing for expected read failures. |
| `WRITE_REC name` | Pop record id, then value; write to currently open file variable `name`. Throws on unopened handle or backend write failure. |
| `WRITE_REC_TRY name` | Try form of `WRITE_REC`: pushes success flag (`1`/`0`) instead of throwing for expected write failures. |
| `READ_REC_U name` | Pop record id, read with update lock via session lock context, push record value. Lock conflict sets `STATUS()` and routes to `ON ERROR` when set, else throws `ERR_RECORD_LOCKED`. |
| `READ_REC_U_TRY name` | Try form of `READ_REC_U`: pushes record value (or `""`) and success flag; lock conflict is a try failure (`0`), no `ON ERROR` branch. |
| `WRITE_REC_U name` | Pop record id, then value; write with update lock. Same lock-conflict dispatch as `READ_REC_U`. |
| `WRITE_REC_U_TRY name` | Try form of `WRITE_REC_U`; lock conflict pushes `0`. |
| `RELEASE_REC name` | Pop record id and release session lock on that record (silent if not held). |
| `SET_ON_ERROR_HANDLER ip` | Set instruction pointer for `ON ERROR GOTO` (`0` disables). |
| `CLOSE_FILE name` | Release binding for file variable `name`. No-op if `name` is not currently open. |
| `CALL_FUNC ns-id, fn-id, arg-count` | Pop **`arg-count`** stack values (last argument on top), dispatch to the boot-time **`LanguageRegistry`** for namespace **`ns-id`**, function **`fn-id`**. Push the handler's return value. Requires a configured language registry. See [`bytecode.md`](bytecode.md) for encoding, stack order, namespace/function IDs, and **`LANG:`** errors. |
| `INVOKE_BUILTIN "name"` | Legacy name-based built-in dispatch (BASIC shim → registry). Prefer **`CALL_FUNC`** for new bytecode; see [`basic-language.md`](basic-language.md). |

Jump targets must refer to defined labels and resolve to valid instruction indices; the parser validates range.

## Parser API

`PickVM::Parser`:

- **`LoadedBytecode parse(std::istream&)`** / **`parseFile(path)`** return a struct with:
  - **`program`** — `std::vector<Instruction>`
  - **`labels`** — `std::unordered_map<std::string, int>` (label → instruction index)
  - **`sourceLinePerInstr`** — parallel to `program`, 1-based `.tbc` line per instruction (or `0` if unused)

## Runtime API

`PickVM::Runtime`:

- **`loadProgram(program)`** — replace program, clear stack, reset IP.
- **`loadProgram(program, sourceLinePerInstr)`** — same as above, with optional per-instruction source map.
- **`run()`** — run until **`HALT`** or IP past the last instruction.
- **`step()`** — execute **one** instruction; returns **`false`** when execution stops (including after **`HALT`**, which leaves the VM in a finished state with IP at end of image).
- **`instructionPointer()`** — current IP (0-based index into the loaded program).
- **`currentSourceLine()`** / **`sourceLineAtInstruction(ip)`** — source line lookup for debugger tooling.
- **`isLoaded()`** — true if a non-empty program is loaded.
- **`setOutputStream(ostream*)`** — where **`PRINT_*`** go; **`nullptr`** means **`std::cout`**.
- **`setInputStream(istream*)`** — where **`INPUT_STR`** and **`INPUT_INT`** read from; **`nullptr`** means **`std::cin`**.
- **`stack()`** / **`dumpStack()`** — inspection helpers.
- Runtime keeps a per-loaded-program variable map for `STORE_VAR`/`LOAD_VAR`, a **call stack** for `CALL`/`RETURN`, a **for-stack** for `FOR_SETUP`/`FOR_NEXT`, and an **arrays map** for `DIM_ARRAY`/`LOAD_ARR`/`STORE_ARR`; loading a new program resets all four.
- When no metadata is supplied (or metadata is shorter than the program), missing entries are treated as source line **`0`** (sentinel for "no source line").

## Standalone runner boundary

[Milestone 19](milestones/19-standalone-vm-runner.md) adds a Pick-independent host executable (**`gemini-vm`**) that runs `.tbc` files without catalogue, Tcl, session table, or Pick filesystem. The portable VM core already exists in **`gemini-core`**; Pick hosts wrap it with session and filesystem services. This section records the boundary for implementers. **`gemini-system`**, **`gemini-daemon`**, and **`gemini-console`** behaviour is unchanged.

### Usage (`gemini-vm`)

```text
gemini-vm [--modules PATH] [-h|--help] <program.tbc>
```

| Item | Behaviour |
|------|-----------|
| Program | Host filesystem path to text bytecode (`.tbc`) |
| Console | stdout / stdin (default VM streams) |
| Filesystem | Unbound — Pick FS opcodes fail if used |
| Modules | Optional; `--modules PATH`, else `GEMINI_MODULES_PATH`, else `./gemini/modules` |
| Exit `0` | Clean halt (or `--help`) |
| Exit `1` | Missing args, parse error, or runtime error (`gemini-vm: …` on stderr) |
| Exit `130` | Interrupted (`UserInterrupt`) |

Example: `gemini-vm programs/hello.tbc` prints `Hello, world`. Programs that use only core opcodes (e.g. `PRINT_*`) need no language modules. Programs that emit **`CALL_FUNC`** need a matching module loaded.

### Apollo Compiler workflow

Apollo Compiler Milestone 6 completed its standalone proof against `gemini-vm`. From an Apollo build tree beside this repository:

```bash
./build/src/tools/apolloc --emit examples/hello.pas \
  | ../pick-system/build/src/gemini-vm /dev/stdin

./build/src/tools/apolloc --emit examples/count.pas \
  | ../pick-system/build/src/gemini-vm /dev/stdin
```

The first command prints `Hello, Gemini!`; the second prints integers 1 through 10. For hosts without `/dev/stdin`, use an intermediate file:

```bash
./build/src/tools/apolloc --emit examples/hello.pas > hello.tbc
../pick-system/build/src/gemini-vm hello.tbc
```

Apollo M5's bootstrap console binding emits core `PRINT_*` / `INPUT_*` opcodes, so these commands do **not** require `--modules`. Pascal namespace **3** IDs remain published for a future `CALL_FUNC` module binding; that migration is follow-on work and was not required to close M19 / Apollo M6.

### VM entry paths today

| Entry path | Primary files | Pick coupling |
|------------|---------------|---------------|
| Application cold start | [`Main.cpp`](../src/Main.cpp), [`BootMonitor.cpp`](../src/core/boot/BootMonitor.cpp) | Catalogue, `ACCOUNTS.json`, module path under catalogue root |
| Daemon multi-session | [`daemon/Main.cpp`](../src/daemon/Main.cpp), [`GeminiServiceDaemon`](../src/core/daemon/GeminiServiceDaemon.h) | Same + IPC, [`SessionTable`](../src/userland/tcl/SessionTable.h) |
| Tcl `RUN` | [`Shell::cmdRun`](../src/userland/tcl/Shell.cpp) | VOC resolver, Pick FS `_OBJ` records, [`SystemVarReaderScope`](../src/userland/tcl/Shell.cpp), CHAIN re-resolution |
| BASIC compile-and-run | [`Shell::executeCompiledBasicProgram`](../src/userland/tcl/Shell.cpp) | Session I/O streams, system variables, SIGINT → `interrupt()` |
| Assembler debugger loop | [`GeminiSession::executeVmLoop`](../src/userland/tcl/GeminiSession.cpp) | Breakpoints, session-scoped runtime |
| Direct API (tests) | [`test_Runtime.cpp`](../tests/core/test_Runtime.cpp), [`test_CallFunc.cpp`](../tests/core/test_CallFunc.cpp) | None — model for Stage 2 |

**Session-owned Pick surface:** [`GeminiSession`](../src/userland/tcl/GeminiSession.h) owns `PickVM::Runtime`, `PickFS::FileSystem`, `VocResolver`, and `Shell`. [`SessionTable`](../src/userland/tcl/SessionTable.cpp) wires `runtime().setLanguageRegistry()` from the daemon. FS opcodes throw `"filesystem backend not configured"` when `fileSystem_` is null. Lock opcodes (`READ_REC_U`, etc.) depend on session lock context.

**File loading:** [`Parser::parseFile`](../src/core/vm/Parser.cpp) opens a host path and parses `.tbc` text with no Pick coupling; **`gemini-vm`** uses it for the standalone runner. Tcl `RUN` loads bytecode from a Pick VOC record (`<name>_OBJ`) and rejects program names containing `.` (so `RUN mini.tbc` fails).

**Library note:** `gemini-core` bundles VM, catalogue, daemon, and login code. The standalone runner links **`gemini-core`** only (not **`gemini-tcl`**); a Mercury repo split is an explicit non-goal for M19.

### Portable vs Pick host

| Layer | Portable (`gemini-vm`, M19 Stage 2+) | Pick host only |
|-------|--------------------------------------|----------------|
| Load bytecode | `Parser::parseFile(argv path)` | `RUN` via VOC + `_OBJ` record |
| Execute | `Runtime::loadProgram` + `Runtime::run` | `executeVmLoop`, debugger, CHAIN |
| Console I/O | `setOutputStream` / `setInputStream`; PRINT/INPUT opcodes | Session-attached streams |
| Language modules | `loadLanguageModules` + `setLanguageRegistry` | [`BootMonitor`](../src/core/boot/BootMonitor.cpp) during cold start |
| Filesystem | Leave `setFileSystem(nullptr)` | `GeminiSession::fileSystem`, VOC |
| System variables | Omit `setSystemVariableReader` | `SystemVarReaderScope` (@USERNO, etc.) |
| Locks / multi-session | Not used | Session table, cooperative runner |
| Process UX | `argv → run → exit code` | Login, Tcl REPL, daemon IPC |

Pascal namespace **`3`** and its console function IDs are published in [`pascal_function_ids.hpp`](../include/gemini/pascal_function_ids.hpp) for a future **`CALL_FUNC`** binding; see [`bytecode.md`](bytecode.md). Apollo's completed bootstrap binding currently uses core console opcodes.

## Instruction listing

`PickVM::formatInstructionLine(ip, instr, loaded?)` produces lines like:

`{ip}: MNEMONIC [operands]`

Optional **`(line L)`** suffix when `loaded` is provided and a source line is known (`L > 0`). Jump operands are printed as **numeric indices** after label resolution.

## Sample programs

See the **`programs/`** directory (e.g. `hello.tbc`, `stacktest.tbc`) for runnable examples.

## See also

- [Concurrency and record locking](concurrency.md) — lock opcodes in context of the session lock model.
- [Bytecode contract for external compilers](bytecode.md) — **`CALL_FUNC`** ABI, namespace/function IDs, stack semantics.
- [Language module ABI](language-modules.md) — writing shared modules for **`register_language`**.
