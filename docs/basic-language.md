# BASIC language (compiler subset v1)

This page documents BASIC compiler subset v1: the currently supported language features compiled from `BASIC>` source lines into VM bytecode.

- BASIC shell commands and the shared **`EDIT`** line editor are documented in [BASIC shell](basic-shell.md).
- VM opcode/runtime behavior is documented in [Bytecode VM](vm.md).
- Compiler phase architecture is documented in [Compiler architecture](compiler-architecture.md).

## Compiler architecture note

Compiler internals are in an incremental refactor phase. Expression parsing and statement parsing are now separated into dedicated frontend parser modules with AST nodes, while bytecode emission remains in the compiler backend flow. This keeps external compiler behavior stable while improving internal structure for future front/middle/back separation.

## Supported statements (v1)

- `LET <var> = <expr>`
- `PRINT <expr>`
- `GOTO <line>`
- `GOSUB <line>`
- `RETURN`
- `FOR <var> = <init> TO <limit> [STEP <step>]`
- `NEXT <var>`
- `DIM <var>(<size>)`
- `MAT <arr> = <expr>`
- `MAT <arr> = MAT <arr>`
- `MAT READ <arr> FROM <filevar>, <id-expr> [ELSE <line-or-statement>]`
- `MAT WRITE <arr> ON <filevar>, <id-expr> [ELSE <line-or-statement>]`
- `OPEN <file-expr> TO <filevar> [ELSE <line-or-statement>]`
- `READ <var> FROM <filevar>, <id-expr> [ELSE <line-or-statement>]`
- `WRITE <expr> ON <filevar>, <id-expr> [ELSE <line-or-statement>]`
- `READNEXT <var> FROM <filevar> [ELSE <line-or-statement>]`
- `READV <var> FROM <filevar>, <id-expr>, <attr-expr>[, <value-index-expr>] [ELSE <line-or-statement>]`
- `WRITEV <expr> ON <filevar>, <id-expr>, <attr-expr>[, <value-index-expr>] [ELSE <line-or-statement>]`
- `CLOSE <filevar>`
- `CLEAR`
- `IF <cond> THEN <line-or-statement> [ELSE <line-or-statement>]`
- `STOP`
- `INPUT <var>`
- `INPUT <prompt-expr>, <var>`
- `CHAIN <program-expr>`
- `END` (optional)

If `END` is omitted, the compiler emits an implicit terminating `HALT`.

## Variables and naming

Pick BASIC variables are fundamentally typeless dynamic strings. The suffix on a variable name determines how the runtime handles it:

| Suffix | Example | Meaning |
|--------|---------|---------|
| `$` | `A$`, `NAME$` | **Force string.** Any value is stored and printed as-is. Using a `$` variable as an arithmetic operand is a **compile-time error**. |
| `%` | `A%`, `COUNT%` | **Force integer.** The value is coerced to int on assignment and on `INPUT` (numeric coercion, truncating toward zero). |
| *(none)* | `A`, `TOTAL` | **Dynamic.** Any value is stored as-is. Arithmetic uses numeric coercion at runtime (`int`/`float` domain). |

Additional naming rules:

- Variable names are case-insensitive; the compiler and runtime canonicalize them to uppercase.
- The base name (before the suffix) must begin with a letter and contain only letters and digits (for example `A`, `X9`, `TOTAL1`), **or** the whole identifier (no `$` / `%` suffix) is exactly **`@USERNO`**, **`@ACCOUNT`**, **`@LOGNAME`**, or **`@DEFDATA`** — **session system variables** (the last is the logical file name from **`MD,DEFDATA`** when configured). They are allowed in **expressions** only; **`LET`**, **`INPUT`**, **`FOR`**, **`NEXT`**, and **`DIM`** targets may not use these names (compile error: read-only system variable). Other **`@Something`** identifiers are invalid in expressions.

## Coercion rules (runtime contract)

