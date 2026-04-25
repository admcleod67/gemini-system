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
- `IF <cond> THEN <line> [ELSE <line>]`
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
- `IF <cond> THEN <line>` jumps to `<line>` when `<cond>` is true; otherwise execution continues to the next sequential line.

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
- `IF <cond> THEN <line1> ELSE <line2>` jumps to `<line1>` when true, otherwise to `<line2>`.
- `STOP` halts execution immediately (same runtime effect as `END`).
- Unknown target lines are compile-time errors.

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
