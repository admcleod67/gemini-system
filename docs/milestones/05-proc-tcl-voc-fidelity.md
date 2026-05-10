← [Project milestones index](../milestones.md)

## Milestone 5 — PROC, TCL & VOC Fidelity Pass

This milestone focuses on broadening the Gemini System by bringing the PROC language, TCL shell ergonomics, and VOC behaviour up to Pick-authentic levels. With BASIC and the filesystem now significantly matured in Milestone 4, Milestone 5 strengthens the horizontal platform: scripting, automation, command dispatch, and dictionary-driven behaviour. This milestone does not extend BASIC semantics; instead it balances the system by elevating PROC and TCL to the same level of fidelity as BASIC and ENGLISH.

### Goals

- Deliver a substantially more complete PROC interpreter, supporting the core R83 control-flow and scripting constructs.
- Improve TCL shell ergonomics, including quoting, tokenisation, and error consistency.
- Expand VOC authoring and introspection, enabling Pick-authentic dictionary management from within the system.
- Strengthen the command-layer foundation for future milestones (formatting/reporting, correlatives, MAT operations).
- Maintain architectural cleanliness: PROC and TCL remain host-level languages; BASIC and VM remain unchanged in this milestone.

---

### Scope

#### 1. PROC Language Fidelity (core scripting features)

- **IF / THEN / ELSE**
  - Full Pick-style conditional syntax.
  - Support for inline and multi-token THEN/ELSE bodies.
- **GO / RETURN / labels**
  - Label resolution improvements.
  - `RETURN` is a flat control-transfer exit from the current PROC script.
  - Authentic error behaviour for missing labels and invalid jumps.
- **LOOP / REPEAT**
  - Minimal loop construct with EXIT and conditional EXITIF.
- **SELECT / READNEXT integration**
  - Native PROC `SELECT`/`READNEXT` statements integrate with session active-list APIs.
  - Stage 1 scope: simple `SELECT <file>` and record-id iteration; advanced selection expressions are deferred.
- **TCL bridging improvements**
  - Multi-token TCL commands.
  - Better variable substitution rules.
  - Support for multi-line TCL blocks (optional stretch).
- **R83 compatibility aliases + canonical forms**
  - PROC accepts both original one-letter R83 command forms and modern long-form synonyms.
  - Long-form names are canonical for docs, diagnostics, and internal command identity; one-letter forms are aliases.
- **Token/substitution contract (normative)**
  - The first token is always parsed as the command keyword and is never variable-substituted.
  - Substitution applies only to operand tokens, only on exact token-name matches, with no quoting/escaping/partial-match behavior.
  - PROC does not support dynamic command construction from variable values.
- **Argument handling**
  - Positional parameters P1, P2, ...
  - Defaulting behaviour and error messages aligned with Pick.
- **Error handling**
  - Consistent runtime errors for undefined variables, missing labels, and malformed statements.
  - Preserve assignment/keyword disambiguation under alias support (for example, one-letter command aliases must not change assignment parsing rules).

---

#### 2. TCL Shell Ergonomics & Tokenisation

- **Quoting rules**
  - Support "quoted strings" and escaped characters.
  - Preserve empty tokens when quoted.
- **Tokenisation improvements**
  - More predictable whitespace handling.
  - Consistent behaviour across ECHO, SET, WRITE, PROC TCL lines.
- **Error consistency**
  - Harmonise arity and syntax error messages across commands.
  - Improve diagnostics for malformed LIST/SELECT/SORT lines.
- **$-substitution refinements**
  - More authentic Pick-style variable expansion.
  - Preserve existing session variable behaviour (@USERNO, @ACCOUNT, etc.).

---

#### 3. VOC Authoring & Introspection

- **CREATE-VOC / DELETE-VOC**
  - Minimal commands to create and remove VOC entries.
  - Validation of A, D, F, Q, V, X types.
- **LIST-VOC**
  - Enumerate VOC entries with type and target information.
- **Improved RESOLVE-FIELD diagnostics**
  - Clearer output for DICT and DICT- resolution.
  - Better error messages for malformed or missing dictionary items.
- **Structured VOC editing**
  - Optional helper commands to generate correct attribute layouts.
  - Raw EDIT remains available for full control.

### Milestone 5 Stage 3 delivery status (implemented)

Stage 3 VOC delivery is implemented in-tree: `CREATE-VOC`, `DELETE-VOC`, and `LIST-VOC` are available with strict arity diagnostics, `A/D/F/Q/V/X` type validation, canonical attribute-per-line writes, and resolver cache invalidation on mutation.

`LIST-VOC` now emits deterministic machine-parseable lines in `<item-id> <type>` format, with malformed entries reported as `INVALID`.

---

#### 4. Command-Layer Polish

- **Help system improvements**
  - HELP <command>
  - HELP PROC, HELP TCL, HELP VOC
  - Short descriptions and usage examples.
- **Versioning and introspection**
  - Add a SYSTEM or ABOUT command for environment metadata.
  - Clarify milestone/version boundaries in output.
- **Bootstrap diagnostics**
  - Better error messages for missing VOC, MD, or account directories.
  - More robust handling of malformed catalogue entries.

### Milestone 5 Stage 4 delivery status (implemented)

Stage 4 command-layer polish is implemented in-tree: `HELP` supports no-arg listing, per-command lookup, and topic help (`PROC`, `TCL`, `VOC`) with deterministic topic-first precedence and stable unknown-help diagnostics.

`SYSTEM` is available as structured environment introspection with `ABOUT` as a literal alias, while `VERSION` output remains unchanged.

Bootstrap/login diagnostics now distinguish missing versus malformed catalogue states and surface account root / `VOC` / `MD` attachment issues without changing intended login/REPL control flow.

---

### Non-Goals (Deferred)

- BASIC language extensions (functions, MAT operations, PRINT USING, CHAIN).
- ENGLISH formatting/reporting (HEADING, BREAK-ON, TOTAL, pagination).
- Correlative execution (F/I-type DICT items).
- Multi-user concurrency and record locking (READU, WRITEU, RELEASE).
- TCL pipeline semantics or shell scripting extensions.
- VM opcode expansion (beyond what PROC requires for control flow).