All rules below are implemented in [`src/core/vm/Runtime.cpp`](src/core/vm/Runtime.cpp) (`coerceToDouble`, `coerceToInt`, `valueToString`, `compareValues`) and exercised through VM opcodes and **`InvokeBuiltin`**.

| Context | Rule |
|---------|------|
| **Arithmetic** (`+`, `-`, `*`, `/`, unary `-`) | Operands use **numeric coercion**: empty string → `0`; numeric **prefix** via `strtod` (e.g. `"12ABC"` → `12`). Pure `int` paths preserve integers where possible; operands with non-numeric trailing junk can force a **float** result (see arithmetic implementation). |
| **`%` / `COERCE_INT`** (`LET` / `INPUT` / `FOR` init into `%`, array store to `%`) | Same numeric input as arithmetic, then **truncate toward zero** to `int` (`coerceToInt`). Non-numeric prefix → `0`. This matches the **`INT`** builtin’s truncation step after reading a double. |
| **Comparisons** (`=`, `<>`, `<`, …) | **Both** operands must be **numeric** (`int`/`double`, coerced as double) **or** **both** `string` (lexicographic). **Mixed types** → runtime error (by design; see Milestone 7). |
| **`PRINT` / `PRINT_VAL`** | Uses **`valueToString`**: same string form as the **`STR`** builtin (int decimal, floats with trailing-zero trim, strings unchanged). |
| **Built-in arguments** | Each argument is compiled in either **numeric** or **string** context per the builtin (see `builtinArgInArithmetic` in [`BasicBytecodeEmitter.cpp`](src/userland/basic/BasicBytecodeEmitter.cpp)); handlers use **`coerceToInt` / `coerceToDouble` / `valueToString`** accordingly. |

**`IF` conditions:** the compiler emits **`JUMP_IF_ZERO`**, which expects an **`int`** on the stack. Comparison results are `int` (0/1). A bare floating-point condition (for example `IF 1.5 THEN …`) is not guaranteed to work until the condition pipeline is extended; use comparisons or integer expressions.

**`INPUT`:** one trimmed line is read as a string; for `%` targets, **`COERCE_INT`** applies the same rules as **`LET A% = "<line>"`**.

## Expressions

Expressions are supported on the right-hand side of `LET`, as the argument to `PRINT`, and as the condition in `IF`.

Supported expression features:

- int literals (for example `42`)
- float literals (for example `3.14`, `0.5`, `1E3`, `-2.5E-1`)
- string literals (for example `"hello"`)
- variable references (for example `A`, `A$`, `A%`, `@ACCOUNT`, `@DEFDATA`)
- parentheses
- unary minus
- binary operators `+`, `-`, `*`, `/`
- comparison operators `=`, `<>`, `<`, `<=`, `>`, `>=`

Operator precedence:

1. unary `-`
2. `*`, `/`
3. `+`, `-`
4. comparison operators

Associativity for binary operators is left-to-right.

### Arithmetic type behaviour

Arithmetic operators (`+`, `-`, `*`, `/`) operate in a numeric domain:

- `int` and `float` values interoperate naturally.
- Strings are coerced using numeric-prefix rules (`strtod`-style): `"" -> 0`, `"12ABC" -> 12`.
- Pure `int` arithmetic for `+`, `-`, `*` preserves integer results; mixed or float operands produce float results.
- `/` keeps historical integer behavior for pure ints, and produces float for mixed/float operands.

Examples:

```basic
LET A = "hello"
PRINT A + 5       ; prints 5  ("hello" coerces to 0)

LET B = "10"
PRINT B + 20      ; prints 30 ("10" coerces to 10)

LET C = "12ABC"
PRINT C + 1       ; prints 13
```

Using a `$`-suffix variable in an arithmetic expression is a **compile-time error**:

```basic
LET A$ = "hello"
PRINT A$ + 1      ; ERROR: compile-time rejection
```

Division is integer division with truncation toward zero; divide-by-zero raises a runtime error:

