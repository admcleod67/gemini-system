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

## Reference implementation

**[`gemini-module-basic`](../modules/gemini-basic/)** is the canonical example:

- Namespace **`Gemini::kNamespaceIdBasic`** (`2`)
- 28 handlers in [`BasicBuiltinHandlers.cpp`](../src/core/languages/basic/BasicBuiltinHandlers.cpp)
- Registration in [`BasicLanguageRegistration.cpp`](../src/core/languages/basic/BasicLanguageRegistration.cpp)
- Function IDs in [`include/gemini/basic_function_ids.hpp`](../include/gemini/basic_function_ids.hpp)

Stub modules (**`gemini-module-pascal`**, **`-comal`**, **`-cobol`**) register metadata only and reserve namespace IDs for future compilers.

## See also

- [`bytecode.md`](bytecode.md) — **`CALL_FUNC`** encoding and BASIC function table
- [`schemas/language-namespaces.json`](schemas/language-namespaces.json) — machine-readable ID catalogue
- [`milestones/11-multi-language-runtime-infrastructure.md`](milestones/11-multi-language-runtime-infrastructure.md)
