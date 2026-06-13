# Compiler architecture

This document describes the current BASIC compiler pipeline and phase boundaries.

## Phase overview

```mermaid
flowchart LR
    basicProgram[BasicProgram] --> parser[BasicStatementParser]
    parser --> stmtResult[StatementParseResult]
    stmtResult --> semantic[BasicSemanticAnalyzer]
    semantic --> normIr[NormalizedProgram]
    normIr --> emitter[BasicBytecodeEmitter]
    emitter --> compileResult[BasicCompileResult]
```

## Responsibilities

- `BasicStatementParser`
  - Parses BASIC source lines into statement AST nodes.
  - Produces syntax/shape diagnostics with source line numbers.
- `BasicSemanticAnalyzer`
  - Validates cross-line semantics (for example control-flow targets).
  - Converts statement AST into normalized backend IR.
  - Produces semantic diagnostics with source line numbers.
- `BasicBytecodeEmitter`
  - Lowers normalized IR into VM instructions.
  - Resolves jump addresses and appends implicit trailing `HALT` when required.
  - Emits **`CALL_FUNC`** for built-in function calls (namespace **`2`**, stable function IDs — see [`bytecode.md`](bytecode.md) and [`include/gemini/basic_function_ids.hpp`](../include/gemini/basic_function_ids.hpp)).
  - Enforces Pick BASIC type-suffix rules:
    - `$`-suffix variables used as arithmetic operands are rejected with a compile-time error.
    - `%`-suffix variables get a `COERCE_INT` instruction emitted after the value is produced (on `LET` assignment and `INPUT`).
  - Emits `PRINT_VAL` for all `PRINT` statements and `INPUT_STR` for all `INPUT` statements.
  - Guards malformed IR payloads and reports deterministic backend diagnostics.
- `BasicCompiler`
  - Orchestrates parse -> semantic -> emit.
  - Adapts phase diagnostics into `BasicCompileResult`.

## Data handoff contracts

- Parser output -> semantic input:
  - Statement AST may still include expression trees and raw target lines.
- Semantic output -> emitter input:
  - Control-flow targets are validated.
  - Statement forms are normalized in `BasicNormalizedIr`.
  - Emitter still validates required nullable payloads defensively.

## Diagnostic ownership

- Parser owns statement syntax diagnostics (for example malformed `IF THEN` target syntax).
- Semantic owns cross-line validity diagnostics (for example unknown target lines).
- Emitter owns malformed IR and expression-lowering diagnostics.
- `BasicCompiler` aggregates phase diagnostics into shell-visible compile errors.
