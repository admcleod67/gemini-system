# Language module ABI

Gemini loads language helper libraries at **cold start** from a configurable modules directory (default: **`gemini/modules/`** next to the catalogue root). Each module registers one **namespace** with the global **`LanguageRegistry`**. The VM invokes built-ins through **`CALL_FUNC`** (see [`bytecode.md`](bytecode.md)).

## Boot discovery

At cold start, [`BootMonitor`](../src/core/boot/BootMonitor.cpp) scans the modules directory for shared libraries (`.dylib`, `.so`, `.dll`), loads each via the host dynamic linker, and calls the exported entry point. Override the path with **`GEMINI_MODULES_PATH`**.

See also the bootstrap summary in [`gemini-bootstrap.md`](gemini-bootstrap.md#language-modules-milestone-11).

Failed modules are **logged and skipped**; they do not abort startup. The registry is **frozen** after loading completes — no runtime registration or unloading in Milestone 11.

## Entry point

Each module must export a C-linkage symbol (see [`LanguageModuleAbi.h`](../src/core/languages/LanguageModuleAbi.h)):

```cpp
extern "C" void register_language(PickCore::Languages::LanguageRegistry &registry);
```

Use **`GEMINI_LANGUAGE_MODULE_EXPORT`** on the definition so the symbol is visible to **`dlopen`** / **`LoadLibrary`**.

The symbol name is **`register_language`** (`PickCore::Languages::kRegisterLanguageSymbol`).

## Namespace descriptor

Inside **`register_language`**, build a [`LanguageNamespaceDescriptor`](../src/core/languages/LanguageTypes.h):

| Field | Purpose |
|-------|---------|
| `id` | Stable **`NamespaceId`** (`std::uint32_t`) from [`include/gemini/namespace_ids.hpp`](../include/gemini/namespace_ids.hpp) |
| `metadata.name` | Short language name (for example `"basic"`, `"pascal"`) |
| `metadata.version` | Module version string (for example `"1"`) |
| `functions` | Dense vector indexed by **`FunctionId`** |
| `hooks.onInit` / `hooks.onTeardown` | Optional; called with boot **`hostContext`** when non-null |

Call **`registry.registerNamespace(std::move(descriptor), hostContext)`** once per module.

### Function table layout

`functions` is a **dense array**: slot **`N`** implements function ID **`N`**. Each entry is a [`LanguageFunctionEntry`](../src/core/languages/LanguageTypes.h):

```cpp
struct LanguageFunctionEntry {
    int arity;                              // exact argument count for CALL_FUNC
    void (*fn)(std::vector<Value>& stack, void* hostContext);
};
```

- **`arity`** must match the **`arg-count`** operand on every **`CALL_FUNC`** targeting that slot.
- **`fn == nullptr`** or an out-of-range **`function-id`** yields **`LANG: unknown function`** at runtime.
- Handlers pop arguments from the **top of the stack** (see [`bytecode.md`](bytecode.md)); push exactly one result unless the language defines void calls (not used in M11 BASIC).

### Namespace IDs

Published allocation is in [`include/gemini/namespace_ids.hpp`](../include/gemini/namespace_ids.hpp). Do not reuse IDs assigned to another language. Stub modules may register an empty **`functions`** vector (metadata only) to reserve a namespace for a future compiler.

## Handler signature

```cpp
void handler(std::vector<PickVM::Value> &stack, void *hostContext);
```

- **`stack`**: operand stack; pop **`arity`** values from the top, push return value.
- **`hostContext`**: boot passes the VM **`Runtime`** pointer when loading from **`BootMonitor`**; unit tests may pass **`nullptr`**. BASIC **`SYSTEM`** / **`STATUS`** cast this to **`PickVM::Runtime*`**.

## Building a module

Mirror [`modules/gemini-basic/`](../modules/gemini-basic/):

1. **`add_library(gemini-module-<name> SHARED …)`**
2. **`target_link_libraries(… PRIVATE gemini-core)`** — provides registry types and VM **`Value`**.
3. Export **`register_language`** from a single `.cpp` file.
4. Post-build **copy** the shared library into **`${CMAKE_BINARY_DIR}/src/gemini/modules/`** so bootstrap layout matches runtime.

Register the subdirectory in [`modules/CMakeLists.txt`](../modules/CMakeLists.txt). Add **`add_dependencies(gemini-system gemini-module-<name>)`** in [`src/CMakeLists.txt`](../src/CMakeLists.txt) so the copy runs before the host starts.

## Reference implementation (canonical BASIC module)

**[`gemini-module-basic`](../modules/gemini-basic/)** is the canonical end-to-end example. Use it as a template when adding a new language module.

### File map

| File | Role |
|------|------|
| [`BasicModule.cpp`](../modules/gemini-basic/BasicModule.cpp) | Exports **`register_language`** → calls **`registerBasicLanguage`** |
| [`BasicLanguageRegistration.cpp`](../src/core/languages/basic/BasicLanguageRegistration.cpp) | Builds **`LanguageNamespaceDescriptor`** with dense function table |
| [`BasicBuiltinHandlers.cpp`](../src/core/languages/basic/BasicBuiltinHandlers.cpp) | Handler implementations (pop stack, push result) |
| [`include/gemini/basic_function_ids.hpp`](../include/gemini/basic_function_ids.hpp) | Published stable function IDs for external compilers |
| [`include/gemini/namespace_ids.hpp`](../include/gemini/namespace_ids.hpp) | Namespace ID **`Gemini::kNamespaceIdBasic`** (`2`) |

### Verify at boot and from Tcl

1. Cold start should log **`MODULE libgemini-module-basic.*: OK`** and **`MODULES: N loaded, …`** (see [`gemini-bootstrap.md`](gemini-bootstrap.md)).
2. At the Tcl prompt, **`SYSTEM LANGUAGES`** should include a line like **`2 basic 1 28`** (namespace id, name, version, function count).
3. **`SYSTEM LANGUAGES VERBOSE`** lists arity for each function slot (`0`–`27` for BASIC).

The in-tree BASIC compiler emits **`CALL_FUNC`** using these IDs; see [`bytecode.md`](bytecode.md) and [`basic-language.md`](basic-language.md).

### Stub modules

Stub modules (**`gemini-module-pascal`**, **`-comal`**, **`-cobol`**) register metadata and reserve namespace IDs for future compilers. COMAL and COBOL appear in **`SYSTEM LANGUAGES`** with function count **`0`** until handlers ship.

**Pascal (Apollo):** console function IDs are published in [`include/gemini/pascal_function_ids.hpp`](../include/gemini/pascal_function_ids.hpp) and [`bytecode.md`](bytecode.md) (namespace **`3`**: `write`, `writeln`, `read`, `readln`). The in-tree **`gemini-module-pascal`** stub registers the namespace but has **no handlers** (function count **`0`**). Apollo M5's bootstrap binding emits core `PRINT_*` / `INPUT_*` opcodes, and that module-free path completed [Gemini M19](milestones/19-standalone-vm-runner.md) and Apollo M6. A later codegen binding may emit **`CALL_FUNC`** using the published IDs. **Steady state:** build and ship the Pascal helper module with **apollo-compiler** against this ABI; Gemini remains loader + published IDs, not the permanent home of every outside language’s builtins.

## See also

- [`bytecode.md`](bytecode.md) — **`CALL_FUNC`** encoding, BASIC and Pascal function tables
- [`schemas/language-namespaces.json`](schemas/language-namespaces.json) — machine-readable ID catalogue
- [`milestones/11-multi-language-runtime-infrastructure.md`](milestones/11-multi-language-runtime-infrastructure.md)
- [`milestones/19-standalone-vm-runner.md`](milestones/19-standalone-vm-runner.md) — Pick-independent runner; Pascal module ownership
