# BASIC language (compiler subset v1)

This page documents BASIC compiler subset v1: the currently supported language features compiled from `BASIC>` source lines into VM bytecode.

- Shell/editor command behavior is documented in [BASIC shell](basic-shell.md).
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
- `OPEN <file-expr> TO <filevar> [ELSE <line-or-statement>]`
- `READ <var> FROM <filevar>, <id-expr> [ELSE <line-or-statement>]`
- `WRITE <expr> ON <filevar>, <id-expr> [ELSE <line-or-statement>]`
- `CLOSE <filevar>`
- `CLEAR`
- `IF <cond> THEN <line-or-statement> [ELSE <line-or-statement>]`
- `STOP`
- `INPUT <var>`
- `END` (optional)

If `END` is omitted, the compiler emits an implicit terminating `HALT`.

## Variables and naming

Pick BASIC variables are fundamentally typeless dynamic strings. The suffix on a variable name determines how the runtime handles it:

| Suffix | Example | Meaning |
|--------|---------|---------|
| `$` | `A$`, `NAME$` | **Force string.** Any value is stored and printed as-is. Using a `$` variable as an arithmetic operand is a **compile-time error**. |
| `%` | `A%`, `COUNT%` | **Force integer.** The value is coerced to int on assignment and on `INPUT` (`strtol`; non-numeric → 0). |
| *(none)* | `A`, `TOTAL` | **Dynamic.** Any value is stored as-is. When used in an arithmetic expression the value is coerced to int at runtime; non-numeric strings yield 0. |

Additional naming rules:

- Variable names are case-insensitive; the compiler and runtime canonicalize them to uppercase.
- The base name (before the suffix) must begin with a letter and contain only letters and digits (for example `A`, `X9`, `TOTAL1`).

## Expressions

Expressions are supported on the right-hand side of `LET`, as the argument to `PRINT`, and as the condition in `IF`.

Supported expression features:

- int literals (for example `42`)
- string literals (for example `"hello"`)
- variable references (for example `A`, `A$`, `A%`)
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

Arithmetic operators (`+`, `-`, `*`, `/`) coerce their operands to integers:

- An `int` value is used as-is.
- A string value is parsed as a decimal integer (`strtol`); non-numeric strings (including empty) yield `0`.

Examples:

```basic
LET A = "hello"
PRINT A + 5       ; prints 5  ("hello" coerces to 0)

LET B = "10"
PRINT B + 20      ; prints 30 ("10" coerces to 10)
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

Comparison operators (`=`, `<>`, `<`, `<=`, `>`, `>=`) require both operands to be the same type (int/int or string/string). Mixed-type comparisons raise a runtime error.

## `PRINT <expr>`

`PRINT` accepts any expression (integer or string) and always uses the `PRINT_VAL` VM opcode, which prints int or string values transparently.

`PRINT` newline behaviour:

- `PRINT <expr>` prints the value and ends the line.
- `PRINT <expr>;` prints the value without ending the line.

The semicolon form is useful for prompt-style interaction, for example printing a prompt before `INPUT`.

## `INPUT <var>`

`INPUT` reads one line from runtime input as a raw string (leading/trailing whitespace trimmed) and stores it in the target variable.

- `INPUT <var>` always emits `INPUT_STR` to read a raw string.
- For `%`-suffix variables (for example `INPUT A%`), the compiler additionally emits `COERCE_INT`, which converts the raw string to an integer (non-numeric → 0) before storing.
- `INPUT` currently supports exactly one variable argument.
- End-of-input raises a runtime error.

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

## File handling (MVP)

File handling uses the PickFS backend and file-variable handles:

- `OPEN <file-expr> TO <filevar> [ELSE <line-or-statement>]`
- `READ <var> FROM <filevar>, <id-expr> [ELSE <line-or-statement>]`
- `WRITE <expr> ON <filevar>, <id-expr> [ELSE <line-or-statement>]`
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
- `OPEN`, `READ`, and `WRITE` route failures to `ELSE` when provided.
- Without `ELSE`, failures raise runtime errors and stop execution.
- File `ELSE` accepts either a line target or one inline statement.
- `CLOSE` on an unopened file variable is a no-op.
- Open file handles are automatically released when the program ends.

## CLEAR

```
CLEAR
```

`CLEAR` drops all scalar variables and all array allocations from memory. After `CLEAR`:

- Any attempt to read a variable that was previously set will raise a runtime "Undefined variable" error.
- Any attempt to access an array element will raise a runtime "Array not dimensioned" error until the array is re-dimensioned with `DIM`.

`CLEAR` takes no arguments. It does not affect the program itself, the VM stack, the instruction pointer, the call stack, or the for-stack.

## Built-in Functions

Built-in functions are called with a single argument enclosed in parentheses. Function names are case-insensitive.

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
