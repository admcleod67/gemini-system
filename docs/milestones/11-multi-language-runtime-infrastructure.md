← [Project milestones index](../milestones.md)

## Milestone 11 — Multi‑Language Runtime Infrastructure

Milestone 11 introduces the foundational runtime architecture required for Gemini to support multiple compiled languages (BASIC, Pascal, COMAL, COBOL, and future front‑ends) **without modifying or rebuilding the core system** for each new language. This milestone adds a stable, language‑agnostic **`CALL_FUNC`** opcode, a global **`LanguageRegistry`**, and a **dynamic module loader** that discovers and registers language helper libraries at system boot. The VM remains minimal in role: it does not encode language semantics; language‑specific behaviour lives in dynamically loadable modules. External compilers (for example **Apollo**) can target Gemini by emitting VM bytecode that references **known language namespaces** and **function IDs**, so new languages can be added by placing shared libraries in the system module directory and restarting — without rebuilding the Gemini binary. **Pick‑authentic dynamic program loading** is preserved: object code can be supplied via normal Pick file storage and executed when bytecode is valid.

### Goals

- Introduce a stable, VM‑level function‑call mechanism (**`CALL_FUNC`**) that supports **multiple language namespaces**.
- Add a global **`LanguageRegistry`** that maps language namespaces to **function tables** and optional **runtime hooks**.
- Support **dynamic loading** of language helper libraries at boot via shared libraries (`.so` / `.dll`).
- **Decouple** language semantics from the VM core, enabling new languages without rebuilding Gemini.
- **Migrate** BASIC built‑ins delivered in [Milestone 7](07-basic-functions-mat-operations.md) onto the new function‑dispatch mechanism (source compatibility unchanged).
- Preserve **Pick‑authentic dynamic program loading**: externally compiled object code can be added and executed without a full system rebuild.
- **Maintain VM stability:** no language‑specific opcodes, no semantic pollution of existing bytecode beyond the new dispatch primitive, and **no change** to the meaning of bytecode that does not use **`CALL_FUNC`** (existing programs keep prior semantics).

### Scope

#### 1. `CALL_FUNC` opcode (language‑agnostic function dispatch)

Introduce a new VM opcode (exact encoding to be specified in **`docs/vm.md`**), conceptually:

```text
CALL_FUNC <namespace-id>, <function-id>, <arg-count>
```

Semantics:

- Pop **`<arg-count>`** values from the VM stack (evaluation order documented).
- Resolve **`<namespace-id>`** in the **`LanguageRegistry`** and dispatch to that namespace’s **function table**.
- Invoke the function at **`<function-id>`** with the popped arguments.
- Push the **return value** onto the VM stack.
- Raise **runtime errors** for unknown namespaces, unknown function IDs, arity mismatches, or module failures.

This opcode is the **sole** VM mechanism for invoking language‑specific built‑ins across all registered languages (BASIC, future Pascal/COMAL/COBOL helpers, etc.).

#### 2. `LanguageRegistry` (global dispatch table)

Implement a global registry mapping **namespace ID** to:

- **Function table** (implementation detail: table of callable targets — C++ function pointers, thin wrappers, or equivalent indirection; ABI documented for module authors).
- **Optional runtime hooks** (initialisation, teardown, metadata queries).
- **Optional opcode extensions** — explicitly **deferred** beyond registration hooks unless a follow‑on milestone narrows the contract; registry may reserve extension points without implementing them in M11.

The registry is **populated at boot** from successfully loaded dynamic modules. The VM **does not** branch on language names; it only queries the registry by numeric or opaque **namespace** / **function** identifiers as defined in the bytecode format.

#### 3. Dynamic module loader (`modules/` directory)

Gemini loads language helper libraries **early in system boot**. A dedicated **module loader**:

- Scans a **configurable** directory (default example: **`gemini/modules/`** next to catalogue layout, or a path set by host configuration — exact default documented at implementation time).
- Attempts to load each shared library using the host API (**`dlopen`** on Unix‑like systems, **`LoadLibrary`** on Windows).
- Resolves and invokes a single **C ABI** entry point per module:

