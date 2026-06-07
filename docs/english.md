# ENGLISH query core

This page documents the implemented ENGLISH subset (**M3b** projection, **M3c** **`SORT`** / active-list scope, and **DICT-<file>** / **`DICT`** lookup precedence).

For roadmap context, see [Project milestones](milestones.md).  
For the attribute record model, see [Filesystem M3 model](filesystem-m3.md).

## Supported commands

- `LIST …` — projection (ENGLISH) or legacy filesystem listing (see Tcl rules in [Developer shell (TCL)](tcl-shell.md)).
- `COUNT …` — counts records in scope.
- `SELECT <file> [field ...]` — scans a logical file and materializes the session **active list** (IDs + source file name).
- `SORT …` — same raw row set as `LIST` for the same query, then **stable sort**.

Session helpers: `LIST-LIST`, `CLEAR-LIST`.

Tcl **`RESOLVE-FIELD`** *\<data-file\>* *\<token\>* prints how that token resolves (attribute number, F/I definition, conversion hint, and whether file-scoped `DICT-*` applies). Tcl **`LIST-DICT`** *\<dict-file\>* lists DICT items with type and validity. Tcl **`DEFINE-FIELD`** writes a minimal type **`A`** item into **`DICT`** or **`DICT-<file>`** (see below). F/I items are authored via **`EDIT DICT`** — see [DICT dictionary items](dict.md). Detail: [Developer shell (TCL)](tcl-shell.md).

## Grammar (whitespace-separated tokens)

After the verb:

1. **Explicit file:** first token names the logical Pick file, **unless** implicit active-list scope applies (below).
2. **Projection fields:** non-keyword tokens placed before an optional `WITH` clause (if any) and before the optional `BY` sort clause.
3. **`WITH <tokens…>`** — tokens after `WITH` are **parsed but not evaluated** (selection deferred post-Milestone 9). Projection and sort on computed fields work; **`WITH`** predicates do not filter rows yet. Parsing runs up to the token that starts the `BY` clause (for `SORT`) or end of line.
4. **`BY` / `BY-DSND` ( `SORT` only )** — after `BY`, list sort fields; `BY-DSND` immediately before a field makes that key **descending**. Repeating `BY` tokens is allowed for readability.

Examples:

- `LIST DATA NAME CITY`
- `LIST DATA NAME WITH SOME PREDICATE PLACEHOLDER`
- `SORT DATA NM BY NM`
- `SORT DATA BY NM BY-DSND CITY`
- `SORT DATA` — no `BY` → sort by **item-id** (lexicographic compare on the id string)

`BY` on `LIST` or `COUNT` is rejected at parse time (`BY is only valid with SORT`).

## Field and sort-key resolution

For both projection and `BY` keys, resolution order is deterministic:

1. **Decimal attribute number** → that attribute on the **data** record (1-based literal token).
2. **File-scoped dictionary** — if logical file **`DICT-<dataFile>`** exists (hyphen naming), look up the token there first (**`S`** synonyms follow targets using the same file-scoped-then-global rule).
3. **Global dictionary** — logical file **`DICT`**, same `A` / `S` rules.
4. **Unknown name** → the executor rejects the query (`Unknown ENGLISH field "…"`); there is no silent empty projection for named fields anymore.

Inside a dictionary record:

- Type **`A`**: attribute 2 = attribute number on the **data** file. Attributes **7–10** on the dictionary item supply conversion hints (`D`, `MD`, `MC`) for **`SORT`** (*see Ordering*).
- Type **`S`**: attribute 2 = next dictionary id (look up again in **`DICT-<dataFile>`** then **`DICT`**).
- Type **`F`**: correlative on a source attribute (value index, leftmost/rightmost, or OCONV). Evaluated per row via the correlative engine — see [Correlatives and computed attributes](correlatives.md).
- Type **`I`**: computed expression (arithmetic, conditionals, conversion calls). Evaluated per row — see [Correlatives and computed attributes](correlatives.md).

Full DICT reference: [DICT dictionary items](dict.md).

### Authoring `DICT` items from Tcl

**`DEFINE-FIELD <dict-file> <field-name> <attribute-number>`** (four tokens—see [Developer shell (TCL)](tcl-shell.md)) creates or **replaces** a minimal type **`A`** record: attribute 1 = `A`, attribute 2 = the target column on the **data** file—no multi-line authoring on the Tcl line and no change to **`WRITE`** semantics (`WRITE` stays **single string, full replace**). Example: **`DEFINE-FIELD DICT NAME 1`** for global **`DICT`**; **`CREATE-FILE DICT-DATA`** then **`DEFINE-FIELD DICT-DATA NAME 1`** for file-scoped dictionary.

