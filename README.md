# AtomC-TD

Lexical analyzer for the **AtomC** language — coursework for the *Compilation Techniques* labs (L2/L3).

Contains both the transition-diagram (TD) design and a working C implementation of the lexer built directly from that TD.

## Structure

```
.
├── src/                   C source for the lexer
│   ├── atomc.h            Token codes, Token struct, API, SAFEALLOC
│   ├── lex.c              getNextToken() FSM — one case per TD state
│   └── main.c             Driver: read file, run lexer, print tokens
├── Makefile               gcc -Wall -Wextra -std=c99, output to build/
├── docs/                  Markdown specs and walkthroughs
│   ├── AtomC_TD.md        Transition Diagram specification (Mermaid)
│   └── explanation.md     Plain-language walkthrough / defense prep
├── diagrams/
│   ├── src/               Graphviz .dot sources
│   └── rendered/          .png renders of the .dot sources
├── references/            Course PDFs (lab handouts, lexical rules)
└── tests/                 Sample AtomC programs (input for the lexer)
```

## Building and running the lexer

```bash
make                         # build build/atomc
make run                     # build + run against tests/0.c (default)
make run FILE=tests/5.c      # run against a specific file
make test                    # build + run all tests and diff against tests/expected/
make clean                   # delete the build/ directory
```

Lexer output is one token per line in the form `<line>  <TOKEN_NAME>[:attribute]`, ending with `END`. Example:

```
   2  INT
   2  ID:sum
   2  LPAR
   2  RPAR
   ...
  21  END
```

`make test` compares each file's output against `tests/expected/<name>.txt` and prints a `PASS`/`FAIL` summary. This is the same check GitHub Actions runs on every push.

## Diagrams

The TD is described in two equivalent notations:

- **Mermaid**, embedded in `docs/AtomC_TD.md` — readable directly on GitHub.
- **Graphviz DOT**, in `diagrams/src/` — rendered to PNG in `diagrams/rendered/`.

The TD is split into six sub-diagrams that share state `0` as a common initial state: numbers, identifiers, char/string constants, multi-char operators, single-char delimiters, and whitespace. `full_td.dot` and `unified_td.dot` are the combined views.

### Rendering

After editing a `.dot` file:

```bash
dot -Tpng diagrams/src/full_td.dot -o diagrams/rendered/full_td.png
```

Requires Graphviz (`dot`) on `PATH`.

## References

`references/AtomC - reguli lexicale.pdf` is the authoritative source for what each token should match. `CT-L2.pdf` and `CT-L3.pdf` are the lab handouts (L2 = TD design, L3 = lexer implementation).

## Implementation notes

The lexer follows the "explicit-state FSM" approach from the L3 handout: one `switch` `case` per TD state, with `(else)` transitions implemented as fall-through `return` paths that do not consume the current character.

- **State numbering in `src/lex.c` matches `diagrams/src/unified_td.dot`** — each `case N:` corresponds to state `N` in the diagram. This is intentional so the code can be read next to the TD.
- **Numbers** — `CT_INT` supports decimal, octal (leading `0`), and hex (`0x…`). Internally, once the FSM has validated the digit shape, `strtol(buf, NULL, 0)` is used to convert, which auto-detects the base from the prefix. `CT_REAL` handles `.` and `e/E` with optional sign.
- **Keywords** — lexed as identifiers and reclassified via a small keyword table, per the lab's recommendation (so the TD stays simple).
- **Strings and chars** — the formal AtomC rules (`references/AtomC - reguli lexicale.pdf`) literally forbid escape sequences (`CT_CHAR: ['] [^'] [']`, `CT_STRING: ["] [^"]* ["]`). The provided `tests/8.c` relies on `\"`, `\t`, and `'\\'`, so the lexer extends these definitions with the standard C escape set (`\n \t \r \0 \\ \' \"`). The extension is localized to `decodeEscape` / `decodeString` in `lex.c`; the TD shape is unchanged.
- **Errors** — any invalid character, malformed hex prefix, exponent without digits, or lone `&`/`|` triggers `tkerr()`, which prints the offending line and exits. AtomC has no bitwise `&` or `|`, so those are errors by design.

## Status

- [x] L2 — Transition Diagram designed and documented
- [x] L3 — Lexer implementation (builds cleanly, all 10 sample programs in `tests/` lex successfully)
