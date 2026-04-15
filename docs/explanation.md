# AtomC Transition Diagram — How to Explain It

## What is this?

This is a **Transition Diagram (TD)** for the AtomC language lexical analyzer. It shows how the lexer reads input characters one by one and decides what **lexical atom (token)** it found.

Think of it like a flowchart the computer follows: it starts at state 0, reads a character, moves to the next state, and keeps going until it reaches a **final state** (double circle) — that means it recognized a token.

---

## The basics you need to know

### States (circles)
- **State 0** is always the starting point. After recognizing a token, you go back to state 0.
- Numbered states (1, 2, 3...) are intermediate — you're in the middle of recognizing something.
- **Double circles** = final/accepting states. When you reach one, you've found a complete token (like CT_INT, ID, COMMA, etc.)

### Transitions (arrows)
- Each arrow is labeled with the character(s) that trigger it. For example, `0-9` means "any digit."
- **Solid arrows** = consume the input character (you move forward in the input).
- **Dashed arrows labeled "else"** = you do NOT consume the character. It means "if nothing else matches, go here." The character stays and will be used in the next pass.

### Error states (pink rectangles)
- These are reached when the input is invalid. For example, seeing a letter right after a decimal point (`3.x`) is an error because a digit is expected.

---

## Section by section

### 1. Numbers: CT_INT and CT_REAL

This is the most complex part because integers and reals share the same beginning — they both start with digits. This is called **factorization** (extracting the common prefix).

**How it works:**
- From state 0, if you see a digit (0-9), you go to state 1.
- In state 1, more digits keep you in state 1 (the loop arrow).
- From state 1, three things can happen:
  - **You see something that's not a digit, dot, or e/E** → "else" → it's a **CT_INT** (like `42`, `100`).
  - **You see a dot `.`** → go to state 2 → you need digits after it → state 3 → it becomes a **CT_REAL** (like `3.14`).
  - **You see `e` or `E`** → go to state 4 → this is the scientific notation exponent (like `3e10`).

**The exponent path (states 4-5-6):**
- After `e`/`E`, you can optionally have `+` or `-` (state 5), then you must have digits (state 6).
- After the exponent digits → **CT_REAL** (like `3.14e-10`, `42E5`).

**Why factorization?** Because from state 0 you can't know yet if `123` will be an integer or the start of `123.45`. So you read the digits first (shared path), then the dot or lack of dot tells you which token it is.

**If asked "why not just have separate paths for INT and REAL?":** Because both start with `[0-9]+`. If we had two paths from state 0 both triggered by `0-9`, we'd have nondeterminism — we wouldn't know which path to follow. Factorizing ensures that from any state, with any character, there's only one possible transition.

### 2. Identifiers: ID

Simple:
- Starts with a letter or underscore (`a-zA-Z_`) → state 1.
- Continues with letters, digits, or underscores (loop on state 1).
- When you see something else → "else" → it's an **ID**.

**Keywords** (like `if`, `while`, `return`) are NOT shown on the TD. They look exactly like identifiers to the lexer. After recognizing an ID, you check: "is this word in the keywords list?" If yes, it becomes that keyword token instead.

### 3. Constants: CT_CHAR and CT_STRING

**CT_CHAR** (`'x'`):
- See `'` → state 1
- See any character that isn't `'` → state 2
- See closing `'` → final state **CT_CHAR**
- Errors: empty char `''` or missing closing quote

**CT_STRING** (`"hello"`):
- See `"` → state 4
- Keep reading any character that isn't `"` (loop on state 4)
- See closing `"` → final state **CT_STRING**
- Error: reaching end of file `\0` before closing quote

### 4. Operators with shared prefixes

Same idea as numbers — some operators start with the same character, so we factorize:

- **`/`** → could be **DIV** (`/`) or start of **LINECOMMENT** (`//`)
  - See another `/` → you're in a line comment, consume everything until end of line, go back to state 0 (no token generated — comments are not tokens).
  - See anything else → "else" → it's **DIV**.

- **`!`** → could be **NOT** (`!`) or **NOTEQ** (`!=`)
  - See `=` after → **NOTEQ**
  - Else → **NOT**

- **`=`** → could be **ASSIGN** (`=`) or **EQUAL** (`==`)
- **`<`** → could be **LESS** (`<`) or **LESSEQ** (`<=`)
- **`>`** → could be **GREATER** (`>`) or **GREATEREQ** (`>=`)

All follow the same pattern: read first char, check if second char extends it, if not → "else" to the shorter token.

- **`&&`** and **`||`** — these MUST be two characters. A single `&` or `|` alone is an error in AtomC (unlike C where they have bitwise meaning).

### 5. Single-character delimiters and operators

These are the easy ones — one character, one token, no ambiguity:
- `,` → COMMA
- `;` → SEMICOLON
- `(` `)` → LPAR, RPAR
- `[` `]` → LBRACKET, RBRACKET
- `{` `}` → LACC, RACC
- `+` → ADD
- `-` → SUB
- `*` → MUL
- `.` → DOT

Note: `.` from state 0 is the DOT operator. The `.` inside numbers is handled separately (from state 1, not state 0), so there's no conflict.

### 6. Whitespace / SPACE

Spaces, tabs, newlines (`\n`, `\r`, `\t`) just loop on state 0. They're consumed but **no token is generated**. The lexer just skips them.

Same for LINECOMMENT — it ends back at state 0 without producing a token.

---

## Key concepts to remember if asked

**Q: Why are all diagrams separate but share state 0?**
A: The lab says that when a TD becomes too complex, you can split it into separate diagrams sharing the initial state (0) as the common point. They're logically one big diagram.

**Q: What does "else" mean?**
A: "In any other case" — it's like the default/else branch of an if. It does NOT consume the input character. It's tested last, after all labeled transitions.

**Q: What is factorization?**
A: When two tokens share a common prefix (like CT_INT and CT_REAL both starting with digits), we extract that shared beginning into common states. This avoids nondeterminism — we never have two transitions from the same state consuming the same character.

**Q: Why no keywords on the TD?**
A: Keywords (`if`, `while`, `break`, etc.) look identical to identifiers lexically. The lexer first recognizes them as ID, then does a lookup: if the text matches a keyword, it changes the token type. This keeps the TD much simpler.

**Q: What kind of automaton is this?**
A: It's based on NFA (Nondeterministic Finite Automaton) formalism because the "else" transitions are like epsilon-transitions. But the construction rules ensure it behaves deterministically — from any state with any input character, there's at most one valid transition to follow.

**Q: Why do final states have no outgoing transitions?**
A: Because once you recognize a token, you're done. You return the token and start fresh from state 0 for the next one.

**Q: Why do comments and spaces end at state 0 instead of a final state?**
A: Because they don't produce tokens. The next phase of the compiler (parser) doesn't need to know about spaces or comments. So the lexer just consumes them silently and keeps going.
