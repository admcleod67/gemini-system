# BASIC language (compiler subset v1)

This page documents BASIC compiler subset v1: the currently supported language features compiled from `BASIC>` source lines into VM bytecode.

- Shell/editor command behavior is documented in [BASIC shell](basic-shell.md).
- VM opcode/runtime behavior is documented in [Bytecode VM](vm.md).

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
- comparison operators `=`, `<>`, `<`, `<=`, `>`, `>=`

Operator precedence:

1. unary `-`
2. `*`, `/`
3. `+`, `-`
4. comparison operators

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

`PRINT` newline behavior:

- `PRINT <expr>` prints the value and ends the line.
- `PRINT <expr>;` prints the value without ending the line.

The semicolon form is useful for prompt-style interaction, for example printing a prompt before `INPUT`.

## `INPUT <var>`

`INPUT` reads one line from runtime input, parses it as an integer, and stores it in the target variable.

- `INPUT` currently supports exactly one variable argument.
- Input is integer-only in v1.
- Invalid input and end-of-input are runtime errors.

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
