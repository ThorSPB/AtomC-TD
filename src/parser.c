/*
 * AtomC syntactic analyzer (Recursive Descent Parser, L4).
 *
 * Each nonterminal in docs/AtomC_grammar.md is a static int predicate
 * here. A predicate returns 1 if it matched (consuming exactly its
 * tokens) or 0 if it did not (with crtTk restored to the entry value
 * if any tokens were tentatively consumed). When a predicate has
 * already committed to a rule and a required token is missing, it
 * calls perr(...) which reports the line and exits.
 *
 * Implements the *strict* AtomC grammar from
 *   references/AtomC - reguli sintactice.pdf
 * Notes on tested-against-our-files extensions:
 *   - The lex.c CT_CHAR/CT_STRING escape relaxation is documented in
 *     lex.c. The parser does not extend the grammar in any way; the
 *     lexer-test files tests/N.c use C extensions (multi-declarator
 *     `int a, b, c;`, expression array bounds `v[20/4+5]`) that are
 *     deliberately rejected here. See docs/AtomC_grammar.md §4 and
 *     tests/parser/ for parser-specific test inputs.
 */
#include "atomc.h"
#include <stdarg.h>

Token *crtTk = NULL;
Token *consumedTk = NULL;

int consume(int code) {
    if (crtTk && crtTk->code == code) {
        consumedTk = crtTk;
        crtTk = crtTk->next;
        return 1;
    }
    return 0;
}

/* Print a syntax error referencing the current token, then exit. */
static void perr(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "syntax error in line %d: ", crtTk->line);
    vfprintf(stderr, fmt, va);
    fprintf(stderr, " (got %s", tokenName(crtTk->code));
    switch (crtTk->code) {
    case ID:
    case CT_STRING: if (crtTk->text) fprintf(stderr, " '%s'", crtTk->text); break;
    case CT_INT:    fprintf(stderr, " %ld", crtTk->i); break;
    case CT_CHAR:   fprintf(stderr, " '%c'", (int)crtTk->i); break;
    case CT_REAL:   fprintf(stderr, " %g", crtTk->r); break;
    default: break;
    }
    fputc(')', stderr);
    fputc('\n', stderr);
    va_end(va);
    exit(-1);
}

/* Forward declarations — predicates call each other freely. */
static int unit(void);
static int structDef(void);
static int varDef(void);
static int typeBase(void);
static int arrayDecl(void);
static int fnDef(void);
static int fnParam(void);
static int stm(void);
static int stmCompound(void);
static int expr(void);
static int exprAssign(void);
static int exprOr(void);
static int exprAnd(void);
static int exprEq(void);
static int exprRel(void);
static int exprAdd(void);
static int exprMul(void);
static int exprCast(void);
static int exprUnary(void);
static int exprPostfix(void);
static int exprPrimary(void);

/* unit: ( structDef | fnDef | varDef )* END */
static int unit(void) {
    while (structDef() || fnDef() || varDef()) { }
    if (!consume(END)) perr("expected top-level definition or end of file");
    return 1;
}

/* structDef: STRUCT ID LACC varDef* RACC SEMICOLON
 * STRUCT also begins a typeBase, so commit only on the full STRUCT ID LACC prefix. */
static int structDef(void) {
    Token *startTk = crtTk;
    if (!consume(STRUCT))    return 0;
    if (!consume(ID))        { crtTk = startTk; return 0; }
    if (!consume(LACC))      { crtTk = startTk; return 0; }
    while (varDef()) { }
    if (!consume(RACC))      perr("expected '}' to close struct definition");
    if (!consume(SEMICOLON)) perr("expected ';' after struct definition");
    return 1;
}

/* varDef: typeBase ID arrayDecl? SEMICOLON */
static int varDef(void) {
    Token *startTk = crtTk;
    if (!typeBase())         return 0;
    if (!consume(ID))        { crtTk = startTk; return 0; }
    arrayDecl();             /* optional */
    if (!consume(SEMICOLON)) { crtTk = startTk; return 0; }
    return 1;
}

/* typeBase: INT | DOUBLE | CHAR | STRUCT ID
 * Once STRUCT is consumed an ID is required (anonymous structs are not in AtomC). */
static int typeBase(void) {
    if (consume(INT))    return 1;
    if (consume(DOUBLE)) return 1;
    if (consume(CHAR))   return 1;
    if (consume(STRUCT)) {
        if (!consume(ID)) perr("expected struct name after 'struct'");
        return 1;
    }
    return 0;
}