```text
DIV: divide by zero
```

### Comparison type behaviour

Comparison operators (`=`, `<>`, `<`, `<=`, `>`, `>=`) require both operands to be the same type (int/int or string/string). Mixed-type comparisons raise a runtime error. See **Coercion rules (runtime contract)** above for the full matrix.

## `PRINT <expr>`

`PRINT` accepts any expression (integer or string) and always uses the `PRINT_VAL` VM opcode, which prints int or string values transparently.

`PRINT` newline behaviour:

- `PRINT <expr>` prints the value and ends the line.
- `PRINT <expr>;` prints the value without ending the line.

The semicolon form is useful for prompt-style interaction, for example printing a prompt before `INPUT`.

## `INPUT`

`INPUT` reads one line from runtime input as a raw string (leading/trailing whitespace trimmed) and stores it in the target variable.

- `INPUT <var>` always emits `INPUT_STR` to read a raw string.
- `INPUT <prompt-expr>, <var>` first emits prompt output (`PRINT` without newline), then `INPUT_STR`.
- For `%`-suffix variables (for example `INPUT A%`), the compiler additionally emits `COERCE_INT`, which converts the raw string to an integer (non-numeric → 0) before storing.
- Prompt expression can be any BASIC expression; malformed prompt expressions are compile-time errors.
- End-of-input raises a runtime error.

## `CHAIN <program-expr>`

`CHAIN` evaluates `<program-expr>` to a target program name and transfers execution to that BASIC program.

- Example: `CHAIN "NEXTPROG"`.
- Target program is resolved using the same VOC/BP resolution as normal BASIC `RUN`.
- No argument passing is supported in this milestone.
- `CHAIN` with an empty program name raises a runtime error.

## Control flow

- `GOTO <line>` jumps to the specified BASIC line number.
- `GOSUB <line>` calls the subroutine beginning at the specified BASIC line number. Execution resumes at the statement following `GOSUB` when a matching `RETURN` is reached.
- `RETURN` returns from the most recently called `GOSUB`. Raises a runtime error if there is no active `GOSUB` call.
- `IF <cond> THEN <line-or-statement>` executes the branch when `<cond>` is true; otherwise execution continues to the next sequential line.

A running program can be interrupted at any time by pressing **Ctrl-C**. The VM stops cleanly and prints `Break`, then returns to the BASIC shell prompt. Ctrl-C has no effect at the command prompt.

## Loops

`FOR <var> = <init> TO <limit> [STEP <step>]` … `NEXT <var>` implements a counted loop.

Authentic Pick BASIC semantics are preserved:

- **Body executes at least once.** The termination test occurs *after* `NEXT`, not before the body.
- **`NEXT` increments** the loop variable by `step` (default 1), then checks whether the loop is done:
  - Positive step: loop ends when `var > limit`.
  - Negative step: loop ends when `var < limit`.
  - Zero step: runtime error `"FOR: STEP cannot be zero"`.
- `limit` and `step` are captured once at loop entry (`FOR_SETUP`), so any later assignment to variables used in those expressions does not affect the loop.
- The loop control variable obeys the usual suffix rules: `$` suffix is a **compile-time error**; `%` suffix applies `COERCE_INT` to the initial value; unsuffixed variables are stored as-is.

Example:

```
10 FOR I = 1 TO 5
20   PRINT I
30 NEXT I
40 END
```

This prints `1`, `2`, `3`, `4`, `5` (one per line).

Counting down requires an explicit negative step:

```
10 FOR I = 5 TO 1 STEP -1
20   PRINT I
30 NEXT I
40 END
```

`NEXT` without a matching `FOR` raises `"NEXT without FOR"` at runtime. If the variable name in `NEXT` does not match the top of the loop stack, a `"FOR/NEXT variable mismatch"` runtime error is raised.
- `IF <cond> THEN <line-or-statement> ELSE <line-or-statement>` supports line targets and inline single statements in both branches.
- `STOP` halts execution immediately (same runtime effect as `END`).
- Unknown target lines are compile-time errors.

