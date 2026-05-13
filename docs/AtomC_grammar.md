# AtomC — Grammar transformation for Recursive Descent Parsing

This document is the **grammar half** of L4. It takes the rules from
`references/AtomC - reguli sintactice.pdf` and rewrites them so a
**Recursive Descent Parser (RDP)** can implement them directly:

1. **Eliminate left recursion** — a left-recursive rule `A ::= A α | β`
   would translate to a function whose first call is itself, with no
   token consumed in between → infinite recursion.
2. **Factor common prefixes** — if two alternatives both start with `α`,
   the parser cannot pick between them just from the current token.
3. **Note the ambiguities that survive** the rewriting — these need
   either a peek (look at the next token without consuming) or a
   backtrack (`startTk = crtTk; … crtTk = startTk;`) in the C code.

The original rules and the rewritten rules are equivalent: they accept
the same language. Only the *shape* changes.

---

## 1. Original rules (from the PDF)

```
unit:        ( structDef | fnDef | varDef )* END
structDef:   STRUCT ID LACC varDef* RACC SEMICOLON
varDef:      typeBase ID arrayDecl? SEMICOLON
typeBase:    INT | DOUBLE | CHAR | STRUCT ID
arrayDecl:   LBRACKET CT_INT? RBRACKET
fnDef:       ( typeBase | VOID ) ID LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
fnParam:     typeBase ID arrayDecl?

stm:         stmCompound
           | IF LPAR expr RPAR stm ( ELSE stm )?
           | WHILE LPAR expr RPAR stm
           | FOR LPAR expr? SEMICOLON expr? SEMICOLON expr? RPAR stm
           | BREAK SEMICOLON
           | RETURN expr? SEMICOLON
           | expr? SEMICOLON
stmCompound: LACC ( varDef | stm )* RACC

expr:        exprAssign
exprAssign:  exprUnary ASSIGN exprAssign | exprOr
exprOr:      exprOr OR exprAnd | exprAnd
exprAnd:     exprAnd AND exprEq | exprEq
exprEq:      exprEq ( EQUAL | NOTEQ ) exprRel | exprRel
exprRel:     exprRel ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd | exprAdd
exprAdd:     exprAdd ( ADD | SUB ) exprMul | exprMul
exprMul:     exprMul ( MUL | DIV ) exprCast | exprCast
exprCast:    LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
exprUnary:   ( SUB | NOT ) exprUnary | exprPostfix
exprPostfix: exprPostfix LBRACKET expr RBRACKET
           | exprPostfix DOT ID
           | exprPrimary
exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
           | CT_INT | CT_REAL | CT_CHAR | CT_STRING
           | LPAR expr RPAR
```

The rules in **bold trouble** for an RDP are:

- Left-recursive: `exprOr`, `exprAnd`, `exprEq`, `exprRel`, `exprAdd`,
  `exprMul`, `exprPostfix`.
- Common-prefix at top level: `varDef` and `fnDef` both start with
  `typeBase ID …`.
- `LPAR` ambiguity: an `LPAR` inside `exprCast` may be the start of a
  cast `(int)x` *or* the start of a parenthesized primary `(x)`.
- `exprAssign`: the LHS form `exprUnary ASSIGN exprAssign` shares its
  prefix with `exprOr`, since `exprUnary` is reachable from `exprOr`.

The rest of this document handles each one.

---

## 2. Left recursion → while-loop form

The mechanical formula from the L4 handout:

```
A  ::= A α | β        →     A  ::= β A'
                            A' ::= α A' | ε
```

Applied to AtomC, that gives `exprOr1`, `exprAnd1`, `exprEq1`,
`exprRel1`, `exprAdd1`, `exprMul1`, `exprPostfix1` rules. They are
correct, but **we don't need to write a separate predicate** for each
prime rule — the equivalent C code is a `while` loop inside the parent
predicate. That is how the parser will be implemented.

Both forms are listed below so the equivalence is explicit.

### exprOr / exprAnd / exprEq / exprRel / exprAdd / exprMul

Same shape — only the operator(s) differ.

```
# Mechanical (handout formula):
exprOr  ::= exprAnd exprOr1
exprOr1 ::= OR exprAnd exprOr1 | ε

# Implementation form (used in parser.c):
exprOr  ::= exprAnd ( OR exprAnd )*
```