```cpp
extern "C" void register_language(LanguageRegistry&);
```

The module receives a reference to the global **`LanguageRegistry`** and registers its **namespace**, **function table**, and any **optional hooks**. After all modules load, **`CALL_FUNC`** can resolve any namespace supplied by those modules.

Modules may provide (non‑exhaustive):

- **BASIC** built‑ins (migrated from Milestone 7).
- **Pascal**, **COMAL**, **COBOL**, or other **future** language helper tables.
- Stubs or minimal tables until a full compiler ships.

#### 4. BASIC migration to `CALL_FUNC`

Refactor BASIC built‑ins introduced in Milestone 7:

- Move built‑in implementations (**`LEN`**, **`TRIM`**, **`FIELD`**, **`ABS`**, **`DATE`**, etc.) into a **BASIC** namespace function table (typically shipped inside the reference BASIC module or core‑adjacent library as decided at build time).
- Update the **BASIC compiler** to emit **`CALL_FUNC`** with the **BASIC** namespace and stable **function IDs** instead of ad hoc lowering, once the table and IDs are frozen.
- **Remove** direct lowering paths for those built‑ins **after** the function table and bytecode tests are stable.
- **Preserve** all Milestone 7 **source** semantics and observable behaviour; migration is **internal** to compiler and runtime.

#### 5. External compiler compatibility (Apollo)

Define a **stable ABI** for external compilers:

- **Namespace IDs** per language (published constants or header).
- **Function ID tables** (published metadata or schema).
- **Bytecode encoding** for **`CALL_FUNC`** (operand layout, endianness if applicable, versioning).
- **Error behaviour** for unknown namespaces, unknown functions, and bad arity.

**Apollo** and other external compilers target Gemini by emitting bytecode that references **known** namespaces. Compiled programs integrate via **normal Pick file storage** (same dynamic loading story as today’s object records); **no Gemini binary rebuild** is required to add a new **module** that registers a namespace.

#### 6. Documentation and tooling

- Document the **module ABI** (`register_language`, function table layout, namespace IDs, versioning).
- Document **`CALL_FUNC`** and bytecode encoding in **`docs/vm.md`**; compiler contract in **`docs/compiler.md`** (or dedicated **`docs/bytecode.md`** if split).
- Provide a **reference module** for BASIC as the canonical example.
- Update **[`docs/milestones.md`](../milestones.md)** (hub) and milestone files under [`docs/milestones/`](../milestones/) (including cross‑links from Milestone 7).
- Add **operator/developer tooling** to inspect loaded languages (for example a **`SYSTEM LANGUAGES`** surface or Tcl equivalent — exact UX decided at implementation time).

### Non‑Goals (Deferred)

- **No dynamic unloading** of modules after registration.
- **No runtime addition** of new VM opcodes from modules (M11 does not make the opcode set user‑extensible beyond **`CALL_FUNC`** dispatch).
- **No JIT** compilation or dynamic optimisation in M11.
- **No cross‑language** symbol resolution (e.g. Pascal calling BASIC built‑ins by name across namespaces without explicit ABI).
- **No module sandboxing** or hardened security model (trust model: operator‑controlled **`modules/`** only; hardening is a later milestone).
- **No** dynamic linking of **arbitrary user‑authored** untrusted libraries outside the controlled module policy (future milestone if needed).

These belong to later extensibility, performance, and security milestones.

### Rationale

Gemini’s VM is intentionally **minimal** and **language‑agnostic**. To support multiple compiled languages without polluting the VM with language‑specific opcodes or requiring a **rebuild** for each new front‑end, Gemini needs a **stable generic call** primitive and a **boot‑time registration** path. Milestone 11 introduces that infrastructure **cleanly**: existing BASIC, PROC, and ENGLISH **source** semantics are not rewritten at the Pick layer; BASIC gains a **dispatch boundary** for built‑ins while the VM core stays a small execution engine. Once Milestone 11 is complete, Gemini becomes a **multi‑language** execution host: external compilers can target the VM, new languages can be added by **dropping** shared libraries into the module directory, and the VM remains **stable** and **future‑proof** relative to language surface growth.
