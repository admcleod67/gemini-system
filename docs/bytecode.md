# Bytecode contract for external compilers

This document is the **external compiler ABI** for invoking language-specific built-ins on the Gemini VM. It complements the VM opcode reference in [`vm.md`](vm.md) and the module author guide in [`language-modules.md`](language-modules.md).

Published numeric constants live in [`include/gemini/language_abi.hpp`](../include/gemini/language_abi.hpp). A machine-readable copy is in [`schemas/language-namespaces.json`](schemas/language-namespaces.json).

## Overview

Gemini dispatches language built-ins through a single VM opcode, **`CALL_FUNC`**. The VM pops arguments from its operand stack, looks up a **namespace ID** and **function ID** in the boot-time **`LanguageRegistry`**, and invokes the registered handler. Language semantics live in shared modules under `gemini/modules/`, not in the VM core.

External compilers (for example **Apollo**, or future Pascal/COMAL/COBOL front-ends) emit **`CALL_FUNC`** with stable IDs from the published headers. The in-tree BASIC compiler already does this.

## Text encoding (`.tbc`)

Text bytecode lines use comma-separated operands (see [`Parser.cpp`](../src/core/vm/Parser.cpp)):

```text
CALL_FUNC <namespace-id>, <function-id>, <arg-count>
```

| Operand | Type | Rule |
|---------|------|------|
| `namespace-id` | unsigned 32-bit integer | Must match a namespace registered at boot |
| `function-id` | unsigned 32-bit integer | Dense index into that namespace's function table |
| `arg-count` | non-negative integer | Number of stack operands consumed by the call |

**Example:** `CALL_FUNC 2, 3, 1` invokes BASIC function ID **3** (`LEN`) with one argument.

Decimal literals are accepted (`2`, `0x2` is not parsed as hex today — use decimal `2` for namespace BASIC).

## Stack contract

Before emitting **`CALL_FUNC`**, push exactly **`arg-count`** values onto the VM stack:

1. Evaluate arguments **left-to-right** (first argument pushed first, last argument on top).
2. Emit **`CALL_FUNC`** with the matching **`arg-count`**.

The handler pops from the **top of the stack first** (last argument first). Example for `MOD(a, b)`: push `a`, push `b`, then `CALL_FUNC …, 2`. The handler pops `b`, then `a`.

After dispatch completes, the handler has removed all arguments and pushed **one** return value. Net stack delta: **`-arg-count + 1`**.

### Value types

Stack values are [`PickVM::Value`](../src/core/vm/Runtime.h): **`int`**, **`double`**, or **`std::string`**. Handlers coerce operands according to language rules (BASIC uses the helpers in [`BasicBuiltinSupport`](../src/core/languages/basic/BasicBuiltinSupport.h)).

## Runtime requirements

The VM must have a **`LanguageRegistry`** attached (`Runtime::setLanguageRegistry`). Boot-time loading via [`LanguageModuleLoader`](../src/core/languages/LanguageModuleLoader.cpp) registers namespaces from `gemini/modules/`.

If the registry is not configured:

```text
LANG: registry not configured
```

If the stack holds fewer than **`arg-count`** values when **`CALL_FUNC`** executes:

```text
LANG: arity mismatch
```

## Error model

Registry dispatch uses stable **`LANG:`** prefixes ([`LanguageRegistry.cpp`](../src/core/languages/LanguageRegistry.cpp)):

| Condition | Message |
|-----------|---------|
| Unknown namespace | `LANG: unknown namespace (<id>)` |
| Unknown / unimplemented function slot | `LANG: unknown function (<id>)` |
| `arg-count` ≠ registered arity, or stack too shallow | `LANG: arity mismatch` |
| Registry frozen / duplicate registration (module load) | `LANG: registry frozen` / `LANG: duplicate namespace (<id>)` |

Semantic errors inside a handler (for example BASIC `LOG` domain) use language-specific prefixes such as **`BUILTIN:`** and are documented per language.

## Namespace IDs

| ID | Name | Module | Status |
|----|------|--------|--------|
| `1` | *(reserved)* | — | Internal unit tests only; not shipped |
| `2` | `basic` | `gemini-module-basic` | Reference implementation; 28 functions |
| `3` | `pascal` | `gemini-module-pascal` | Console IDs published; handlers are follow-on work |
| `4` | `comal` | `gemini-module-comal` | Stub — metadata only |
| `5` | `cobol` | `gemini-module-cobol` | Stub — metadata only |
| `256` (`0x100`) | `stub` | `gemini-module-stub` | Integration-test stub |

Constants: [`include/gemini/namespace_ids.hpp`](../include/gemini/namespace_ids.hpp). **IDs are immutable once shipped**; new languages take the next free slot from **`6`** upward.

