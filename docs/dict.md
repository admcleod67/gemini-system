# DICT dictionary items

Companion to [ENGLISH query core](english.md), [Correlatives and computed attributes](correlatives.md), and [Developer shell (TCL)](tcl-shell.md).

## Lookup precedence

When ENGLISH, BASIC, or Tcl resolve a field token against data file **`DATA`**:

1. **Numeric token** — treated as a 1-based attribute number on the data record (no DICT lookup).
2. **File-scoped dictionary** — logical file **`DICT-DATA`** when it exists.
3. **Global dictionary** — logical file **`DICT`**.

Within a dictionary record, type **`S`** (synonym) follows attribute 2 to the next dictionary id using the same file-scoped-then-global rule (depth limit 16).

## Item types

| Type | Attribute 1 | Attribute 2 | Attribute 3+ |
|------|-------------|-------------|--------------|
| **A** | `A` | Data attribute number (positive integer) | Optional conversion hints on attrs 7–10 (`D`, `MD`, `MC`) |
| **S** | `S` | Target dictionary id | — |
| **F** | `F` | Source attribute number | Selector or OCONV code (see [correlatives.md](correlatives.md)) |
| **I** | `I` | Expression source | — |

Structured definitions are parsed at resolution time; invalid F/I rows are rejected by ENGLISH with **`Unknown ENGLISH field`** unless diagnosed via Tcl (below).

## Authoring

### Type A (Tcl helper)

**`DEFINE-FIELD <dict-file> <field-name> <attribute-number>`** writes a minimal A-type item (four Tcl tokens). The dictionary file must exist (**`CREATE-FILE`** first).

Example:

```text
CREATE-FILE DICT
DEFINE-FIELD DICT NAME 1
```

For file-scoped dictionaries: **`CREATE-FILE DICT-DATA`** then **`DEFINE-FIELD DICT-DATA NAME 1`**.

### Types F and I (line editor)

Use **`EDIT DICT <field-name>`** (or **`EDIT DICT-DATA <field-name>`**) to author multi-line F/I records.

**Important:** at the **`ED>`** prompt, **`I`** is the **INSERT** command alias, not the I-type code. To create an I-type item:

```text
EDIT DICT NET
INSERT
I
A + B
.
SAVE
QUIT
```

Wrong pattern: typing **`I`** alone at **`ED>`** starts INSERT and saves only the expression line as attribute 1 → **`Field kind: unknown`** / **`LIST-DICT … INVALID`**.

For F-type items, attribute 1 = **`F`**, attribute 2 = source attribute number, attribute 3 = value index, **`L`**, **`R`**, or OCONV code.

See [Correlatives and computed attributes](correlatives.md) for F/I syntax and evaluation behaviour.

## Introspection (Tcl)

| Command | Purpose |
|---------|---------|
| **`LIST-DICT <dict-file>`** | Sorted listing: `<id> <type> VALID\|INVALID` per DICT record |
| **`RESOLVE-FIELD <data-file> <token>`** | How ENGLISH resolves the token; invalid F/I rows show **`Validity: INVALID`** and parse errors |

Examples:

```text
LIST-DICT DICT
RESOLVE-FIELD DATA NET
RESOLVE-FIELD DATA BAD
```

## See also

- [Correlatives and computed attributes](correlatives.md) — F/I evaluation, OCONV/ICONV, BASIC READV
- [BASIC file I/O (DICT-aware reads)](basic-file-io.md)
- [ENGLISH query core](english.md) — projection, sort, and formatting on computed fields
