# BASIC language (compiler subset v1)

This page documents BASIC compiler subset v1: the currently supported language features compiled from `BASIC>` source lines into VM bytecode.

- Shell/editor command behavior is documented in [BASIC shell](basic-shell.md).
- VM opcode/runtime behavior is documented in [Bytecode VM](vm.md).

## Supported statements (v1)

- `LET <var> = <expr>`
- `PRINT <expr>`
- `END` (optional)

If `END` is omitted, the compiler emits an implicit terminating `HALT`.

## Variables and naming

- Variable names are case-insensitive.
- Compiler/runtime canonicalize variable names to uppercase.
- Current variable name pattern is letter-first alphanumeric (for example `A`, `X9`, `TOTAL1`).
- Variables are currently integer-backed for numeric expression paths.

## Expressions (integer path)

Numeric expressions are supported in:

- `LET` right-hand side
- `PRINT` (numeric path)

Supported expression features:

- int literals (for example `42`)
- variable references (for example `A`)
- parentheses
- unary minus
- binary operators `+`, `-`, `*`, `/`

Operator precedence:

1. unary `-`
2. `*`, `/`
3. `+`, `-`

Associativity for binary operators is left-to-right.

Division is integer division with truncation toward zero; divide-by-zero raises runtime error:

```text
DIV: divide by zero
```

## `PRINT` string literals

`PRINT` also supports string literals:

```basic
PRINT "HELLO"
```

String literal `PRINT` uses string VM print path; numeric expressions use integer VM print path.

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