/* arrayDecl: LBRACKET CT_INT? RBRACKET */
static int arrayDecl(void) {
    if (!consume(LBRACKET))  return 0;
    consume(CT_INT);         /* optional */
    if (!consume(RBRACKET))  perr("expected ']' to close array declaration");
    return 1;
}

/* fnDef: ( typeBase | VOID ) ID LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
 *
 * Commit point: after we have ( typeBase|VOID ) ID LPAR. Before that we
 * must be willing to backtrack so varDef can be tried instead. VOID is
 * unambiguous on its own (only fnDef accepts it). */
static int fnDef(void) {
    Token *startTk = crtTk;
    if (consume(VOID)) {
        /* committed to fnDef the moment VOID is seen */
        if (!consume(ID))   perr("expected function name after 'void'");
        if (!consume(LPAR)) perr("expected '(' after function name");
    } else if (typeBase()) {
        if (!consume(ID))   { crtTk = startTk; return 0; }
        if (!consume(LPAR)) { crtTk = startTk; return 0; }
    } else {
        return 0;
    }
    /* committed: parameter list and body required */
    if (!consume(RPAR)) {
        if (!fnParam()) perr("expected parameter or ')'");
        while (consume(COMMA)) {
            if (!fnParam()) perr("expected parameter after ','");
        }
        if (!consume(RPAR)) perr("expected ')' to close parameter list");
    }
    if (!stmCompound()) perr("expected function body");
    return 1;
}

/* fnParam: typeBase ID arrayDecl? */
static int fnParam(void) {
    if (!typeBase()) return 0;
    if (!consume(ID)) perr("expected parameter name after type");
    arrayDecl();              /* optional */
    return 1;
}

/* stm: stmCompound
 *    | IF LPAR expr RPAR stm ( ELSE stm )?
 *    | WHILE LPAR expr RPAR stm
 *    | FOR LPAR expr? SEMICOLON expr? SEMICOLON expr? RPAR stm
 *    | BREAK SEMICOLON
 *    | RETURN expr? SEMICOLON
 *    | expr? SEMICOLON */
static int stm(void) {
    if (stmCompound()) return 1;

    if (consume(IF)) {
        if (!consume(LPAR)) perr("expected '(' after 'if'");
        if (!expr())        perr("expected expression in if condition");
        if (!consume(RPAR)) perr("expected ')' to close if condition");
        if (!stm())         perr("expected statement for if branch");
        if (consume(ELSE)) {
            if (!stm())     perr("expected statement for else branch");
        }
        return 1;
    }

    if (consume(WHILE)) {
        if (!consume(LPAR)) perr("expected '(' after 'while'");
        if (!expr())        perr("expected expression in while condition");
        if (!consume(RPAR)) perr("expected ')' to close while condition");
        if (!stm())         perr("expected statement for while body");
        return 1;
    }

    if (consume(FOR)) {
        if (!consume(LPAR))      perr("expected '(' after 'for'");
        expr();                  /* init: optional */
        if (!consume(SEMICOLON)) perr("expected ';' after for-init");
        expr();                  /* condition: optional */
        if (!consume(SEMICOLON)) perr("expected ';' after for-condition");
        expr();                  /* step: optional */
        if (!consume(RPAR))      perr("expected ')' to close for");
        if (!stm())              perr("expected statement for for body");
        return 1;
    }

    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) perr("expected ';' after 'break'");
        return 1;
    }

    if (consume(RETURN)) {
        expr();                  /* optional */
        if (!consume(SEMICOLON)) perr("expected ';' after return");
        return 1;
    }

    /* expr? SEMICOLON — empty statement is just `;` */
    {
        Token *startTk = crtTk;
        int hasExpr = expr();
        if (consume(SEMICOLON)) return 1;
        if (hasExpr) perr("expected ';' after expression");
        crtTk = startTk;
        return 0;
    }
}

/* stmCompound: LACC ( varDef | stm )* RACC */
static int stmCompound(void) {
    if (!consume(LACC)) return 0;
    while (varDef() || stm()) { }
    if (!consume(RACC)) perr("expected '}' to close compound statement");
    return 1;
}

/* expr: exprAssign */
static int expr(void) {
    return exprAssign();
}

/* exprAssign: exprOr ( ASSIGN exprAssign )?     // §4.3 — was: exprUnary ASSIGN exprAssign | exprOr
 * Right-associative: a = b = c parses as a = (b = c). */
static int exprAssign(void) {
    if (!exprOr()) return 0;
    if (consume(ASSIGN)) {
        if (!exprAssign()) perr("expected expression after '='");
    }
    return 1;
}