Branch arm rules for `THEN` and `ELSE`:

- Each arm accepts either a positive target line (for example `THEN 200`) or one inline statement (for example `ELSE PRINT "ERR"`).
- Only one statement is allowed per arm. Multi-statement forms (for example using `:`) are rejected.
- `FOR` and `NEXT` are not allowed as inline branch statements; loops must remain standalone line statements.
- `ELSE` binds to the nearest unmatched `IF` (classic dangling-`ELSE` behavior).

Conditions evaluate to integer booleans (`1` true, `0` false).

## Compile diagnostics

On compile failure, diagnostics are line-scoped and reported by the shell as:

```text
Error on line <basicLine>: <message>
Compilation failed.
```

Expression-related messages include categories such as:

- unexpected token
- missing `)`
- empty expression
- invalid operand/operator placement
- `String variable '<name>' cannot be used in an arithmetic expression` (compile-time rejection of `$` variables in arithmetic)

## Arrays

`DIM <var>(<size>)` declares a one-dimensional array with `<size>` elements. Arrays are 1-based: `<var>(1)` through `<var>(<size>)`.

```
10 DIM A(10)
20 LET A(1) = 100
30 PRINT A(1)
40 END
```

**Rules:**

- `<size>` is evaluated at runtime and must be ≥ 1, otherwise a runtime error is raised.
- Re-executing `DIM` on the same variable silently re-allocates and zero-initialises the array.
- `LET <var>(<index>) = <expr>` writes to an element; `<var>(<index>)` anywhere in an expression reads an element.
- Accessing an element before `DIM` has been executed, or with an out-of-range index, raises a runtime error.
- Array variables obey the same suffix rules as scalars:
  - `A$(n)` — string array; using in arithmetic is a compile-time error.
  - `A%(n)` — integer array; values are coerced to int on write.
  - `A(n)` — dynamic array; values are stored as-is and coerced to int in arithmetic contexts.
- Arrays and scalar variables with the same base name are independent: `A` and `A(1)` are different storage locations.

Arrays are distinct from record multi-values. `A(1)` indexes an in-memory `DIM` array cell, while
`REC<attr,value>` (or `READV`/`WRITEV`) indexes a subvalue inside an attribute of a record-body string.
There is no implicit mapping between array indexes and record attributes/subvalues.

## MAT operations

`MAT` statements operate on whole `DIM`'d arrays. All four shapes require the target array (and, for `MAT A = MAT B`, the source array) to have been previously declared with `DIM`; using an undefined array raises a runtime error.

**Shapes:**

- `MAT <arr> = <expr>` — broadcast a scalar into every slot of `<arr>`.

  ```
  10 DIM A(3)
  20 MAT A = 0
  30 PRINT A(1); PRINT A(2); PRINT A(3)
  40 END
  ```

  For `%`-suffix arrays each slot is coerced to int on assignment (so `MAT A% = "abc"` fills every slot with `0`, the same rule as `LET A% = "abc"`).

- `MAT <arr-dst> = MAT <arr-src>` — element-wise copy.

  ```
  10 DIM A(3)
  20 DIM B(3)
  30 MAT A = 1
  40 MAT B = MAT A
  50 PRINT B(2)
  60 END
  ```

  Strict: both arrays must already be `DIM`'d **and have the same size**. A size mismatch raises a runtime error (no padding, no truncation).

- `MAT READ <arr> FROM <filevar>, <id-expr> [ELSE <line-or-statement>]` — read a record and split its attributes into the slots of `<arr>` (slot `i` ← attribute `i`).

  ```
  10 DIM CUST(4)
  20 OPEN "CUSTOMERS" TO FVAR ELSE STOP
  30 MAT READ CUST FROM FVAR, "1001" ELSE STOP
  40 PRINT CUST(1)
  50 END
  ```

  Strict: the record's attribute count must equal `DIM(<arr>)`; otherwise a runtime error is raised. The `ELSE` arm fires on a missing record (same semantics as `READ … ELSE`).