| Rule       | Operator(s)                              |
| ---------- | ---------------------------------------- |
| `exprOr`   | `OR`                                     |
| `exprAnd`  | `AND`                                    |
| `exprEq`   | `EQUAL` \| `NOTEQ`                       |
| `exprRel`  | `LESS` \| `LESSEQ` \| `GREATER` \| `GREATEREQ` |
| `exprAdd`  | `ADD` \| `SUB`                           |
| `exprMul`  | `MUL` \| `DIV`                           |

All six are **left-associative**, which is the natural reading the
while-loop form preserves: `a - b - c` parses as `(a - b) - c`, the
same way the original left-recursive rule would have built the tree.

### exprPostfix

```
# Mechanical:
exprPostfix  ::= exprPrimary exprPostfix1
exprPostfix1 ::= LBRACKET expr RBRACKET exprPostfix1
              |  DOT ID exprPostfix1
              |  ε

# Implementation form:
exprPostfix  ::= exprPrimary ( LBRACKET expr RBRACKET | DOT ID )*
```

---

## 3. Optional / repetition rewrites already in EBNF

A few rules are already in a parser-friendly EBNF form (they use `?`,
`*`, `+`). They map to C as follows — no transformation needed, just
noted here for completeness:

| EBNF construct   | C pattern                                   |
| ---------------- | ------------------------------------------- |
| `a?`             | `a();` (ignore return value)                |
| `a*`             | `while (a()) { }`                           |
| `( a \| b )*`    | `while (a() \|\| b()) { }`                  |
| `a ( SEP a )*`   | `if (a()) while (consume(SEP)) { if (!a()) tkerr(...); }` |

So `unit`, `structDef`, `arrayDecl`, `fnParam` list, `stmCompound`,
`exprPrimary` arg list, etc. all translate directly.

---

## 4. Surviving ambiguities (resolved in the parser, not the grammar)

These are cases where the rewritten grammar is still ambiguous from a
single-token lookahead. Each one is resolved in `parser.c` either by
**peeking** (looking at `crtTk->next->code` without consuming) or by
**backtracking** (`Token *startTk = crtTk; … crtTk = startTk; return 0;`).

### 4.1 `varDef` vs `fnDef` inside `unit`

```
varDef: typeBase ID arrayDecl? SEMICOLON
fnDef:  ( typeBase | VOID ) ID LPAR ...
```

After consuming `typeBase ID`, both are still possible. Resolution:

- **Strategy used:** backtrack. Try `fnDef` first. If after `typeBase ID`
  we don't see `LPAR`, restore `crtTk` and try `varDef`.
- Equivalent peek-strategy: after consuming `typeBase`, peek the token
  *after* the `ID` — `LPAR` ⇒ `fnDef`, anything else ⇒ `varDef`.

The `VOID` case is unambiguous: only `fnDef` accepts `VOID`, so a leading
`VOID` immediately commits to `fnDef`.

The factored form, written out for the record:

```
unit:    ( structDef | fnOrVarDef )* END
fnOrVarDef:  VOID ID fnDefRest
          |  typeBase ID ( fnDefRest | varDefRest )
fnDefRest:   LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
varDefRest:  arrayDecl? SEMICOLON
```

We **do not** rewrite the grammar this way in the doc tree we hand in
(the original three-rule shape is what the PDF specifies). The
backtrack inside `unit` does the same job.

### 4.2 `exprCast` vs `( expr )` in `exprPrimary`

Both start with `LPAR`:

```
exprCast:    LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
exprPrimary: ... | LPAR expr RPAR
```

Resolution:

- **Strategy used:** peek. After seeing `LPAR`, look at the next
  token. If it is one of `INT`, `DOUBLE`, `CHAR`, or `STRUCT`, it must
  be a cast — commit to the cast branch. Otherwise it is a
  parenthesized expression — fall through to `exprUnary`.
- This works because `typeBase` always begins with one of those four
  keywords, and none of them can start an `expr`.

### 4.3 `exprAssign`: LHS form vs `exprOr`

```
exprAssign: exprUnary ASSIGN exprAssign | exprOr
```

