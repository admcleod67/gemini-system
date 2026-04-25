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
- `IF <cond> THEN <line>` jumps to `<line>` when `<cond>` is true; otherwise execution continues to the next sequential line.
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