- `MAT WRITE <arr> ON <filevar>, <id-expr> [ELSE <line-or-statement>]` — pack the slots of `<arr>` (slot `i` → attribute `i`) into a record and write it.

  ```
  10 DIM ORDER(3)
  20 LET ORDER(1) = "ORD-1"
  30 LET ORDER(2) = "2026-05-26"
  40 LET ORDER(3) = "SHIPPED"
  50 OPEN "ORDERS" TO FVAR ELSE STOP
  60 MAT WRITE ORDER ON FVAR, "ORD-1" ELSE STOP
  70 END
  ```

  The `ELSE` arm fires on a write failure (same semantics as `WRITE … ON … ELSE`).

**Runtime errors (stable substrings):**

- `BUILTIN: MAT undefined array "<ARR>"` — the named array was not previously `DIM`'d.
- `BUILTIN: MAT dimension mismatch` — `MAT A = MAT B` with `DIM(A) != DIM(B)`.
- `BUILTIN: MAT READ attribute count mismatch` — record attribute count differs from `DIM(<arr>)`.

**Non-goals (v1):** matrix arithmetic (`MAT A = MAT B + MAT C`), `MAT INPUT`, `MAT PRINT`, automatic `DIM` via `MAT READ`, and overflow of trailing record attributes into the final slot are not supported.

## File handling (Stage 1)

File handling uses the PickFS backend and file-variable handles:

- `OPEN <file-expr> TO <filevar> [ELSE <line-or-statement>]`
- `READ <var> FROM <filevar>, <id-expr> [ELSE <line-or-statement>]`
- `WRITE <expr> ON <filevar>, <id-expr> [ELSE <line-or-statement>]`
- `READNEXT <var> FROM <filevar> [ELSE <line-or-statement>]`
- `READV <var> FROM <filevar>, <id-expr>, <attr-expr>[, <value-index-expr>] [ELSE <line-or-statement>]`
- `WRITEV <expr> ON <filevar>, <id-expr>, <attr-expr>[, <value-index-expr>] [ELSE <line-or-statement>]`
- `CLOSE <filevar>`

Examples:

```
10 OPEN "CUSTOMERS" TO FVAR ELSE 90
20 READ REC FROM FVAR, ID ELSE 80
30 WRITE REC ON FVAR, ID ELSE 90
40 CLOSE FVAR
50 END
80 LET REC = ""
90 END
```

Inline `ELSE` statement examples:

```
10 OPEN "CUSTOMERS" TO FVAR ELSE PRINT "OPEN FAIL"
20 READ REC FROM FVAR, ID ELSE LET REC = ""
30 WRITE REC ON FVAR, ID ELSE STOP
40 END
```

Rules:

- `OPEN` succeeds only if the file already exists. There is no automatic file creation from BASIC.
- `OPEN`, `READ`, `WRITE`, `READNEXT`, `READV`, and `WRITEV` route failures to `ELSE` when provided.
- Without `ELSE`, failures raise runtime errors and stop execution.
- `READNEXT` iterates record IDs in deterministic lexicographic order per open file handle.
- `OPEN` resets `READNEXT` cursor state for that file handle (`PickFS::FileSystem::FileHandle`).
- `WRITE` and `WRITEV` invalidate (reset) the `READNEXT` cursor for the same file handle.
- `READV`/`WRITEV` preserve unrelated attributes and sibling multi-values.
- `READV` presence semantics: missing attribute/subvalue is a classified failure (`ATTRIBUTE.NOT.FOUND` / `SUBVALUE.NOT.FOUND`).
  - With `ELSE` it routes to the `ELSE` clause (via the `READV_TRY` opcode path).
  - Without `ELSE` it raises a runtime error.
- File `ELSE` accepts either a line target or one inline statement.
- `CLOSE` on an unopened file variable is a no-op.
- Open file handles are automatically released when the program ends.

