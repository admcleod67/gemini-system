← [Project milestones index](../milestones.md)

## Milestone 11 — Multi‑Language Runtime Infrastructure (Expanded)

Milestone 11 establishes the runtime substrate that allows Gemini to execute programs compiled from multiple languages—BASIC, Pascal, COMAL, COBOL, and future front‑ends—**without modifying or rebuilding the VM**. This milestone introduces a stable, language‑agnostic **`CALL_FUNC`** opcode, a global **`LanguageRegistry`**, and a **dynamic module loader** that discovers and registers language helper libraries at system boot. The VM remains deliberately minimal: it does not encode language semantics; instead, language‑specific behaviour is delegated to dynamically loadable modules. External compilers (such as **Apollo**) can target Gemini by emitting bytecode referencing **namespace IDs** and **function IDs**, enabling new languages to be added simply by placing shared libraries in the module directory and restarting the system.

This milestone completes the architectural shift from a single‑language VM to a **multi‑language execution host**, while preserving Pick‑authentic dynamic program loading and ensuring that **existing bytecode remains stable and unaffected**.

---

### 1. Objectives

#### 1.1 Architectural goals

- Introduce a stable, language‑agnostic function‑call mechanism (**`CALL_FUNC`**) for all language built‑ins.
- Provide a global registry for language namespaces, function tables, and optional runtime hooks.
- Support dynamic discovery and loading of language helper modules at system boot.
- Decouple language semantics from the VM core, enabling new languages without rebuilding Gemini.
- Preserve Pick‑authentic dynamic program loading: object code stored in Pick files remains executable without system rebuilds.

#### 1.2 Compatibility goals

- Maintain full backward compatibility for all existing bytecode that does not use **`CALL_FUNC`**.
- Migrate BASIC built‑ins ([Milestone 7](07-basic-functions-mat-operations.md)) onto the new dispatch mechanism without changing BASIC source semantics.
- Provide a stable ABI for external compilers (Apollo, future Pascal/COMAL/COBOL compilers).

---

### 2. `CALL_FUNC` opcode (language‑agnostic dispatch)

#### 2.1 Purpose

**`CALL_FUNC`** is the sole VM opcode for invoking language‑specific built‑ins. It replaces ad‑hoc lowering paths and prevents language semantics from leaking into the VM.

#### 2.2 Conceptual form

```text
CALL_FUNC <namespace-id>, <function-id>, <arg-count>
```

Exact operand encoding (layout, types, versioning) is specified in [`docs/vm.md`](../vm.md) at implementation time.

#### 2.3 Semantics

- Pop **`<arg-count>`** arguments from the VM stack (evaluation order documented).
- Resolve **`<namespace-id>`** in the **`LanguageRegistry`**.
- Dispatch to the function table for that namespace.
- Invoke the function at **`<function-id>`** with the popped arguments.
- Push the return value onto the stack.
- Raise runtime errors for:
  - Unknown namespace
  - Unknown function ID
  - Arity mismatch
  - Module failure

#### 2.4 VM stability

- No new language‑specific opcodes are added beyond **`CALL_FUNC`** as the generic dispatch primitive.
- Existing bytecode remains unchanged in meaning.
- **`CALL_FUNC`** is strictly additive and does not alter prior semantics for programs that do not emit it.

---

### 3. `LanguageRegistry` (global dispatch table)

#### 3.1 Responsibilities

The registry maps **namespace IDs** to:

- **Function tables** (arrays of callable targets)
- **Optional runtime hooks** (initialisation, teardown, metadata)
- **Optional extension points** (reserved for future milestones; no dynamic opcode registration in M11)

#### 3.2 Registry characteristics

- Populated at boot by dynamic modules.
- The VM never branches on language names; it only uses numeric or opaque IDs.
- Registry is **immutable after boot** (no dynamic unloading in M11).

#### 3.3 ABI requirements

Each namespace must provide:

- A stable **namespace ID**
- A **function table** with fixed indices
- Optional **metadata** (version, language name, capabilities)

Implementation detail (C++ function pointers, thin wrappers, or equivalent indirection) is documented for module authors in the module ABI reference.

---

### 4. Dynamic module loader

#### 4.1 Boot‑time module discovery

