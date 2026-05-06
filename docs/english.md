# ENGLISH query core

This page documents the implemented ENGLISH subset (Milestone **3b** projection + **3c** ordering / active-list scope rules).

For roadmap context, see [Project milestones](milestones.md).  
For the attribute record model, see [Filesystem M3 model](filesystem-m3.md).

## Supported commands

- `LIST …` — projection (ENGLISH) or legacy filesystem listing (see Tcl rules in [Developer shell (TCL)](tcl-shell.md)).
- `COUNT …` — counts records in scope.
- `SELECT <file> [field ...]` — scans a logical file and materializes the session **active list** (IDs + source file name).
- `SORT …` — same raw row set as `LIST` for the same query, then **stable sort**.

Session helpers: `LIST-LIST`, `CLEAR-LIST`.

## Grammar (whitespace-separated tokens)

After the verb:

1. **Explicit file:** first token names the logical Pick file, **unless** implicit active-list scope applies (below).
2. **Projection fields:** non-keyword tokens placed before an optional `WITH` clause (if any) and before the optional `BY` sort clause.
3. **`WITH <tokens…>`** — tokens after `WITH` are **not evaluated** in this milestone (selection placeholder). Parsing runs up to the token that starts the `BY` clause (for `SORT`) or end of line.
4. **`BY` / `BY-DSND` ( `SORT` only )** — after `BY`, list sort fields; `BY-DSND` immediately before a field makes that key **descending**. Repeating `BY` tokens is allowed for readability.

Examples:

- `LIST DATA NAME CITY`
- `LIST DATA NAME WITH SOME PREDICATE PLACEHOLDER`
- `SORT DATA NM BY NM`
- `SORT DATA BY NM BY-DSND CITY`
- `SORT DATA` — no `BY` → sort by **item-id** (lexicographic compare on the id string)

`BY` on `LIST` or `COUNT` is rejected at parse time (`BY is only valid with SORT`).

## Field and sort-key resolution

For both projection and `BY` keys:

1. Decimal attribute number → that attribute (1-based).
2. `DICT` type `A`: attribute 2 holds the attribute number. Attributes 7–10 are scanned (first non-empty cell) for conversion hint: leading `D`, `MD`, or `MC` (case-insensitive) marks the key for **numeric** comparison when the value parses (*see Ordering*).
3. `DICT` type `S`: follow to another `DICT` id (depth limited).

If lookup fails, projected values are empty; sort keys compare as empty strings or fall back to non-numeric comparison.

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

- **`SORT`** ENGLISH detection follows the same style as **`LIST`**: enough tokens, clause keywords, **`SORT <existing-file>`** two-token form, or implicit active-list shapes. Other `SORT` lines are left for a future legacy Tcl implementation.
- See [Developer shell (TCL)](tcl-shell.md) for the exact Tcl table text and session reset rules.

## Worked example

```text
CREATE-FILE DATA
CREATE-FILE DICT
WRITE DICT NAME A
WRITE DICT NAME 1
WRITE DATA CUST1 ALICE
WRITE DATA CUST2 BOB
LIST DATA NAME
SORT DATA NAME BY NAME
SELECT DATA
COUNT
LIST NAME
SORT BY NAME
CLEAR-LIST
```

## Non-goals (deferred)

- Headings, breaks, totals, pagination, printer/report delivery.
- Executed `WITH` predicates.
- Full `MD` / date-mask / currency semantics and correlatives.
- Legacy Tcl `SORT` for non-ENGLISH-shaped input.
