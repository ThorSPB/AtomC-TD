# AtomC-TD

Lexical analyzer design for the **AtomC** language — coursework for the *Compilation Techniques* labs (L2/L3).

This repo currently holds the transition-diagram (TD) specification and supporting materials. The lexer implementation itself (CT-L3) will be added here as the course progresses.

## Structure

```
.
├── docs/                  Markdown specs and walkthroughs
│   ├── AtomC_TD.md        Transition Diagram specification (Mermaid)
│   └── explanation.md     Plain-language walkthrough / defense prep
├── diagrams/
│   ├── src/               Graphviz .dot sources
│   └── rendered/          .png renders of the .dot sources
├── references/            Course PDFs (lab handouts, lexical rules)
└── tests/                 Sample AtomC programs (input for the lexer)
```

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

## Status

- [x] L2 — Transition Diagram designed and documented
- [ ] L3 — Lexer implementation (upcoming)
