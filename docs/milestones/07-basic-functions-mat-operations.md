← [Project milestones index](../milestones.md)

## Milestone 7 — BASIC Functions & MAT Operations

Milestone 7 expands the BASIC language with a substantial set of Pick‑authentic built‑in functions, improved string and numeric operations, and the first slice of MAT‑family behaviour. This milestone deepens BASIC’s expressive power without altering the VM architecture, relying instead on the stable variable, expression, and file semantics delivered in Milestone 4. The goal is to bring BASIC closer to everyday usability for real Pick programs by adding function calls, richer coercion rules, string utilities, numeric conversions, and minimal MAT READ/MAT WRITE support. This milestone lays the groundwork for later formatting, correlatives, and full MAT semantics while keeping the implementation surface tightly scoped to the BASIC compiler and runtime.

### Goals

- Introduce core Pick BASIC built‑in functions (string, numeric, conversion, and utility functions).
- Add function‑call syntax and runtime support without expanding the VM opcode set.
- Improve string and numeric coercion rules to match Pick semantics consistently across expressions, built‑ins, **`PRINT`**, and **`INPUT`** paths (building on coercion work already grounded in Milestone 4 arithmetic).
- Implement minimal MAT operations (**`MAT READ`**, **`MAT WRITE`**, **`MAT`** assignment and scalar initialization) sufficient for early matrix‑style programs.
- Preserve architectural cleanliness: BASIC evolves, but the VM and filesystem remain unchanged in this milestone (no new opcodes unless an unavoidable hotspot is justified and documented — default path is lowering to existing VM operations plus runtime helpers).
- Maintain backward compatibility with existing BASIC programs.

### Scope

#### 1. Built‑in BASIC functions (core set)

Add a Pick‑authentic subset of the most widely used functions — names, arity, and edge cases tracked against classic Pick BASIC references during implementation:

- **String:** **`LEN`**, **`TRIM`**, **`LCASE`**, **`UCASE`**, **`INDEX`**, **`FIELD`**, **`SEQ`**, **`SPACE`**, **`STR`**, **`CONVERT`** (behaviour clarified per function: delimiter rules, trimming policy, **`CONVERT`** control tables).
- **Numeric:** **`ABS`**, **`INT`**, **`MOD`**, **`RND`**, **`SIN`**, **`COS`**, **`TAN`**, **`EXP`**, **`LOG`** (ranges, domains, error handling for overflow/invalid args).
- **Conversion:** **`NUM`**, **`ICONV`**, **`OCONV`** — **minimal** first pass backed by a small, explicit conversion table (easy to extend later; document supported codes and rejects).
- **Utility:** **`DATE`**, **`TIME`**, **`SYSTEM(n)`** — **minimal** subset (enumerate supported **`n`** values and behaviours; stubs or host‑backed timestamps as documented).

Function calls use standard Pick syntax (**`X = LEN(A)`**, **`IF INDEX(S, "/", 2) ...`**). The compiler **lowers** calls to existing VM arithmetic/string/move operations wherever practical, and otherwise dispatches through thin runtime evaluator entry points that **reuse** (`do not duplicate`) coercion and value‑shape rules established in Milestone 4.

#### 2. Function‑call semantics

- Parser support for **`NAME(expr [, expr …])`** forms in expressions and **`LET`**‑style assignments.
- Argument evaluation order, short‑circuit boundaries (document interaction with **`AND`** / **`OR`** if any divergence from Pick).
- Argument type coercion, optional vs required parameters per function signature, return‑type rules (dynamic vs narrowed types for `%`/`$`/`#`/`!` conventions).
- Return‑value plumbing through assignment, **`PRINT`**, comparisons, and nested calls.
- **No new VM opcodes required by default:** implement as bytecode patterns or centralized runtime helpers wired from existing primitives; if a narrowly scoped opcode is unavoidable, justify it explicitly in delivery notes.

#### 3. String and numeric semantics

