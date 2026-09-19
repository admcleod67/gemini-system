← [Project milestones index](../milestones.md)

## Milestone 21 — Shared Math Language Module

Ship a **language-neutral math** `CALL_FUNC` namespace and drop-in module so compiled front-ends (Apollo Pascal first; BASIC and others welcome) can call a locked set of unary real→real transcendentals without duplicating handlers per language. Unblocks Apollo Compiler transcendental binding after [**Milestone 20**](20-vm-console-numeric-ergonomics.md) core numeric ergonomics. *Status: implemented.*

Follow-on Apollo-facing track: host filesystem façade ([**Milestone 22**](22-host-filesystem-facade.md)). R83 gaps are [**Milestone 23**](../compatibility-r83-pick.md) (detail page TBD); CPU fairness is deferred [**Milestone 24**](24-execution-fairness-cpu-bound-yield.md).

**Standing invariant:** `gemini-system`, `gemini-daemon`, `gemini-console`, BASIC built-in `CALL_FUNC` ids, and Pick `MAT_*` / `DIM_ARRAY` behaviour must not regress. Full `ctest` remains green. Prefer a **new namespace + module** over new core opcodes or rewriting the BASIC math ABI in this milestone.

Consumer context: [`docs/apollo-consumer-notes.md`](../apollo-consumer-notes.md) §3 / transcendental note (Stage 1b via module / shared math surface).

---

### 1. Purpose and rationale

Wirth ordinal/arithmetic helpers (`abs`, `trunc`, `sqr`, …) and M20 core ops (`COERCE_FLT`, `MOD`, …) already cover compiler-lowered primitives. **Transcendentals** (`sin`, `sqrt`, …) are language-neutral and previously lived only under the **basic** namespace (partial overlap). Apollo has locked a six-function real→real Pascal set (`sqrt`/`sin`/`cos`/`arctan`/`ln`/`exp`) and should not reimplement `std::sin` under Pascal namespace **3**. The shared module also includes **`Tan`** so BASIC’s existing `TAN` and a complete trig surface share one implementation; Apollo need not bind `Tan`.

A shared **math** module gives one ABI, one test surface, and one error contract. Language modules stay for dialect behaviour (Pascal I/O formatting; BASIC Pick string/`OCONV` builtins).

---

### 2. Scope

#### 2.1 Locked function set (v1)

All functions are **unary real → real**: pop one `Value`, coerce to `double`, push `double`. Angles in **radians**.

| ID | Published name | C++ mapping | Domain / errors (stable `MATH:` prefix) |
|----|----------------|-------------|----------------------------------------|
| 0 | `Sqrt` | `std::sqrt` | Negative → `MATH: SQRT domain` |
| 1 | `Sin` | `std::sin` | (no domain reject) |
| 2 | `Cos` | `std::cos` | (no domain reject) |
| 3 | `Tan` | `std::tan` | (no domain reject; poles may yield ±Inf per IEEE) |
| 4 | `Arctan` | `std::atan` | (no domain reject) |
| 5 | `Ln` | `std::log` | ≤ 0 → `MATH: LN domain` |
| 6 | `Exp` | `std::exp` | Overflow may yield ±Inf per IEEE; do not throw solely for overflow |

Exact function-id integers and namespace id are locked in [`language-namespaces.json`](../schemas/language-namespaces.json) and [`include/gemini/math_function_ids.hpp`](../../include/gemini/math_function_ids.hpp) (namespace id **6** / `0x00000006`, name `math`, after cobol **5**).

Stack order for `CALL_FUNC`: same as today — last argument on top; arity **1**.

#### 2.2 In scope

- Shared library module **`gemini-module-math`** registering namespace `math`
- Schema + C++ ID header; additive namespace under existing ABI version
- Unit tests: happy paths; `Sqrt`/`Ln` domain errors; module load via `gemini-vm --modules`
- Docs: [`bytecode.md`](../bytecode.md) / [`language-modules.md`](../language-modules.md) / consumer-notes pointer; hub status

#### 2.3 Hard non-goals

- New **core opcodes** for these seven functions
- Migrating BASIC `SIN`/`COS`/`TAN`/`EXP`/`LOG` ids onto math in this milestone (optional thin forward is follow-on)
- Pascal dialect I/O field widths / `eof`/`eoln` ([M22](22-host-filesystem-facade.md) and Pascal module tracks)
- Host filesystem façade ([**Milestone 22**](22-host-filesystem-facade.md))
- R83 gaps ([**Milestone 23**](../compatibility-r83-pick.md)) or CPU fairness ([**Milestone 24**](24-execution-fairness-cpu-bound-yield.md))
- Requiring Apollo to emit `CALL_FUNC math,…` in the same Gemini release

---

### 3. Compatibility

- **BASIC / Pick:** existing programs keep emitting basic-namespace math; behaviour unchanged unless an explicit later emit switch is chosen.
- **Apollo:** success when Gemini documents ids/errors and tests prove the seven ops; Apollo binds its six (`sqrt`/`sin`/`cos`/`arctan`/`ln`/`exp`) afterwards and may ignore `Tan`.
- **Standalone runner:** load math with existing `--modules` / default module path; no new CLI required.

---

### 4. Deliverables

| Area | Artifact |
|------|----------|
| Module | [`modules/gemini-math/`](../../modules/gemini-math/) + CMake + bootstrap copy |
| IDs | [`include/gemini/math_function_ids.hpp`](../../include/gemini/math_function_ids.hpp); schema row; `namespace_ids.hpp` constant |
| Tests | Runtime/`CALL_FUNC` cases in `test_LanguageModuleLoader.cpp` |
| Docs | Schema, bytecode/language-modules notes, consumer-notes status, hub |

---

### 5. Milestone completion criteria

- [x] Namespace `math` registered; seven functions implemented, documented, and tested
- [x] Domain errors use a stable `MATH:` prefix; radians documented
- [x] BASIC math `CALL_FUNC` paths unchanged in intent; full `ctest` green
- [x] Consumer notes / hub list M21 implemented when closed

---

### 6. Suggested implementation stages

1. **IDs + schema + empty module registration** — publish namespace/function table. *Status: implemented.*
2. **Handlers + unit tests** — seven functions and domain cases. *Status: implemented.*
3. **Docs + closes M21** — bytecode/language-modules/consumer-notes/hub. **Closes Milestone 21.** *Status: implemented.*

Only Stage 3 claims “Closes Milestone 21.”

---

### 7. Follow-on (beyond M21)

- Apollo binds Pascal transcendentals to `CALL_FUNC math,…` (six-function subset)
- Optional BASIC thin wrappers or emit migration onto math (preserve old ids)
- Host filesystem façade — [**Milestone 22**](22-host-filesystem-facade.md)
- R83 compatibility — [**Milestone 23**](../compatibility-r83-pick.md) (detail page TBD)
- CPU-bound cooperative yield — [**Milestone 24**](24-execution-fairness-cpu-bound-yield.md)

*Status: implemented.*