### Angle-bracket attribute access

BASIC expressions support read-only multi-value extraction from a record-body variable:

- `REC<attr>` returns the raw attribute text.
- `REC<attr,valueIndex>` returns the 1-based subvalue from that attribute.

This syntax uses the same `StructuredRecord`/value-mark splitting rules as `READV`/`WRITEV` and ENGLISH.
To avoid ambiguity with comparisons, attribute access is recognized only when `<` is immediately adjacent
to an identifier (`REC<2>`). `REC < 2` remains a less-than comparison.

## CLEAR

```
CLEAR
```

`CLEAR` drops all scalar variables and all array allocations from memory. After `CLEAR`:

- Any attempt to read a variable that was previously set will raise a runtime "Undefined variable" error.
- Any attempt to access an array element will raise a runtime "Array not dimensioned" error until the array is re-dimensioned with `DIM`.

`CLEAR` takes no arguments. It does not affect the program itself, the VM stack, the instruction pointer, the call stack, or the for-stack.

## Built-in Functions

Built-in functions use the form `NAME(expr [, expr …])`. Arity is fixed per function (some accept zero arguments, e.g. `DATE()`). Function names are case-insensitive. Exact numeric/string rules for builtins that depend on coercion are documented in comments next to the **`InvokeBuiltin`** handlers in [`src/core/vm/Runtime.cpp`](src/core/vm/Runtime.cpp).

### ABS

```
ABS(<numeric-expr>)
```

Returns the absolute value of the numeric expression. The argument is coerced to integer (Pick semantics: non-numeric string → 0). Using a `$`-suffix variable as the argument is a compile-time error.

```
10 LET X = ABS(-42)   ;* X = 42
20 PRINT ABS(0)       ;* prints 0
```

### SGN

```
SGN(<numeric-expr>)
```

Returns the sign of the numeric expression: `-1` for negative, `0` for zero, `1` for positive. The argument is coerced to integer. Using a `$`-suffix variable as the argument is a compile-time error.

```
10 PRINT SGN(100)   ;* prints 1
20 PRINT SGN(-5)    ;* prints -1
30 PRINT SGN(0)     ;* prints 0
```

### SEQ

```
SEQ(<expr>)
```

Returns the ASCII (numeric) value of the first character of the string representation of `<expr>`. If the argument evaluates to an empty string, `SEQ` returns `0`. All variable types (including `$`-suffix) are accepted.

```
10 PRINT SEQ("A")       ;* prints 65
20 PRINT SEQ("Hello")   ;* prints 72  (ASCII of 'H')
30 PRINT SEQ("")        ;* prints 0
```

### INDEX

```
INDEX(<string-expr>, <substring-expr> [, <occurrence-expr>])
```

Returns the **1-based byte offset** of the start of the *n*th occurrence of `substring` in `string` (UTF-8 byte indices, consistent with `LEN`). Occurrence defaults to **1** when the third argument is omitted (the compiler supplies `1`). Returns **0** if there are fewer than *n* matches. Overlapping matches are allowed: the next search begins at the previous match start plus one byte. Empty `substring`, or an occurrence less than 1, is a runtime error (`BUILTIN: INDEX …`).

### FIELD

```
FIELD(<string-expr>, <delimiter-expr>, <field-number-expr>)
```

Splits `string` on each non-overlapping occurrence of the delimiter substring (not a regular expression). `field-number` is **1-based**; field 1 is the substring before the first delimiter. If `field-number` is greater than the number of fields, `FIELD` returns an empty string. Empty delimiter, or a field number less than 1, is a runtime error (`BUILTIN: FIELD …`).

### STR

```
STR(<expr>)
```

Returns the dynamic string form of `<expr>` using the same conversion rules as the VM’s internal `valueToString` helper (integers as decimal text, floats trimmed of trailing zeros, strings unchanged).

### OCONV