/* The six left-recursive arithmetic/relational rules become
 *   left-op-right while-loops. Once an operator is consumed, the right
 *   operand is required. */
static int exprOr(void) {
    if (!exprAnd()) return 0;
    while (consume(OR)) {
        if (!exprAnd()) perr("expected expression after '||'");
    }
    return 1;
}

static int exprAnd(void) {
    if (!exprEq()) return 0;
    while (consume(AND)) {
        if (!exprEq()) perr("expected expression after '&&'");
    }
    return 1;
}

static int exprEq(void) {
    if (!exprRel()) return 0;
    while (1) {
        if (consume(EQUAL)) {
            if (!exprRel()) perr("expected expression after '=='");
        } else if (consume(NOTEQ)) {
            if (!exprRel()) perr("expected expression after '!='");
        } else break;
    }
    return 1;
}

static int exprRel(void) {
    if (!exprAdd()) return 0;
    while (1) {
        if (consume(LESS)) {
            if (!exprAdd()) perr("expected expression after '<'");
        } else if (consume(LESSEQ)) {
            if (!exprAdd()) perr("expected expression after '<='");
        } else if (consume(GREATER)) {
            if (!exprAdd()) perr("expected expression after '>'");
        } else if (consume(GREATEREQ)) {
            if (!exprAdd()) perr("expected expression after '>='");
        } else break;
    }
    return 1;
}

static int exprAdd(void) {
    if (!exprMul()) return 0;
    while (1) {
        if (consume(ADD)) {
            if (!exprMul()) perr("expected expression after '+'");
        } else if (consume(SUB)) {
            if (!exprMul()) perr("expected expression after '-'");
        } else break;
    }
    return 1;
}

static int exprMul(void) {
    if (!exprCast()) return 0;
    while (1) {
        if (consume(MUL)) {
            if (!exprCast()) perr("expected expression after '*'");
        } else if (consume(DIV)) {
            if (!exprCast()) perr("expected expression after '/'");
        } else break;
    }
    return 1;
}

/* exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
 *
 * §4.2: peek past LPAR to disambiguate cast vs parenthesized primary.
 * INT/DOUBLE/CHAR/STRUCT are the only tokens that can start a typeBase
 * and none of them can start an expr, so the peek is unambiguous. */
static int exprCast(void) {
    if (crtTk->code == LPAR) {
        int next = crtTk->next ? crtTk->next->code : END;
        if (next == INT || next == DOUBLE || next == CHAR || next == STRUCT) {
            consume(LPAR);
            if (!typeBase())    perr("expected type in cast");
            arrayDecl();        /* optional */
            if (!consume(RPAR)) perr("expected ')' to close cast");
            if (!exprCast())    perr("expected expression after cast");
            return 1;
        }
    }
    return exprUnary();
}

/* exprUnary: ( SUB | NOT ) exprUnary | exprPostfix */
static int exprUnary(void) {
    if (consume(SUB) || consume(NOT)) {
        if (!exprUnary()) perr("expected expression after unary operator");
        return 1;
    }
    return exprPostfix();
}

/* exprPostfix: exprPrimary ( LBRACKET expr RBRACKET | DOT ID )* */
static int exprPostfix(void) {
    if (!exprPrimary()) return 0;
    while (1) {
        if (consume(LBRACKET)) {
            if (!expr())            perr("expected index expression after '['");
            if (!consume(RBRACKET)) perr("expected ']' to close index");
        } else if (consume(DOT)) {
            if (!consume(ID))       perr("expected field name after '.'");
        } else break;
    }
    return 1;
}

/* exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
 *            | CT_INT | CT_REAL | CT_CHAR | CT_STRING
 *            | LPAR expr RPAR */
static int exprPrimary(void) {
    if (consume(ID)) {
        if (consume(LPAR)) {                   /* function call */
            if (!consume(RPAR)) {
                if (!expr()) perr("expected argument or ')'");
                while (consume(COMMA)) {
                    if (!expr()) perr("expected expression after ','");
                }
                if (!consume(RPAR)) perr("expected ')' to close argument list");
            }
        }
        return 1;
    }
    if (consume(CT_INT))    return 1;
    if (consume(CT_REAL))   return 1;
    if (consume(CT_CHAR))   return 1;
    if (consume(CT_STRING)) return 1;
    if (consume(LPAR)) {
        if (!expr())        perr("expected expression after '('");
        if (!consume(RPAR)) perr("expected ')' to close parenthesized expression");
        return 1;
    }
    return 0;
}

void parse(Token *tokenList) {
    crtTk = tokenList;
    consumedTk = NULL;
    unit();
}
