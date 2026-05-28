# Correlatives and computed attributes

Companion to [docs/english.md](english.md) and [Milestone 9](milestones/09-correlatives-computed-attributes.md).

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

`LIST`, `SORT`, and related verbs resolve F-type field tokens through the DICT and evaluate them per row. Use `RESOLVE-FIELD <file> <token>` in the Tcl shell to inspect a definition.

I-type computed attributes and `ICONV` in F tails are planned for later Milestone 9 stages.
