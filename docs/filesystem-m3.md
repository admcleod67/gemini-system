# Filesystem M3: attribute-aware record model

This document defines the **Milestone 3** filesystem evolution from raw record blobs to a
structured, attribute-aware model suitable for DICT and ENGLISH.

It complements:

- [Project milestones](milestones.md) (Milestone 3 scope)
- [File system (host backend transition)](filesystem.md) (current baseline behaviour)

## Purpose

Milestone 3 introduces the first semantically meaningful change to record handling:

- External Tcl semantics for `READ` / `WRITE` remain Pick-style raw record text.
- Internally, records become **attribute-indexed** so ENGLISH, DICT resolution, and BASIC can
  access canonical attribute/value views.

This is the minimum structural step needed before formatting, correlatives, computed attributes,
and report semantics.

## M3a implementation status

The current M3a implementation lands the internal model groundwork while preserving external Tcl
contracts:

- `READ` / `WRITE` still operate on raw Pick-style record text at command boundaries.
- Core filesystem paths normalize raw payloads through shared parse/serialize helpers.
- Record internals expose attribute-indexed access and centralized multivalue splitting primitives.

## M3b implementation status

M3b now adds a minimal ENGLISH processor layer on top of the M3a structured model:

- standalone core module at `src/core/english/` with parser, dictionary resolver, planner, executor, and service facade;
- at M3b slice delivery, grammar covered `LIST <file> [field ...]`, `COUNT <file> [field ...]`, `SELECT <file> [field …]`; **`SORT`**, file-scoped **`DICT-<file>`** precedence, **`RESOLVE-FIELD`**, and unknown-field hard errors are recorded under **M3c** below;
- executor scans records through filesystem APIs and reads projected fields through `Record::structured()`;
- DICT baseline resolves numeric fields directly, plus `A` and simple `S` indirection from **`DICT`** / **`DICT-<file>`** records (see M3c for precedence);
- Tcl integration uses contextual `LIST`: bare `LIST <file>` keeps legacy record-name listing, while field-bearing `LIST` routes to ENGLISH.

Active-list behavior for M3b is session-scoped in Tcl:

- `SELECT` stores selected IDs into the shell session active list;
- `LIST-LIST` prints the current active list;
- `CLEAR-LIST` clears it;
- list state clears on session reset and `LOGOFF`.

## M3c implementation status (ordering + list scope)

Ordering and execution scope build on M3b:

- **`SORT`** verb shares parser/projection/`DICT` resolution with **`LIST`**, adds stable ordering; **`WITH`** absorbed (selection deferred); **`BY`** / **`BY-DSND`** drive sort keys; no `BY` → sort by item-id.
- Sort comparisons: **`D`** / **`MD`** / **`MC`** hints enable numeric compare after minimal parse when possible; otherwise string compare on the primary sub-value; deterministic tie-break on item-id.
- **`DICT` lookup precedence:** literal attribute index → **`DICT-<logicalDataFile>`** if that Pick file exists → global **`DICT`**; unnamed tokens that fail lookup are **`Unknown ENGLISH field`** errors. Synonym **`S`** uses the same rule for each hop.
- Tcl **`RESOLVE-FIELD`** prints DICT outcome for diagnostics; **`SYSPROG/VOC`** ships **`V`** entries for **`LIST`/`SORT`/`COUNT`/`SELECT`** (catalog visibility; handlers stay built-ins).
- **Active-list scope:** after **`SELECT`**, Tcl stores IDs plus **`activeListSourceFile`**; **`COUNT`**, **`LIST`**, and **`SORT`** honor implicit file + constrained IDs until a logical file name appears as an explicit leading token (**full-file scope** overrides). Clearing happens on **`CLEAR-LIST`**, **`LOGOFF`**, **`QUIT`** session reset, and **filesystem root** changes (**`ShellSession::setFileSystemRoot`**).
- **`SORT`** that does not match ENGLISH-shaped Tcl patterns is explicitly reserved (**message** emitted); legacy Tcl `SORT` is not implemented.

Full Pick list-management semantics remain future work.

## Scope and non-goals

### In scope (M3)

- Attribute-aware record model (`attributeNo -> RecordAttribute`).
- Canonical parse/serialize rules between raw Pick text and structured attributes.
- Shared multi-value splitting logic used by ENGLISH and BASIC.
- Stable API layer for processor consumers.

### Deferred (not in this document's implementation scope)

- Formatting/reporting semantics (`HEADING`, `BREAK-ON`, `TOTAL`, pagination, printer model).
- Correlative and computed-attribute execution (`I`/`F` item semantics).
- Full Pick MD dictionary semantics (beyond current M2 `MD,DEFDATA` session default).

## Data model

## `RecordAttribute`

`RecordAttribute` is a thin wrapper around raw attribute text with centralized multivalue parsing
helpers.

Current responsibilities:

- store raw attribute text exactly as parsed;
- return first value and value-by-index (Pick multivalue semantics);
- expose canonical split representation (single implementation used by ENGLISH and BASIC).

## Structured record view

Each record is represented internally as:

- `map<int, RecordAttribute>`
- key = **1-indexed** attribute number;
- only present attributes are stored.

Missing attributes are represented by key absence; lookup of a non-existent attribute returns an
empty `RecordAttribute` view, preserving Pick-style empty results without vector padding.

## Parse and serialize contract

## `WRITE` path (raw -> structured)

Input remains a Pick-style raw record body where newline separates attributes.

On write:

1. Parse newline-delimited attributes.
2. Build structured map entries (`1..N`) as `RecordAttribute`.
3. Preserve empty attributes as empty attribute payloads when explicitly present.

## `READ` path (structured -> raw)

Tcl callers still receive raw Pick-style newline-delimited text.

Internally, the same record instance must expose the structured map to ENGLISH/DICT/BASIC.

## Round-trip invariants

For accepted input, parse + serialize should be stable under these rules:

- attribute boundaries are newline-based;
- empty attributes remain representable;
- ordering remains attribute-number order;
- absent attributes remain absent (not auto-padded).

Newline and empty-attribute round-trip behaviour is covered in core filesystem tests.

## API shape and compatibility

External compatibility remains:

- existing Tcl `READ` / `WRITE` command contracts stay valid;
- filesystem callers can still pass/receive raw record text.

Internal additions provide:

- attribute-number access;
- multivalue access by index;
- shared parser/splitter primitives for higher-level processors.

The compatibility boundary should keep current callers working while enabling progressive migration
to structured access.

## Integration targets

## ENGLISH

- projection and selection read attributes by number through the structured view;
- conversion and DICT item processing consume canonical multivalue splits from `RecordAttribute`.

## DICT resolver

- A/S type resolution can access attribute payloads directly by number;
- conversion and extraction rules build on the same record/attribute primitives.

## BASIC

- compiler/runtime features needing attribute-level reads use shared primitives;
- avoid introducing a second split/parse implementation in BASIC codepaths.

## Test matrix

Minimum expected tests for this milestone:

- raw write/read round-trip with normal attributes;
- explicit empty attributes (including interior empties);
- sparse lookup semantics (missing attribute => empty `RecordAttribute`);
- multivalue splitting parity between ENGLISH and BASIC consumers;
- serialization order/invariance by attribute number.

## Migration notes

- Keep current on-disk format (`.item` text) during M3.
- Introduce structured model in-memory first.
- Defer any persistent binary/hashed-storage redesign to a later milestone.