A new loader scans a **configurable** directory (default: **`gemini/modules/`** next to catalogue layout, or a path set by host configuration) and attempts to load each shared library using the host API (**`dlopen`** on Unix‑like systems, **`LoadLibrary`** on Windows).

#### 4.2 Module entry point

Each module must export:

```cpp
extern "C" void register_language(LanguageRegistry&);
```

The module:

- Registers its namespace
- Provides its function table
- Installs optional hooks
- Performs any initialisation required for its language runtime

After all modules load, **`CALL_FUNC`** can resolve any namespace supplied by those modules.

#### 4.3 Supported module types

- **BASIC** built‑ins (migrated from [Milestone 7](07-basic-functions-mat-operations.md))
- **Pascal** helper library
- **COMAL** helper library
- **COBOL** helper library
- Future languages (e.g. FORTRAN, RPG, etc.)
- **Stub modules** for languages not yet implemented

#### 4.4 Error handling

- Failed modules do not crash the system.
- The registry only contains successfully registered namespaces.
- Diagnostics are logged for operator visibility.

---

### 5. BASIC migration to `CALL_FUNC`

#### 5.1 Motivation

BASIC built‑ins currently use ad‑hoc lowering paths (notably **`InvokeBuiltin`** and compiler‑specific emission). Milestone 11 migrates them to the new dispatch mechanism.

#### 5.2 Migration steps

1. Create a **BASIC** namespace with a stable namespace ID.
2. Move all Milestone 7 built‑ins into a BASIC module:
   - **`LEN`**, **`TRIM`**, **`FIELD`**, **`ABS`**, **`DATE`**, etc.
3. Update the BASIC compiler to emit **`CALL_FUNC`** for built‑ins.
4. Remove old lowering paths once tests are stable.
5. Preserve all BASIC source semantics and observable behaviour.

#### 5.3 Result

BASIC becomes a **first‑class module**, not a special case in the VM.

---

### 6. External compiler support (Apollo and others)

#### 6.1 ABI for compilers

External compilers must know:

- **Namespace IDs**
- **Function ID tables**
- **Bytecode encoding** for **`CALL_FUNC`**
- **Error behaviour** for invalid calls

Published constants, headers, or schema define namespace and function IDs; operand layout and versioning are part of the bytecode contract.

#### 6.2 Integration workflow

1. Compiler emits bytecode referencing known namespaces.
2. Compiled object code is stored in normal Pick files (same dynamic loading story as today’s object records).
3. Gemini loads and executes the bytecode **without rebuilding the system**.
4. New languages can be added by dropping modules into the module directory and restarting.

#### 6.3 Benefits

- Gemini becomes a **multi‑language host**.
- No VM rebuild required for new languages.
- External compilers can evolve independently.

---

### 7. Documentation and tooling

#### 7.1 Documentation deliverables

- [`docs/vm.md`](../vm.md): **`CALL_FUNC`** encoding, operand layout, error model.
- [`docs/compiler.md`](../compiler.md) or dedicated **`docs/bytecode.md`**: ABI for compilers.
- **Module ABI documentation** (`register_language`, function table layout, namespace IDs, versioning).
- Updated [milestone index](../milestones.md) and cross‑links (including from [Milestone 7](07-basic-functions-mat-operations.md)).
- A **reference BASIC module** as the canonical example implementation.

#### 7.2 Developer/operator tooling

- A command or Tcl surface to inspect loaded languages:
  - **`SYSTEM LANGUAGES`**
  - or **`SHOW MODULES`**
- Optional debug tooling to dump function tables.

Exact UX is decided at implementation time; behaviour must expose namespace IDs, module version metadata, and load failures from boot diagnostics.

---

### 8. Non‑goals (explicitly deferred)

To preserve minimalism and avoid premature complexity:

- No dynamic unloading of modules
- No runtime addition of new VM opcodes from modules (M11 does not make the opcode set user‑extensible beyond **`CALL_FUNC`** dispatch)
- No JIT or optimisation layer
- No cross‑language symbol resolution
- No sandboxing or security hardening
- No user‑authored arbitrary shared libraries outside operator‑controlled module policy

These belong to future extensibility, performance, and security milestones.

---

### 9. Completion criteria

Milestone 11 is complete when:

- [ ] **`CALL_FUNC`** opcode is implemented and documented.
- [ ] **`LanguageRegistry`** is implemented and populated at boot.
- [ ] Dynamic module loader loads and registers modules.
- [ ] BASIC built‑ins are migrated to the new dispatch mechanism.
- [ ] External compilers can target Gemini using namespace/function IDs.
- [ ] Existing bytecode remains fully compatible.
- [ ] Documentation and tooling are updated.

---

### Delivery plan

Implementation is sequenced into vertical stages. Each stage ships a test‑locked slice before the next starts. Introduce the **`LanguageRegistry`** and dispatch contract first; add **`CALL_FUNC`** and assembler support second; wire **dynamic module loading** third; **migrate BASIC built‑ins** fourth; publish the **external ABI** and reference stubs fifth; finish with **operator tooling and docs** sixth. Detailed per‑stage plans may live in `~/.cursor/plans/m11_stage_*.plan.md` during implementation; status is summarised here as each stage lands (matching the M8 / M9 / M10 precedent).

- **Stage 1 — Registry foundation**: implement **`LanguageRegistry`** (namespace ID → function table, optional hooks/metadata); stable runtime errors for unknown namespace, unknown function ID, and arity mismatch; unit tests only with an **in‑process test namespace** (no **`dlopen`**, no compiler or **`InvokeBuiltin`** changes). *Status: pending.*

- **Stage 2 — `CALL_FUNC` VM + assembler**: add **`CALL_FUNC`** opcode to **`Runtime`**, text assembler (**`Parser`**, **`InstructionPrint`**, **`BytecodeText`**); dispatch through the registry populated with the Stage 1 test namespace; VM/assembler/runtime tests; existing bytecode unchanged. No dynamic loading yet. *Status: pending.*

- **Stage 3 — Dynamic module loader**: boot‑time scan of configurable **`gemini/modules/`** (or host‑configured path); **`dlopen`** / **`LoadLibrary`**; **`register_language(LanguageRegistry&)`** C ABI; failed modules logged, not fatal; registry immutable after boot. Ship a **minimal stub module** proving load + registration. No BASIC migration yet. *Status: pending.*

- **Stage 4 — BASIC module + compiler migration**: create **BASIC** namespace with stable IDs; move [Milestone 7](07-basic-functions-mat-operations.md) built‑ins (**`LEN`**, **`TRIM`**, **`FIELD`**, **`ABS`**, **`DATE`**, etc.) into a BASIC shared module; update compiler to emit **`CALL_FUNC`**; remove **`InvokeBuiltin`** lowering paths once tests are green; preserve all BASIC source semantics. *Status: pending.*

- **Stage 5 — External ABI + reference docs**: publish namespace/function ID constants (header or schema); document **`CALL_FUNC`** operand layout and module ABI; update [`docs/vm.md`](../vm.md) and compiler/bytecode contract (**[`docs/compiler.md`](../compiler.md)** or **`docs/bytecode.md`**); optional **stub modules** for Pascal/COMAL/COBOL (registration only, no compilers). *Status: pending.*

- **Stage 6 — Tooling + docs — closes M11**: operator/developer surface to inspect loaded languages (**`SYSTEM LANGUAGES`** or **`SHOW MODULES`**); boot diagnostics for module failures; milestone index cross‑links; reference BASIC module documented as canonical example. **Closes Milestone 11.** *Status: pending.*

Only Stage 6's exit criteria should claim "Closes Milestone 11".

---

### Rationale

Gemini’s VM is intentionally **minimal** and **language‑agnostic**. To support multiple compiled languages without polluting the VM with language‑specific opcodes or requiring a **rebuild** for each new front‑end, Gemini needs a **stable generic call** primitive and a **boot‑time registration** path. Milestone 11 introduces that infrastructure **cleanly**: existing BASIC, PROC, and ENGLISH **source** semantics are not rewritten at the Pick layer; BASIC gains a **dispatch boundary** for built‑ins while the VM core stays a small execution engine. Once Milestone 11 is complete, Gemini becomes a **multi‑language** execution host: external compilers can target the VM, new languages can be added by **dropping** shared libraries into the module directory, and the VM remains **stable** and **future‑proof** relative to language surface growth.
