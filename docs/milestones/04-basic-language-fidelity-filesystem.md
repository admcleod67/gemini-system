← [Project milestones index](../milestones.md)

## Milestone 4 — BASIC language fidelity & filesystem maturation

Milestone 4 deepens the **BASIC** subsystem into a more Pick-authentic language, building on the foundation laid by
Milestones 1–3. It expands BASIC’s file semantics, numeric/string behaviour, and multi-value awareness, and introduces
filesystem enhancements required to support those features.

Milestone 4 is intentionally **BASIC-focused**: Tcl/VOC ergonomics, ENGLISH formatting, and PROC polish are deferred to
Milestone 5.

### Goals

- Deliver a substantially more complete Pick BASIC, including attribute-level and multi-value file operations.
- Mature the filesystem beyond M3 to support cursor state, attribute-level writes, and multi-value mutation.
- Introduce floating-point numeric semantics and improved coercion rules.
- Preserve architectural cleanliness: BASIC drives filesystem evolution, not the other way around.
- Maintain backward compatibility with existing BASIC programs and VM behaviour.

### Scope

### Stage 1 status

- Implemented: `READNEXT`, `READV`, `WRITEV` statement/opcode/runtime paths.
- Implemented: `READNEXT` cursor semantics (reset on `OPEN`, invalidated on `WRITE`/`WRITEV`, lexicographic record order).
- Implemented: no BASIC file-creation opcode/statement; missing-file behavior follows `ELSE`/raise contract.
- Implemented: filesystem helpers for attribute/subvalue mutation while preserving surrounding record data.

### Stage 2 status

- Implemented: floating-point literal parsing in BASIC expressions (decimal + scientific forms).
- Implemented: VM value/opcode support for float constants and mixed int/float arithmetic.
- Implemented: numeric-prefix string coercion for arithmetic (`\"\" -> 0`, `"12ABC" -> 12` style).
- Implemented: clean float rendering through `PRINT` value output path.

### Stage 3 status

- Implemented: BASIC expression-level angle-bracket reads (`REC<attr>` / `REC<attr,valueIndex>`) with parser disambiguation from `<` comparison.
- Implemented: dedicated VM extraction opcode path backed by `StructuredRecord` and `RecordAttribute` (no duplicate multi-value parsing rules).
- Implemented: shared multi-value join helper reused by runtime write paths to keep BASIC/ENGLISH/DICT splitting consistent.
- Implemented: documentation clarifying that `DIM` arrays and record multi-values are distinct models (no implicit aliasing).

### Stage 4 status

#### 1. BASIC file semantics (Pick-authentic core)

- **`READNEXT`**
  - Add per-file cursor state.
  - Cursor resets on `OPEN`.
  - Cursor invalidated on `WRITE`.
  - Stable iteration order (lexicographic record ID).
- **`READV` / `WRITEV`**
  - Attribute-level read/write using the M3 structured attribute model.
  - Support multi-value indexing (e.g. `READV var FROM F, id, attr, valueIndex`).
  - Preserve all other attributes and values on write.
- **Improved `ELSE` semantics**
  - Align `OPEN`, `READ`, `WRITE`, `READNEXT`, `READV`, `WRITEV` with Pick BASIC’s `ELSE` behaviour.
  - File-missing policy: BASIC does not implicitly create missing logical files; when a required logical file is missing, the operation routes to the `ELSE` clause or raises (depending on the existing BASIC ELSE design).
  - Inline statement or line target; single-statement rule preserved.
  - File creation remains available only via Tcl (for example, `CREATE-FILE`), not via BASIC statements/opcodes.
- **Optional Pick-style angle-bracket syntax**
  - Implemented: `REC<attr>` and `REC<attr,value>` as syntactic sugar for multi-value reads (`READV`).
  - Compiler disambiguation prevents collisions with `<` comparisons.

#### 2. Numeric & string semantics

- **Floating-point literals**
  - Add decimal literals (`3.14`, `0.5`, `1E3`).
  - Extend expression parser and VM arithmetic.
- **Floating-point arithmetic**
  - Add VM opcodes or extend existing arithmetic to support double precision.
  - Preserve integer coercion rules for `%` variables.
- **Improved coercion rules**
  - String → numeric coercion matches Pick semantics (`\"\"` → `0`, non-numeric prefix allowed).
  - Mixed-type comparisons raise runtime errors (existing rule preserved).
- **Minimal `PRINT` formatting**
  - Support `PRINT USING` (optional stretch).
  - At minimum, ensure floats print cleanly.

#### 3. Multi-value awareness

- Structured multi-value access
  - BASIC expressions can read multi-values via `READV` or angle-bracket syntax.
  - Multi-value writes preserve surrounding values.
- Array interoperability
  - Clarify semantics between `DIM` arrays and multi-value attributes (remain distinct).
- Consistent multi-value splitting
  - Use the same splitter as ENGLISH and DICT (M3 contract).

#### 4. Filesystem enhancements (driven by BASIC)

- **File cursor state**
  - Implemented: cursor state is stored on `PickFS::FileSystem::FileHandle`.
  - `OPEN` resets cursor state; `READNEXT` advances it; `WRITE`/`WRITEV` invalidate it by resetting the cursor.
- **Attribute-level write API**
  - Implemented: filesystem can update a single attribute while preserving the surrounding record data.
- **Multi-value write API**
  - Implemented: filesystem can update a specific subvalue (by value index) while preserving sibling multi-values.
- **Improved error taxonomy**
  - Implemented: classified runtime IO failures (missing file/record/cursor exhaustion, plus attribute/subvalue presence).
  - Presence-sensitive `READV`/`READV_TRY`:
    - `READV` throws `ATTRIBUTE.NOT.FOUND` / `SUBVALUE.NOT.FOUND`
    - `READV ... ELSE` treats missing attribute/subvalue as failure (`flag=0`) and routes to `ELSE`
- **Optional DICT-aware helpers**
  - Allow BASIC to resolve attribute names via DICT (stretch goal).

#### 5. BASIC language polish

- Prompted `INPUT`
  - Implemented: `INPUT \"Prompt\", var` in addition to `INPUT var`.
  - Implemented: compiler lowers prompted form to `PRINT` (no newline) + `INPUT_STR`.
- `CHAIN` (minimal)
  - Implemented: `CHAIN <programExpr>` transfers control to another BASIC program.
  - Implemented: no parameter passing in this milestone.
- Improved diagnostics
  - Implemented: precise compile-time diagnostics for malformed file statements and prompted-INPUT/CHAIN forms.
  - Implemented: classified runtime wording for file/control-flow failures remains stable under regression tests.
- R83-authentic behaviour refinements
  - Implemented: tightened `FOR`/`NEXT` mismatch messaging and `GOSUB`/`RETURN` runtime wording.
  - Implemented: `STOP`/`END` remain equivalent halt semantics, including implicit-end behavior.

### Non-goals (deferred)

- Tcl tokenisation and quoting improvements
- VOC authoring commands (CREATE-VOC, DELETE-VOC, etc.)
- ENGLISH `WITH`-clause evaluation
- ENGLISH formatting (`HEADING`, `BREAK-ON`, `TOTAL`, pagination)
- PROC language enhancements
- Correlative execution (F/I-type DICT items)
- Record locking (`READU`, `WRITEU`, `RELEASE`)
- Multi-user concurrency
- Report writer subsystem
- Full BASIC `MAT` operations (future milestone)