```
OCONV(<value-expr>, <code-expr>)
```

Converts an **internal** value to its **display** form using a Pick conversion code. The supported codes are listed in the **Supported conversion codes** table below. Unknown codes raise a runtime error containing `BUILTIN: OCONV unsupported code "<code>"`. Codes are matched case-insensitively.

```
10 PRINT OCONV(DATE(), "D")    ;* e.g. "26 May 2026"
20 PRINT OCONV(3661, "MTS")    ;* "01:01:01"
30 PRINT OCONV(1234, "MD2")    ;* "12.34"
40 PRINT OCONV("hello", "MCU") ;* "HELLO"
```

### ICONV

```
ICONV(<string-expr>, <code-expr>)
```

The inverse of `OCONV`: parses a display string back into an internal value using the same code set. A parseable input for a known code returns the internal value (an integer for `D`, `MT`, `MTS`, `MD`, `MD2`; a string for `MCU` / `MCL`). Unparseable input raises `BUILTIN: ICONV invalid "<code>" input`; unknown codes raise `BUILTIN: ICONV unsupported code "<code>"`.

```
10 DAY = ICONV("01 Jan 1970", "D")  ;* 732 (Pick internal day)
20 SEC = ICONV("12:30", "MT")       ;* 45000
30 N   = ICONV("12.34", "MD2")      ;* 1234 (rounds half-away-from-zero)
```

### NUM

```
NUM(<expr>)
```

Returns `1` if `<expr>` is **fully numeric** (numeric value, or a string that `strtod` consumes in full with no trailing non-whitespace), otherwise `0`. The empty string is **not** numeric (returns `0`), matching Pick semantics.

```
10 PRINT NUM("123")    ;* 1
20 PRINT NUM("-3.14")  ;* 1
30 PRINT NUM("12ABC")  ;* 0
40 PRINT NUM("")       ;* 0
```

### CONVERT

```
CONVERT(<string-expr>, <from-expr>, <to-expr>)
```

Returns a copy of `string` with each character also present in `from` replaced by the character at the same byte index in `to`. Characters in `from` whose index is past `len(to)` are **deleted**. Characters not in `from` pass through unchanged. An empty `from` returns the input unchanged.

```
10 PRINT CONVERT("hello", "el", "ip")   ;* "hippo" (e→i, l→p)
20 PRINT CONVERT("abc", "b", "")        ;* "ac"    (b is dropped)
30 PRINT CONVERT("hello", "", "X")      ;* "hello" (empty from)
```

#### Supported conversion codes

The first-pass conversion table is intentionally small; codes not in the table raise `BUILTIN: OCONV unsupported code "<code>"` (or `ICONV` for the inverse). Future milestones can extend the table without breaking these stable error substrings.

| Code | OCONV (internal → display) | ICONV (display → internal) |
|------|----------------------------|----------------------------|
| `D`   | Pick internal day (`int`) → `"dd MMM yyyy"` (UTC, English month abbreviation). | `"dd MMM yyyy"` (case-insensitive month) → Pick internal day. |
| `MT`  | Seconds since midnight (`int`) → `"HH:MM"`. | `"HH:MM"` → seconds since midnight. |
| `MTS` | Seconds since midnight (`int`) → `"HH:MM:SS"`. | `"HH:MM:SS"` → seconds since midnight. |
| `MCU` | String → byte-level uppercase (idempotent; identical for OCONV / ICONV). | Same as OCONV (byte-level uppercase). |
| `MCL` | String → byte-level lowercase (idempotent; identical for OCONV / ICONV). | Same as OCONV (byte-level lowercase). |
| `MD`  | Integer (`int`) → decimal text (no implicit decimal point). | Signed decimal text → `int` (rejects non-numeric input). |
| `MD2` | Integer (`int`) → text with implicit two decimal places (`1234` → `"12.34"`, `-5` → `"-0.05"`). | `"12.34"` → `1234` (rounds half-away-from-zero on extra fractional digits). |