`exprUnary` is reachable from `exprOr` via the chain
`exprOr → exprAnd → … → exprCast → exprUnary`. So testing `exprUnary`
first and then `ASSIGN` would consume tokens that we'd have to
re-parse if no `ASSIGN` follows.

Resolution:

- **Strategy used:** parse `exprOr` first. After it succeeds, if the
  current token is `ASSIGN`, consume it and require another
  `exprAssign` on the right (right-associative: `a = b = c` parses as
  `a = (b = c)`).
- Semantic check (whether the LHS is a valid l-value, i.e. reducible
  to `exprUnary`) is a job for the next compiler phase, not the
  parser.

### 4.4 `expr? SEMICOLON` (empty statement)

```
stm: ... | expr? SEMICOLON
```

The `?` means "try to parse an `expr`; if there isn't one, that's
fine, just consume the `;`". Implementation: call `expr()` and ignore
the return value, then require `SEMICOLON`. This is a textbook
optional, not a real ambiguity.

The same shape appears for the three `expr?`s inside `for(...)` and
the `expr?` in `RETURN expr? SEMICOLON`.

---

## 5. Final transformed grammar

This is the grammar the parser actually implements. Left recursion
gone, EBNF where it makes the C straightforward, ambiguities flagged
in the comments next to the rule that resolves them.

```
unit:        ( structDef | fnDef | varDef )* END        // §4.1: backtrack between fnDef and varDef

structDef:   STRUCT ID LACC varDef* RACC SEMICOLON
varDef:      typeBase ID arrayDecl? SEMICOLON
typeBase:    INT | DOUBLE | CHAR | STRUCT ID
arrayDecl:   LBRACKET CT_INT? RBRACKET
fnDef:       ( typeBase | VOID ) ID LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
fnParam:     typeBase ID arrayDecl?

stm:         stmCompound
           | IF LPAR expr RPAR stm ( ELSE stm )?
           | WHILE LPAR expr RPAR stm
           | FOR LPAR expr? SEMICOLON expr? SEMICOLON expr? RPAR stm
           | BREAK SEMICOLON
           | RETURN expr? SEMICOLON
           | expr? SEMICOLON                            // §4.4
stmCompound: LACC ( varDef | stm )* RACC

expr:        exprAssign
exprAssign:  exprOr ( ASSIGN exprAssign )?              // §4.3 — was: exprUnary ASSIGN exprAssign | exprOr

exprOr:      exprAnd ( OR exprAnd )*                    // §2
exprAnd:     exprEq ( AND exprEq )*                     // §2
exprEq:      exprRel ( ( EQUAL | NOTEQ ) exprRel )*     // §2
exprRel:     exprAdd ( ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd )*   // §2
exprAdd:     exprMul ( ( ADD | SUB ) exprMul )*         // §2
exprMul:     exprCast ( ( MUL | DIV ) exprCast )*       // §2

exprCast:    LPAR typeBase arrayDecl? RPAR exprCast     // §4.2: peek next token after LPAR
           | exprUnary
exprUnary:   ( SUB | NOT ) exprUnary | exprPostfix

exprPostfix: exprPrimary ( LBRACKET expr RBRACKET | DOT ID )*               // §2

exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
           | CT_INT | CT_REAL | CT_CHAR | CT_STRING
           | LPAR expr RPAR
```

---

## 6. Error-vs-return-zero policy

The L4 handout gives the rule plainly: **if only one possibility
exists and it doesn't match, generate an error; if multiple
possibilities exist, return 0 so siblings can be tried; if none of the
siblings matches, the *parent* generates the error.**

Concretely in this grammar:

- After committing to a keyword that uniquely identifies a rule
  (`WHILE`, `IF`, `FOR`, `RETURN`, `BREAK`, `STRUCT` at unit level), the
  rest of the rule is required → use `tkerr` on missing tokens.
- After committing to `LPAR` in a cast (we peeked a `typeBase`
  follows), the rest is required → `tkerr`.
- Inside `unit`, the three alternatives `structDef`, `fnDef`, `varDef`
  each return 0 on no match; only when all three fail and the current
  token is not `END` does `unit` itself emit an error.

This is what "tkerr aborts + better messages" (the error mode chosen
for this milestone) means in practice: every `tkerr` call carries a
message that names the **expected token** and gets the **actual token
text and line number** from the `Token` it is given.
