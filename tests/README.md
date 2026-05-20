# Test files

Each `N.c` file is a valid AtomC program that exercises a different subset of the language. `N.txt` in `expected/` is the exact token stream the lexer must produce for that file — line by line, `<line>  <TOKEN_NAME>[:attribute]`, ending with `END`.

`make test` compares live lexer output against these snapshots. If any byte differs, the test fails and prints a diff.

## What each test covers

| File | Size | Focus | Lexical features exercised |
|---|---|---|---|
| `0.c` | 19 lines | Functions, nested `for` loops | `INT` keyword, arrays (`[5]`), assignment, comparison (`<`), addition, function call, `return` |
| `1.c` | 6 lines  | Smallest valid program | `VOID`, string literal (`CT_STRING`), line comments (`//`) |
| `2.c` | 7 lines  | I/O round-trip | Variables, assignment, function calls with return values |
| `3.c` | 8 lines  | Control flow | `IF` / `ELSE`, `<` operator, string branches |
| `4.c` | 12 lines | Logical operators, char literals | `CHAR` type, `CT_CHAR` (`'0'`, `'9'`), `AND` (`&&`), `>=`, `<=` |
| `5.c` | 13 lines | Reals, division | `DOUBLE`, `CT_REAL` (`0.0`), `/`, nested `for` with accumulator |
| `6.c` | 19 lines | Arrays, loops, char output | Large array literal (`v[100]`), `CT_CHAR` (`'#'`), array indexing, subtraction, three chained `for` loops |
| `7.c` | 11 lines | Scientific notation | `CT_REAL` with exponent (`2e0`), multiple real operations |
| `8.c` | 9 lines  | **Number systems + escapes** | Hex (`0xc`, `0x2`), octal (`014`), real forms (`20E-1`, `2.0`, `0.2e+1`), escaped strings (`"\"equal\"\t\t(h,o)"`), escaped char (`'\\'`), `AND`, `EQUAL` |
| `9.c` | 19 lines | Structs, member access | `STRUCT` keyword, `.` (DOT) operator, array subscript with expression (`[20/4+5]`), chained assignment, `&&`, nested comparisons |

## Coverage summary

Across all 10 tests, the lexer is exercised on:

- **Every keyword**: `break` (none), `char`, `double`, `else`, `for`, `if`, `int`, `return`, `struct`, `void`, `while` (none). Two keywords (`break`, `while`) aren't used by the current suite — the lexer handles them but they're untested end-to-end.
- **Every token class**: `ID`, `CT_INT`, `CT_REAL`, `CT_CHAR`, `CT_STRING`, all delimiters, all operators.
- **Every number format**: decimal, octal (`014`), hex (`0xc`, `0x2`), real with `.`, real with exponent (`2e0`, `20E-1`, `0.2e+1`).
- **All factorized operators**: `=` vs `==`, `<` vs `<=`, `>` vs `>=`, `!` vs `!=` (not used — `NOT` and `NOTEQ` aren't tested end-to-end), `/` vs `//` (comment), `&&`, `||` (not used — `OR` isn't tested end-to-end).
- **String / char escapes**: `\"`, `\t`, `\\` (tests/8.c).
- **Line comments** at the start and end of a file (tests/1.c).

Untested end-to-end: `BREAK`, `WHILE`, `NOT`, `NOTEQ`, `OR`. The lexer has code paths for all of them and the FSM logic is symmetric with the tested ones, but if you want to demonstrate them to the teacher, write a small `.c` with `while(...)`, `!x`, `x!=y`, and `a||b` and run `make run FILE=...`.

## Parser tests (`parser/`)

The files under `parser/ok/` and `parser/err/` are dedicated to the L4 syntactic analyzer (`make parse-test`). They are separate from `0.c`–`9.c` because some of those programs use C-style syntax that AtomC's formal grammar deliberately does not allow:

- Multi-declarator `int i, j, k;` — the rule is `varDef: typeBase ID arrayDecl? SEMICOLON` (one ID per declaration). Affects `tests/0.c`, `5.c`, `6.c`, `7.c`, `9.c`.
- Expression array bounds `v[20/4+5]` — the rule is `arrayDecl: LBRACKET CT_INT? RBRACKET` (only an integer constant, or empty). Affects `tests/9.c`.

`tests/parser/ok/*` cover variable declarations, control flow, structs (with a `CT_INT` array bound), the full expression precedence ladder, casts, unary chains, and function calls with 0/1/N arguments. `tests/parser/err/*` exercise the `tkerr` paths — missing `;`, missing `)`, missing `;` after `struct`, and a missing operand. Each error file's first comment line states the message the parser is expected to produce.

## Domain analysis tests (`domain/`)

`make domain-test` runs the L5 phase. `domain/ok/*` are programs whose declarations are all consistent — valid nested-scope shadowing, structs defined before use. `domain/err/*` are syntactically valid but break a domain rule: redefined variable / struct / function / parameter, a function-body local colliding with a parameter, an undefined struct type, and a vector variable with no dimension. Each `err` file's first comment line states the expected error.

## Type analysis tests (`types/`)

`make type-test` runs the L6 phase. `types/ok/*` are fully valid programs exercising implicit numeric conversions, casts, array indexing, and struct field access. `types/err/*` each break one type rule: undefined identifier, calling a non-function, using a function name as a value, too few / too many / wrong-typed call arguments, assigning to a non-l-value, returning a value from a `void` function, a non-scalar condition, indexing a non-array, and selecting a field of a non-struct. Each `err` file's first comment line states the expected error.

`make check` runs the lexer snapshots plus all three analyzer suites.
