# BASIC file I/O (DICT-aware reads)

Companion to [BASIC language](basic-language.md), [DICT dictionary items](dict.md), and [Correlatives and computed attributes](correlatives.md).

## READV attribute forms

```basic
READV var FROM filevar, id, 1
READV var FROM filevar, id, DICT<TOKEN>
READV var FROM filevar, id, "TOKEN"
```

| Attribute expression | Behaviour |
|---------------------|-----------|
| **Numeric** (e.g. `1`, `2`) | Read that 1-based attribute directly from the **data** record (unchanged from Milestone 4). |
| **`DICT<token>`** or **quoted name** | Resolve **`token`** through **`DICT-<file>`** then **`DICT`**; evaluate **A**, **F**, and **I** items via the shared correlative engine. |

F-type and I-type fields return a **single computed cell** (first-value semantics). A value index on computed fields is rejected with **`SUBVALUE.NOT.FOUND`**.

## Evaluation engine

Computed fields use the same **`CorrelativeEvaluator`** as ENGLISH:

- **F-type** — value index, leftmost/rightmost, or OCONV on the source attribute (Milestone 7 conversion subset).
- **I-type** — arithmetic, comparisons, **`IF … THEN … ELSE …`**, **`ICONV()`** / **`OCONV()`** calls.

See [correlatives.md](correlatives.md) for supported expression constructs and error messages.

## Presence and ELSE

- **`READV`** throws **`ATTRIBUTE.NOT.FOUND`** / **`SUBVALUE.NOT.FOUND`** when the attribute or subvalue is missing.
- **`READV … ELSE`** routes classified failures to the **`ELSE`** clause (via the **`READV_TRY`** opcode path).
- Unknown DICT tokens behave as missing attributes.

## Not yet supported

- **`WRITEV`** on F-type or I-type computed fields
- **`REC<"FIELDNAME">`** angle-bracket DICT field syntax (optional stretch)
- Full ICONV/OCONV conversion table beyond Milestone 7 subset

## Example

DICT item **`NET`** (I-type, expression `A + B`):

```basic
OPEN "DATA" TO F ELSE STOP
READV TOTAL FROM F, "R1", DICT<NET>
PRINT TOTAL
```

Or with a quoted field name:

```basic
READV TOTAL FROM F, "R1", "NET"
```

Verify the DICT definition with Tcl **`RESOLVE-FIELD DATA NET`** or **`LIST-DICT DICT`**.

## See also

- [BASIC language](basic-language.md) — full READV/WRITEV/OPEN semantics
- [DICT dictionary items](dict.md) — item types and authoring
- [File system](filesystem.md) — logical files and records