- **Coercion refinement** aligned with Pick where Milestone 4 arithmetic already established baselines — extend **consistency** into function pipelines:
  - Numeric‑prefix extraction (e.g. `"12ABC"` → **`12`** for numeric contexts).
  - Empty string → **`0`** in numeric contexts unless a particular function mandates different Pick rules.
  - **Mixed‑type comparisons** remain hard errors unless Pick specifies otherwise (**existing Milestone 4 rule preserved unless broadened deliberately with tests**).
- **String slicing / substring helpers** consistent with **`FIELD`**, **`INDEX`**, and substring semantics required by the chosen function subset (reject ambiguous overlaps with existing string operators unless Pick‑congruent).
- Ensure **`PRINT`** and **`INPUT`** paths consume function results cleanly (delimiter behaviour, prompting, trailing space/newline parity with current BASIC runtime).

#### 4. Minimal MAT operations

Introduce the first slice of MAT‑family behaviour (**no matrix arithmetic** in this milestone):

- **`MAT READ`** / **`MAT WRITE`** — read/write whole **`DIM`** arrays from/to structured records using the Milestone 4 structured record model; define dimension vs record‑shape correspondence and error wording for truncation/overflow/mismatch.
- **`MAT A = MAT B`** — element‑wise copy with **dimension checking** (`DIM` count must match unless a deliberately Pick‑narrow rule is documented).
- **Scalar MAT initialization:** **`MAT A = 0`**, **`MAT A = ""`** (typed empty vs zero clarified per BASIC variable type).
- **Out of scope for M7:** **`MAT A = MAT B + MAT C`** and higher‑order linear algebra (**see Non‑goals**).

#### 5. Compiler and runtime enhancements

- Extend the **expression parser** (`BasicExpressionParser` and related grammar) for function calls and MAT statement forms (`BasicStatementParser` / statement lowering as appropriate).
- Add **runtime helpers** for centralized function dispatch and MAT file/array marshalling (thin layer over **`PickVM::Runtime`** and existing file ops — no parallel file API).
- **Diagnostics:** malformed calls, wrong argument counts, domain errors, **`CONVERT`** rejects, **`MAT`** dimension mismatches — messages stable, test‑locked, and aligned with Pick BASIC phrasing where a canonical string exists.

### Non‑Goals (Deferred)

- Full MAT arithmetic (`MAT A = MAT B + MAT C`, etc.).
- **`PRINT USING`** and advanced formatting (may remain Milestone 8+ unless explicitly pulled forward).
- Correlative execution (F/I‑type DICT items) — Milestone 9 trajectory.
- ENGLISH formatting/reporting layer.
- **VM opcode expansion** as a blanket solution — prefer lowering; defer broad opcode redesign.
- Multi‑user semantics or record locking.
- BASIC debugger/trace facilities beyond what already ships in **`ASM`**/`BASIC` shells.

These belong to later milestones.

### Rationale

Pick BASIC’s power comes from its rich library of built‑in functions and MAT operations. Milestone 7 brings Gemini closer to real‑world Pick workloads by adding these capabilities while preserving architectural boundaries: the VM stays stable relative to semantic surface area, the filesystem model stays anchored in Milestones 3–4, and BASIC gains depth through compiler and runtime enhancements alone. This milestone provides the functional substrate required for later formatting, correlatives, and full MAT semantics, and significantly improves the day‑to‑day usability of BASIC programs within the Gemini system.

**Later alignment:** [Milestone 11 — Multi‑Language Runtime Infrastructure](11-multi-language-runtime-infrastructure.md) introduces **`CALL_FUNC`**, a **`LanguageRegistry`**, and dynamic **`modules/`** loading; Milestone 7 built‑ins are **migrated** to registry-backed dispatch **without** changing BASIC source compatibility. Published namespace and function IDs: [`include/gemini/basic_function_ids.hpp`](../../include/gemini/basic_function_ids.hpp) and [`docs/bytecode.md`](../bytecode.md).

