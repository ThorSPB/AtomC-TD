# AtomC - Transition Diagram (TD)

**Conventions:**
- `(else)` transitions do **NOT** consume the input character
- Final states (double circles) are labeled with the token name
- SPACE and LINECOMMENT loop back to state 0 (no token generated)
- Keywords are **not** on the TD — identified as special cases of ID
- Error states shown as rectangles
- Diagrams share state **0** as the common initial state (as per lab rules for complex TDs)

---

## 1. Numbers: CT_INT / CT_REAL (factorized)

```mermaid
stateDiagram-v2
    direction LR
    [*] --> s0
    s0 --> s1 : 0-9
    s1 --> s1 : 0-9
    s1 --> s2 : .
    s1 --> s4 : e, E
    s1 --> CT_INT : (else)

    s2 --> s3 : 0-9
    s2 --> err1 : (else)
    s3 --> s3 : 0-9
    s3 --> s4 : e, E
    s3 --> CT_REAL : (else)

    s4 --> s5 : +, -
    s4 --> s6 : 0-9
    s4 --> err2 : (else)
    s5 --> s6 : 0-9
    s5 --> err2 : (else)
    s6 --> s6 : 0-9
    s6 --> CT_REAL : (else)

    state "ERR: digit expected after '.'" as err1
    state "ERR: digit/sign expected in exponent" as err2
```

---

## 2. Identifiers: ID

```mermaid
stateDiagram-v2
    direction LR
    [*] --> s0
    s0 --> s7 : a-zA-Z_
    s7 --> s7 : a-zA-Z0-9_
    s7 --> ID : (else)
```

---

## 3. Character and String Constants: CT_CHAR, CT_STRING

```mermaid
stateDiagram-v2
    direction LR
    [*] --> s0

    s0 --> s8 : '
    s8 --> s9 : [^']
    s8 --> err3 : '
    s9 --> CT_CHAR : '
    s9 --> err4 : [^']

    s0 --> s10 : "
    s10 --> s10 : [^"]
    s10 --> CT_STRING : "

    state "ERR: empty char constant" as err3
    state "ERR: missing closing quote" as err4
```

---

## 4. Operators with Shared Prefixes (factorized)

```mermaid
stateDiagram-v2
    direction LR
    [*] --> s0

    s0 --> s11 : /
    s11 --> s12 : /
    s11 --> DIV : (else)
    s12 --> s12 : [^\n\r\0]
    s12 --> s0 : \n, \r, \0
    note right of s12 : LINECOMMENT (no token)

    s0 --> s13 : !
    s13 --> NOTEQ : =
    s13 --> NOT : (else)

    s0 --> s14 : =
    s14 --> EQUAL : =
    s14 --> ASSIGN : (else)

    s0 --> s15 : <
    s15 --> LESSEQ : =
    s15 --> LESS : (else)

    s0 --> s16 : >
    s16 --> GREATEREQ : =
    s16 --> GREATER : (else)

    s0 --> s17 : &
    s17 --> AND : &
    s17 --> err6 : (else)

    s0 --> s18 : |
    s18 --> OR : |
    s18 --> err7 : (else)

    state "ERR: & expected" as err6
    state "ERR: | expected" as err7
```

---

## 5. Single-Character Delimiters and Operators

```mermaid
stateDiagram-v2
    direction LR
    [*] --> s0

    s0 --> COMMA : ,
    s0 --> SEMICOLON : ;
    s0 --> LPAR : (
    s0 --> RPAR : )
    s0 --> LBRACKET : [
    s0 --> RBRACKET : ]
    s0 --> LACC : {
    s0 --> RACC : }
    s0 --> ADD : +
    s0 --> SUB : -
    s0 --> MUL : *
    s0 --> DOT : .
```

---

## 6. Whitespace and SPACE (no token generated)

```mermaid
stateDiagram-v2
    direction LR
    [*] --> s0
    s0 --> s0 : space, \n, \r, \t
```

---

## State Summary

| State | Description |
|-------|-------------|
| s0 | Initial state (shared across all diagrams) |
| s1 | After `[0-9]+` — number prefix shared by CT_INT and CT_REAL |
| s2 | After `[0-9]+ .` — decimal point, digit must follow |
| s3 | After `[0-9]+ . [0-9]+` — valid decimal part |
| s4 | After `[eE]` — exponent start |
| s5 | After `[eE] [+-]` — exponent sign, digit must follow |
| s6 | After exponent `[0-9]+` — valid exponent digits |
| s7 | After `[a-zA-Z_]` — in identifier |
| s8 | After `'` — expecting char content |
| s9 | After `' [^']` — expecting closing `'` |
| s10 | After `"` — in string body |
| s11 | After `/` — DIV or LINECOMMENT |
| s12 | In line comment `//...` — consuming until end of line |
| s13 | After `!` — NOT or NOTEQ |
| s14 | After `=` — ASSIGN or EQUAL |
| s15 | After `<` — LESS or LESSEQ |
| s16 | After `>` — GREATER or GREATEREQ |
| s17 | After `&` — expecting second `&` |
| s18 | After `|` — expecting second `|` |

## Factorizations Applied

1. **CT_INT / CT_REAL** — both start with `[0-9]+` → factorized through s1
2. **DIV / LINECOMMENT** — both start with `/` → factorized through s11
3. **NOT / NOTEQ** — both start with `!` → factorized through s13
4. **ASSIGN / EQUAL** — both start with `=` → factorized through s14
5. **LESS / LESSEQ** — both start with `<` → factorized through s15
6. **GREATER / GREATEREQ** — both start with `>` → factorized through s16