## BASIC namespace (`2`) — function table

Namespace constant: `Gemini::kNamespaceIdBasic` / `Gemini::Basic::*` in [`include/gemini/basic_function_ids.hpp`](../include/gemini/basic_function_ids.hpp).

| ID | Name | Arity | Notes |
|----|------|-------|-------|
| 0 | `ABS` | 1 | |
| 1 | `SGN` | 1 | |
| 2 | `SEQ` | 1 | |
| 3 | `LEN` | 1 | |
| 4 | `TRIM` | 1 | |
| 5 | `LCASE` | 1 | |
| 6 | `UCASE` | 1 | |
| 7 | `SPACE` | 1 | |
| 8 | `INT` | 1 | |
| 9 | `MOD` | 2 | |
| 10 | `SIN` | 1 | radians |
| 11 | `COS` | 1 | radians |
| 12 | `TAN` | 1 | radians |
| 13 | `EXP` | 1 | |
| 14 | `LOG` | 1 | |
| 15 | `DATE` | 0 | |
| 16 | `TIME` | 0 | |
| 17 | `SYSTEM` | 1 | |
| 18 | `INDEX` | 3 | Third arg is occurrence (compiler supplies `1` when source omits it) |
| 19 | `FIELD` | 3 | |
| 20 | `STR` | 1 | |
| 21 | `OCONV` | 2 | |
| 22 | `ICONV` | 2 | |
| 23 | `NUM` | 1 | |
| 24 | `CONVERT` | 3 | |
| 25 | `RND` | 0 | Zero-argument form |
| 26 | `RND` | 1 | One-argument form (upper bound) |
| 27 | `STATUS` | 0 | |

**RND:** two distinct function IDs because the registry requires an exact arity match (no variadic slot).

**INDEX:** source may omit the third argument; the BASIC compiler pushes a default `1` and always emits **`arg-count` 3**.

See [`docs/basic-language.md`](basic-language.md) for Pick semantics of each built-in.

## Pascal namespace (`3`) — function table

Namespace constant: `Gemini::kNamespaceIdPascal` / `Gemini::Pascal::*` in [`include/gemini/pascal_function_ids.hpp`](../include/gemini/pascal_function_ids.hpp).

Published for a future **Apollo Compiler** `CALL_FUNC` binding. Apollo M5's completed bootstrap binding emits core `PRINT_*` / `INPUT_*` opcodes instead, so Gemini M19 / Apollo M6 did not require these handlers. The steady-state helper module belongs with Apollo.

| ID | Name | Arity | Notes |
|----|------|-------|-------|
| 0 | `write` | 1 | Pop one printable value (`int`, `double`, or `string`); write to stdout **without** newline |
| 1 | `writeln` | 1 | Pop one printable value; write to stdout and append newline |
| 2 | `writeln` | 0 | Write newline only (bare `writeln;`) |
| 3 | `read` | 1 | Pop **variable name** string; read from stdin and store into that variable via `Runtime*` (`hostContext`) |
| 4 | `readln` | 1 | Pop **variable name** string; read one input line and store into that variable via `Runtime*` |

**Variadic source calls:** Pascal `write(a, b, c)` and `writeln(a, b, c)` compile to **one `CALL_FUNC` per argument** (each with **`arg-count` 1**), left-to-right. This matches the registry's fixed-arity model (same pattern as BASIC `INDEX` supplying a default third argument).

**`read` / `readln`:** the compiler pushes the target variable name as a string literal. The module handler casts `hostContext` to [`PickVM::Runtime`](../src/core/vm/Runtime.h), reads from the runtime input stream, and stores into the named variable (using the same type rules Apollo's semantic analyser enforces). Handlers push **`0`** as a dummy return value to satisfy the stack contract.

**Example:** `writeln('Hello')` lowers to push string `"Hello"`, then `CALL_FUNC 3, 1, 1`.

## Legacy `INVOKE_BUILTIN`

Handwritten bytecode and older tests may use:

```text
INVOKE_BUILTIN "NAME"
```

The VM forwards name-based calls to the same registry (BASIC shim). **New compilers must emit `CALL_FUNC`**, not `INVOKE_BUILTIN`.

## See also

- [`language-modules.md`](language-modules.md) — writing a shared module; canonical BASIC module walkthrough
- [`vm.md`](vm.md) — VM opcode listing
- [`compiler-architecture.md`](compiler-architecture.md) — in-tree BASIC pipeline
- [`milestones/11-multi-language-runtime-infrastructure.md`](milestones/11-multi-language-runtime-infrastructure.md) — milestone context
