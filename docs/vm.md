# Bytecode virtual machine

The VM executes a linear sequence of **instructions** with a single **operand stack** whose values are either **32-bit `int`** or **`std::string`**, and an **instruction pointer (IP)** into the program.

Implementation lives under `src/core/vm/` (`Runtime`, `Parser`, `InstructionPrint`). For C++ code outside that tree, the supported include surface is **`#include <pickvm/core.hpp>`** (see `include/pickvm/core.hpp`).

## Text bytecode (`.tbc`)

Programs are **newline-separated** lines of text. Rules:

- **Blank lines** and lines whose first non-whitespace character is **`#`** are ignored.
- **`label:`** — optional label at the start of a line (before the opcode). Labels map to the **instruction index** of the next emitted opcode on that logical line (a label-only line does not emit an instruction).
- **`OPCODE`** and optional **operand** follow; operands are opcode-specific.
- **`PUSH_STR`** string literals may be written in **double quotes**; surrounding quotes are stripped.

The parser records **1-based physical source line numbers** per instruction (for diagnostics and optional `(line N)` suffixes in dumps/trace).

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
| `JUMP label` | Set IP to the instruction index of `label` (resolved at parse time). |
| `JZ label` | Pop int; if it is **zero**, jump to `label`; otherwise fall through. |
| `STORE_VAR name` | Pop value and store it by case-insensitive variable `name` (runtime canonicalizes to uppercase). |
| `LOAD_VAR name` | Push value stored for case-insensitive variable `name`; throws `Undefined variable: <NAME>` if missing. |

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
- **`run()`** — run until **`HALT`** or IP past the last instruction.
- **`step()`** — execute **one** instruction; returns **`false`** when execution stops (including after **`HALT`**, which leaves the VM in a finished state with IP at end of image).
- **`instructionPointer()`** — current IP (0-based index into the loaded program).
- **`isLoaded()`** — true if a non-empty program is loaded.
- **`setOutputStream(ostream*)`** — where **`PRINT_*`** go; **`nullptr`** means **`std::cout`**.
- **`setInputStream(istream*)`** — where **`INPUT_STR`** and **`INPUT_INT`** read from; **`nullptr`** means **`std::cin`**.
- **`stack()`** / **`dumpStack()`** — inspection helpers.
- Runtime keeps a per-loaded-program variable map for `STORE_VAR`/`LOAD_VAR`; loading a new program resets it.

## Instruction listing

`PickVM::formatInstructionLine(ip, instr, loaded?)` produces lines like:

`{ip}: MNEMONIC [operands]`

Optional **`(line L)`** suffix when `loaded` is provided and a source line is known. Jump operands are printed as **numeric indices** after label resolution.

## Sample programs

See the **`programs/`** directory (e.g. `hello.tbc`, `stacktest.tbc`) for runnable examples.