**`WRITE`** cannot build multi-line F/I shapes from normal tokens (**one** joined line); repeating **`WRITE DICT …`** **replaces** the whole record. For type **`A`** rows beyond minimal authoring, or for **`F`** / **`I`** items, use **`EDIT DICT <record-id>`** (see [DICT dictionary items](dict.md) and [Developer shell (TCL)](tcl-shell.md)), then **`RESOLVE-FIELD`** or **`LIST-DICT`** to verify.

## Ordering (`SORT`)

- `SORT` uses the **same** scan + projection path as `LIST`, then **stable_sort** on DICT-resolved keys.
- Conversion `D` / `MD` / `MC`: the executor applies a **minimal numeric parse** (`stod` after stripping some punctuation). If parsing succeeds, compare as **double**; otherwise compare as **string**.
- Keys **without** conversion hints compare as **strings** (byte-wise ordering of the **first** multi-value sub-field, same as `.firstValue()` today).
- **Final tie-break:** **item-id** comparison so output order is deterministic.

Multi-value attributes use the same **`RecordAttribute`** splitting as the rest of M3; ordering uses the **first sub-value** for sort materialization in this milestone.

## Active list and implicit ENGLISH scope

`SELECT <file> …` saves:

- ordered record IDs, and  
- **`activeListSourceFile`** = `<file>` (the logical name used for subsequent `fs.read`).

When the list is non-empty, **implicit** ENGLISH lines omit the file token when:

- **`COUNT`** alone (one token), or
- **`LIST` / `SORT`** start with `BY` / `WITH` immediately after the verb (**no** file token), or
- **`LIST` / `SORT`** plus a **single** token that is **not** the name of an existing logical Pick file (ambiguous names that **are** valid logical files use full-file scope first — **clear the list** first if you meant a field).

Full **Pick** list lifecycle beyond this (saved lists, `SSELECT`, correlated expansion) is deferred.

## Tcl routing notes

- The first Tcl token is **`V`** / **`X`** / **`Q`** resolved through **`VOC`**, then uppercased and dispatched to built-in handlers. **`LIST`**, **`SORT`**, **`COUNT`**, **`SELECT`** are real Tcl commands plus matching **`VOC`** entries on shipped accounts so dictionary discovery matches Pick-style catalogues (`gemini/accounts/.../VOC/`).
- **`SORT`** ENGLISH detection follows the same style as **`LIST`**: enough tokens, clause keywords, **`SORT <existing-file>`** two-token form, or implicit active-list shapes. Other `SORT` lines are left for a future legacy Tcl implementation.
- See [Developer shell (TCL)](tcl-shell.md) for the exact Tcl table text and session reset rules.

## Worked example

Same **`DATA`** rows for both halves: **numeric `1`** needs no **`DICT`**—one-line **`WRITE DATA <id> <value>`** puts values in attribute **1**. **Named `NAME`** needs a **`DICT`** **`A`** item (**`DEFINE-FIELD DICT NAME 1`**). The **`NAME`** segment uses **`LIST DATA NAME`** (explicit **`DATA`**, global **`DICT`** only—no **`DICT-DATA`** file here); **`SELECT DATA`** restores implicit **`COUNT`**, **`LIST NAME`**, and **`SORT BY NAME`** on the active list like the **`1`** segment.

```text
CREATE-FILE DATA
WRITE DATA CUST1 ALICE
WRITE DATA CUST2 BOB
LIST DATA 1
SORT DATA 1 BY 1
SELECT DATA
COUNT
LIST 1
SORT BY 1
CREATE-FILE DICT
DEFINE-FIELD DICT NAME 1
RESOLVE-FIELD DATA NAME
LIST DATA NAME
SORT DATA NAME BY NAME
SELECT DATA
COUNT
LIST NAME
SORT BY NAME
CLEAR-LIST
```

## Report formatting

Milestone 8 report-formatting clauses (full detail in [docs/english-formatting.md](english-formatting.md)):

- **`HEADING "<text>"`** — Pick‑classic `@`-token substitution (`@DATE`, `@TIME`, `@PAGE`, `@<digits>`, `@@` escape).
- **`FOOTING "<text>"`** — same `@` tokens as `HEADING`; once at end of report without `HEADING`, per-page footer when `HEADING` + pagination are active.
- **Pagination** — configurable via `SET PAGE-LENGTH n` (default 24) when a `HEADING` is present.
- **`BREAK-ON <field>`** — full-width hyphen break line when the break-field value changes between consecutive rows.
- **`TOTAL <field>`** — numeric accumulation with subtotals at break boundaries and a grand total at end of report (`TOTAL <field>: <value>`).
- **`ID-SUPP`** — suppress record ids on data rows (fields only).

`HELP LIST`, `HELP SORT`, and `HELP SELECT` document these clauses. Queries without formatting clauses remain byte-identical to the pre‑M8 executor output.

## Non-goals (deferred)

- Executed **`WITH`** selection predicates (including on computed fields).
- Full **`MD`** / date-mask / currency conversion semantics beyond the Milestone 7 subset.
- Legacy Tcl **`SORT`** for non-ENGLISH-shaped input.
