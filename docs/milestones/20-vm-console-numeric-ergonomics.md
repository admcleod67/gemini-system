← [Project milestones index](../milestones.md)

## Milestone 20 — VM Console and Numeric Ergonomics

Add a small set of **language-neutral core opcodes** so compiled front-ends (Apollo Pascal first; handwritten `.tbc` and later BASIC welcome) can print a character from an integer code, read a float, and widen int→float without compiler workarounds. Optional core integer remainder. Existing opcode semantics — including Pick BASIC `PRINT_VAL`, `DIM_ARRAY`, and `MAT_*` — stay unchanged. *Status: implemented.*

Consumer ask: [`docs/apollo-consumer-notes.md`](../apollo-consumer-notes.md) (near-term P0–P3). Unblocks Apollo Compiler **Milestone 8 Stage 2** (console I/O fidelity) after Apollo has already lowered Wirth ordinal/arithmetic functions in the compiler.

Follows [Milestone 19](19-standalone-vm-runner.md). Next post–v1.0 delivery is shared math ([**Milestone 21**](21-shared-math-language-module.md)); then host FS façade ([**Milestone 22**](22-host-filesystem-facade.md)); R83 gaps ([**Milestone 23**](../compatibility-r83-pick.md); detail page TBD); CPU fairness remains deferred [**Milestone 24**](24-execution-fairness-cpu-bound-yield.md).

**Standing invariant:** `gemini-system`, `gemini-daemon`, `gemini-console`, and BASIC `MAT_*` / `DIM_ARRAY` / `PRINT_VAL` behaviour must not regress. Full `ctest` remains green after every stage. Prefer **additive** opcodes only.

---

### 1. Purpose and rationale

The standalone runner ([**Milestone 19**](19-standalone-vm-runner.md)) already executes Apollo console programs via bootstrap `PRINT_*` / `INPUT_*`. Pascal `char` values are ints on the stack; `PRINT_VAL` prints decimals (`65` instead of `A`). Real `readln` and int→float widening are compiler sequences. Those are awkward for **any** emitter, not Pascal-only.

This milestone ships **core primitives**. Dialect-shaped I/O (Pascal field widths, `eof`/`eoln`) stays on the `CALL_FUNC` / language-module path (M19 follow-on). Array ABI, host filesystem, and CPU fairness stay on other tracks.

---

### 2. Scope

#### 2.1 In scope (P0–P2 required; P3 optional)

| Priority | Opcode | Locked behaviour (see [`vm.md`](../vm.md)) |
|----------|--------|--------------------------------------------|
| **P0** | **`PRINT_CHAR`** | Pop int; write one character (0–255); no newline. |
| **P1** | **`INPUT_FLT`** | Read one input line; strict float parse; push `double`. |
| **P2** | **`COERCE_FLT`** | Pop a `Value`; convert to `double` (mirror of `COERCE_INT`). |
| **P3** | **`MOD`** | Pop `b`, pop `a` (ints); truncated toward-zero remainder. Floored **`IMOD`** deferred beyond M20. BASIC continues to use module `CALL_FUNC` `MOD`. |

Parser, `InstructionPrint`, `BytecodeText`, `Runtime::step`, and [`docs/vm.md`](../vm.md) stay in lockstep. Handwritten `.tbc` and `gemini-vm` accept the new mnemonics.

#### 2.2 Hard non-goals

- Changing `PRINT_VAL`, `DIM_ARRAY`, `MAT_COPY`, or `MAT_INIT` semantics
- Pascal field-width / Turbo-style real formatting (language module)
- Host filesystem façade / Pascal `file` I/O ([**Milestone 22**](22-host-filesystem-facade.md))
- Array value-parameter ABI or `var` array aliases ([consumer notes](../apollo-consumer-notes.md) §1–§2)
- Debug metadata / source-name mapping (consumer notes §6)
- Shared math ([**Milestone 21**](21-shared-math-language-module.md)); R83 gaps ([**Milestone 23**](../compatibility-r83-pick.md)) or CPU-bound yield ([**Milestone 24**](24-execution-fairness-cpu-bound-yield.md))
- Requiring Apollo to emit the new opcodes in the same Gemini release (Apollo switches its binding table afterwards)

---

### 3. Compatibility

- **BASIC / Pick:** existing programs and `_OBJ` text that do not mention the new mnemonics are byte-identical in behaviour. Do not replace BASIC `MOD` with a core opcode in this milestone.
- **Apollo:** success when Gemini documents the opcodes and tests prove them; Apollo M8 Stage 2 may then bind `write`/`writeln` of `char` to `PRINT_CHAR` and `readln(real)` to `INPUT_FLT`.
- **Standalone runner:** `gemini-vm` needs no CLI change; new opcodes ride the same parse/run path.

---

### 4. Deliverables

| Area | Artifact |
|------|----------|
| Runtime | `OpCode` + dispatch in [`Runtime`](../../src/core/vm/Runtime.cpp) |
| Text `.tbc` | [`Parser`](../../src/core/vm/Parser.cpp), [`InstructionPrint`](../../src/core/vm/InstructionPrint.cpp), [`BytecodeText`](../../src/core/vm/BytecodeText.cpp) |
| Tests | Unit cases: glyph print (`65` → `A`); `INPUT_FLT` happy/error; `COERCE_FLT`; `MOD` divide-by-zero / sign |
| Docs | Opcode rows in [`docs/vm.md`](../vm.md); pointer from [`docs/bytecode.md`](../bytecode.md) and consumer notes |

---

### 5. Milestone completion criteria

- [x] **`PRINT_CHAR`**, **`INPUT_FLT`**, and **`COERCE_FLT`** implemented, documented, and covered by tests
- [x] Core **`MOD`** shipped with truncated toward-zero semantics; floored **`IMOD`** deferred beyond M20 (see §7)
- [x] Existing `PRINT_VAL` / `MAT_*` / `DIM_ARRAY` tests unchanged in intent; full `ctest` green
- [x] Consumer notes near-term table marked implemented (P3 `MOD` shipped; `IMOD` deferred)
- [x] Hub / README list M20 implemented when closed

---

### 6. Suggested implementation stages

1. **P0 `PRINT_CHAR`** — opcode, parser, tests (`PUSH_INT 65` / `PRINT_CHAR` → `A`). *Status: implemented.*
2. **P1 `INPUT_FLT` + P2 `COERCE_FLT`** — align errors with `INPUT_INT` / `COERCE_INT`. *Status: implemented.*
3. **P3 remainder** — core **`MOD`** with truncated toward-zero integer remainder (C++ `%` / Turbo-style). Floored **`IMOD`** deferred beyond M20. *Status: implemented.*
4. **Docs + closes M20** — `vm.md` rows; consumer-notes status; hub. **Closes Milestone 20.** *Status: implemented.*

Only Stage 4 claims “Closes Milestone 20.”

---

### 7. Follow-on (beyond M20)

- Apollo switches console binding (compiler-side; not this repo’s close criterion)
- Floored integer remainder (**`IMOD`**) if a consumer needs non-truncated semantics
- Shared math language module — [**Milestone 21**](21-shared-math-language-module.md)
- Host filesystem façade / Pascal I/O — [**Milestone 22**](22-host-filesystem-facade.md)
- Array copy / `var` parameters, debug metadata — [`apollo-consumer-notes.md`](../apollo-consumer-notes.md) (separate tracks)
- R83 compatibility — [**Milestone 23**](../compatibility-r83-pick.md) (detail page TBD)
- CPU-bound cooperative yield — [**Milestone 24**](24-execution-fairness-cpu-bound-yield.md)

*Status: implemented.*
