# Correlatives and computed attributes

Companion to [docs/english.md](english.md), [docs/dict.md](dict.md), [docs/basic-file-io.md](basic-file-io.md), and [Milestone 9](milestones/09-correlatives-computed-attributes.md).

## F-type DICT items (three-attribute layout)

| Attribute | Content |
|-----------|---------|
| 1 | `F` (type code) |
| 2 | Source attribute number (positive integer) |
| 3 | Selector or conversion |

Attribute 3 is interpreted as:

| Attr 3 value | Meaning |
|--------------|---------|
| Positive integer (`3`, …) | Value index (1-based) on the source attribute |
| `L` | Leftmost value |
| `R` | Rightmost value |
| Other non-empty text | **OCONV** conversion code applied to the **first value** of the source attribute |

Documentation shorthand `F;2;3` means attr 2 = `2`, attr 3 = `3` (third value), not a semicolon program in one field.

## OCONV codes (F-type attr 3 and BASIC)

F-type conversion uses the same **output** conversion table as BASIC `OCONV` (Milestone 7). Codes must match **exactly** (case-insensitive); composite Pick codes such as `D2/` are not parsed yet.

| Code | Output |
|------|--------|
| `D` | Pick internal day → `dd MMM yyyy` |
| `MT` | Seconds since midnight → `HH:MM` |
| `MTS` | Seconds since midnight → `HH:MM:SS` |
| `MCU` | Uppercase |
| `MCL` | Lowercase |
| `MD` | Integer display |
| `MD2` | Implicit two decimal places (`1234` → `12.34`) |

See [docs/basic-language.md](basic-language.md) for BASIC builtin behaviour.

Unsupported codes produce `F-type: unsupported conversion code "<code>"` when evaluated from ENGLISH F-type fields.

## ENGLISH usage

`LIST`, `SORT`, and related verbs resolve F-type and I-type field tokens through the DICT and evaluate them per row. Use `RESOLVE-FIELD <file> <token>` to inspect a definition; use `LIST-DICT <dict-file>` to list all items with type and validity.

## I-type DICT items (two-attribute layout)

| Attribute | Content |
|-----------|---------|
| 1 | `I` (type code) |
| 2 | Expression source (non-empty) |

Documentation shorthand `I;A + B` means attr 2 = `A + B`, not a semicolon program in one field.

### Supported expressions (Stage 4–5)

| Construct | Meaning |
|-----------|---------|
| `A`–`Z` (case-insensitive) | First value of attribute 1–26 on the **data** record (`A` = attribute 1) |
| Integer / decimal literals | e.g. `123`, `12.34` |
| `"text"` string literals | Used in `IF` branches and conversion codes |
| `+` `-` `*` `/` | Arithmetic; `*` `/` bind tighter than `+` `-` |
| `>`, `<`, `>=`, `<=`, `=`, `<>`, `#` | Comparisons (numeric when both sides coerce, else lexicographic) |
| `IF cond THEN expr ELSE expr` | Conditional (condition is a comparison or numeric truth) |
| `OCONV(value, code)` | Output conversion (M7 table) |
| `ICONV(value, code)` | Input conversion (M7 table) |
| Unary `-` | Negation |
| `( … )` | Grouping |

Empty field values coerce to **0** in numeric contexts. Non-numeric text in a numeric context yields `I-type: type mismatch`. Division by zero yields `I-type: division by zero`.

Example: `IF A > 10 THEN "HIGH" ELSE "LOW"` (DICT attr 2 = that expression).

### ICONV codes (I-type and BASIC)

| Code | Input → internal |
|------|------------------|
| `D` | `dd MMM yyyy` → Pick internal day |
| `MT` / `MTS` | `HH:MM` / `HH:MM:SS` → seconds since midnight |
| `MCU` / `MCL` | Case change (string) |
| `MD` / `MD2` | Display decimal → scaled integer |

Unsupported codes: `I-type: unsupported conversion code "…"`. Invalid input: `I-type: invalid ICONV "…" input`.

### BASIC READV on computed fields

`READV var FROM filevar, id, DICT<TOKEN>` or `READV var FROM filevar, id, "TOKEN"` evaluates **F-type** and **I-type** DICT items via the same correlative engine as ENGLISH. **A-type** behaviour is unchanged (attribute index). Value index on computed fields is rejected (`SUBVALUE.NOT.FOUND`).

### DICT authoring note

In **`EDIT`**, the command **`I`** means **INSERT**, not the I-type code. To create an I-type item use **`INSERT`**, then type **`I`**, the expression, and **`.`** on its own line, then **`SAVE`**.

### Not yet supported

- `DATE()` and other functions beyond `ICONV` / `OCONV`
- `WRITEV` on F/I computed fields
- `REC<"FIELDNAME">` (optional stretch)

Use `RESOLVE-FIELD <file> <token>` to inspect an I-type definition (`Field kind: I`, `Expression: …`). Invalid rows show `Validity: INVALID` and a parse error. See [docs/dict.md](dict.md) for DICT authoring and introspection.
