/*
 * AtomC syntactic analyzer (Recursive Descent Parser) + semantic actions.
 *
 * L4 — the predicates: one static int per nonterminal. Returns 1 on a
 *      match (consuming exactly its tokens), 0 on no match (crtTk restored
 *      if any tokens were tentatively consumed). Past a commit point a
 *      missing token calls perr(...) which reports and exits.
 *
 * L5 — domain analysis: semantic actions in the declaration predicates
 *      (structDef/varDef/typeBase/arrayDecl/fnDef/fnParam/stmCompound)
 *      build the symbol table and reject redefinitions, undefined struct
 *      types, and sized-vector violations.
 *
 * L6 — type analysis: every expression predicate carries a synthesized
 *      Ret attribute (type + lval + const flags). The actions check that
 *      operands are scalar, conversions are legal, calls match their
 *      signatures, l-values are assignable, and so on.
 *
 * Semantic (domain/type) errors are reported with tkerr(); syntax errors
 * with perr(). Grammar: docs/AtomC_grammar.md. Rules: references/AtomC -
 * analiza de domeniu.pdf (L5) and AtomC - analiza de tipuri.pdf (L6).
 */
#include "atomc.h"
#include "symbols.h"
#include <stdarg.h>

Token *crtTk = NULL;
Token *consumedTk = NULL;

/* The struct or function currently being defined/analyzed — NULL at global
 * scope. Declaration actions consult it to attach members/params; the
 * RETURN action consults it for the enclosing function's return type. */
static Symbol *owner = NULL;

int consume(int code) {
    if (crtTk && crtTk->code == code) {
        consumedTk = crtTk;
        crtTk = crtTk->next;
        return 1;
    }
    return 0;
}

/* Syntax error at the current token: names what was expected, exits. */
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
static int typeBase(Type *ret);
static int arrayDecl(Type *ret);
static int fnDef(void);
static int fnParam(void);
static int stm(void);
static int stmCompound(int newDomain);
static int expr(Ret *r);
static int exprAssign(Ret *r);
static int exprOr(Ret *r);
static int exprAnd(Ret *r);
static int exprEq(Ret *r);
static int exprRel(Ret *r);
static int exprAdd(Ret *r);
static int exprMul(Ret *r);
static int exprCast(Ret *r);
static int exprUnary(Ret *r);
static int exprPostfix(Ret *r);
static int exprPrimary(Ret *r);

/* unit: ( structDef | fnDef | varDef )* END */
static int unit(void) {
    while (structDef() || fnDef() || varDef()) { }
    if (!consume(END)) perr("expected top-level definition or end of file");
    return 1;
}

/* structDef: STRUCT ID LACC varDef* RACC SEMICOLON
 * STRUCT also begins a typeBase, so commit only on the full STRUCT ID LACC
 * prefix. L5: the struct name is declared in the *enclosing* domain; its
 * fields go in a fresh domain opened for the body. */
static int structDef(void) {
    Token *startTk = crtTk;
    if (!consume(STRUCT))    return 0;
    if (!consume(ID))        { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;
    if (!consume(LACC))      { crtTk = startTk; return 0; }

    /* committed — declare the struct */
    if (findSymbolInDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);
    Symbol *s = newSymbol(tkName->text, SK_STRUCT);
    s->type = createType(TB_STRUCT, -1);
    s->type.s = s;                       /* a struct's type refers to itself */
    s->owner = owner;
    addSymbolToDomain(s);
    Symbol *outerOwner = owner;
    owner = s;
    pushDomain();

    while (varDef()) { }
    if (!consume(RACC))      perr("expected '}' to close struct definition");
    if (!consume(SEMICOLON)) perr("expected ';' after struct definition");

    dropDomain();
    owner = outerOwner;
    return 1;
}

/* varDef: typeBase ID arrayDecl? SEMICOLON
 * L5: the name must be unique in its domain; a sized-array form is
 * required (int v[] is rejected for variables). A field of a struct is
 * also recorded on the struct's member list. */
static int varDef(void) {
    Token *startTk = crtTk;
    Type t;
    if (!typeBase(&t))       return 0;
    if (!consume(ID))        { crtTk = startTk; return 0; }
    Token *tkName = consumedTk;
    if (arrayDecl(&t)) {
        if (t.n == 0) tkerr(tkName, "a vector variable must have a specified dimension");
    }
    if (!consume(SEMICOLON)) { crtTk = startTk; return 0; }

    /* committed — declare the variable */
    if (findSymbolInDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);
    Symbol *var = newSymbol(tkName->text, SK_VAR);
    var->type = t;
    var->owner = owner;
    addSymbolToDomain(var);
    if (owner && owner->kind == SK_STRUCT)
        addSymbolTo(&owner->members, var);
    return 1;
}

/* typeBase: INT | DOUBLE | CHAR | STRUCT ID
 * L5: a STRUCT type must name a struct already defined in scope. */
static int typeBase(Type *ret) {
    *ret = createType(TB_INT, -1);
    if (consume(INT))    { ret->tb = TB_INT;    return 1; }
    if (consume(DOUBLE)) { ret->tb = TB_DOUBLE; return 1; }
    if (consume(CHAR))   { ret->tb = TB_CHAR;   return 1; }
    if (consume(STRUCT)) {
        if (!consume(ID)) perr("expected struct name after 'struct'");
        Token *tkName = consumedTk;
        ret->tb = TB_STRUCT;
        ret->s = findSymbol(tkName->text);
        if (!ret->s)                     tkerr(tkName, "undefined struct: %s", tkName->text);
        if (ret->s->kind != SK_STRUCT)   tkerr(tkName, "%s is not a struct type", tkName->text);
        return 1;
    }
    return 0;
}

/* arrayDecl: LBRACKET CT_INT? RBRACKET
 * L5: records the element count — n>0 sized, n==0 unsized. */
static int arrayDecl(Type *ret) {
    if (!consume(LBRACKET)) return 0;
    if (consume(CT_INT)) ret->n = (int)consumedTk->i;
    else                 ret->n = 0;
    if (!consume(RBRACKET)) perr("expected ']' to close array declaration");
    return 1;
}

/* fnDef: ( typeBase | VOID ) ID LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
 * Commit point: ( typeBase|VOID ) ID LPAR; VOID alone commits. L5: the
 * function scope opens right after LPAR and the body reuses it (the body's
 * braces do NOT start a further subdomain). */
static int fnDef(void) {
    Token *startTk = crtTk;
    Type t;
    Token *tkName;
    if (consume(VOID)) {
        t = createType(TB_VOID, -1);
        if (!consume(ID))   perr("expected function name after 'void'");
        tkName = consumedTk;
        if (!consume(LPAR)) perr("expected '(' after function name");
    } else if (typeBase(&t)) {
        if (!consume(ID))   { crtTk = startTk; return 0; }
        tkName = consumedTk;
        if (!consume(LPAR)) { crtTk = startTk; return 0; }
    } else {
        return 0;
    }

    /* committed — declare the function, open its scope */
    if (findSymbolInDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);
    Symbol *fn = newSymbol(tkName->text, SK_FN);
    fn->type = t;
    fn->owner = owner;
    addSymbolToDomain(fn);
    Symbol *outerOwner = owner;
    owner = fn;
    pushDomain();

    if (!consume(RPAR)) {
        if (!fnParam()) perr("expected parameter or ')'");
        while (consume(COMMA)) {
            if (!fnParam()) perr("expected parameter after ','");
        }
        if (!consume(RPAR)) perr("expected ')' to close parameter list");
    }
    if (!stmCompound(0)) perr("expected function body");   /* 0 = reuse the fn's domain */

    dropDomain();
    owner = outerOwner;
    return 1;
}

/* fnParam: typeBase ID arrayDecl?
 * L5: unique in the function scope; a parameter array decays to unsized
 * (int v[10] -> int v[]). Recorded in the domain AND on the function. */
static int fnParam(void) {
    Type t;
    if (!typeBase(&t)) return 0;
    if (!consume(ID))  perr("expected parameter name after type");
    Token *tkName = consumedTk;
    if (arrayDecl(&t)) t.n = 0;        /* a parameter array loses its size */

    if (findSymbolInDomain(tkName->text))
        tkerr(tkName, "symbol redefinition: %s", tkName->text);
    Symbol *param = newSymbol(tkName->text, SK_PARAM);
    param->type = t;
    param->owner = owner;
    addSymbolToDomain(param);
    addSymbolTo(&owner->params, param);
    return 1;
}

/* stm: stmCompound
 *    | IF LPAR expr RPAR stm ( ELSE stm )?
 *    | WHILE LPAR expr RPAR stm
 *    | FOR LPAR expr? SEMICOLON expr? SEMICOLON expr? RPAR stm
 *    | BREAK SEMICOLON
 *    | RETURN expr? SEMICOLON
 *    | expr? SEMICOLON
 * L6: conditions must be scalar; RETURN is checked against owner's type. */
static int stm(void) {
    if (stmCompound(1)) return 1;   /* a { } block opens its own subdomain */

    if (consume(IF)) {
        Ret rCond;
        if (!consume(LPAR)) perr("expected '(' after 'if'");
        if (!expr(&rCond))  perr("expected expression in if condition");
        if (!canBeScalar(&rCond)) tkerr(crtTk, "the if condition must be a scalar value");
        if (!consume(RPAR)) perr("expected ')' to close if condition");
        if (!stm())         perr("expected statement for if branch");
        if (consume(ELSE)) {
            if (!stm())     perr("expected statement for else branch");
        }
        return 1;
    }

    if (consume(WHILE)) {
        Ret rCond;
        if (!consume(LPAR)) perr("expected '(' after 'while'");
        if (!expr(&rCond))  perr("expected expression in while condition");
        if (!canBeScalar(&rCond)) tkerr(crtTk, "the while condition must be a scalar value");
        if (!consume(RPAR)) perr("expected ')' to close while condition");
        if (!stm())         perr("expected statement for while body");
        return 1;
    }

    if (consume(FOR)) {
        Ret r;
        if (!consume(LPAR))      perr("expected '(' after 'for'");
        expr(&r);                /* init: optional */
        if (!consume(SEMICOLON)) perr("expected ';' after for-init");
        if (expr(&r)) {          /* condition: optional, but must be scalar if present */
            if (!canBeScalar(&r)) tkerr(crtTk, "the for condition must be a scalar value");
        }
        if (!consume(SEMICOLON)) perr("expected ';' after for-condition");
        expr(&r);                /* step: optional */
        if (!consume(RPAR))      perr("expected ')' to close for");
        if (!stm())              perr("expected statement for for body");
        return 1;
    }

    if (consume(BREAK)) {
        if (!consume(SEMICOLON)) perr("expected ';' after 'break'");
        return 1;
    }

    if (consume(RETURN)) {
        Ret rExpr;
        if (expr(&rExpr)) {
            if (owner->type.tb == TB_VOID)
                tkerr(crtTk, "a void function cannot return a value");
            if (!canBeScalar(&rExpr))
                tkerr(crtTk, "the return value must be a scalar value");
            if (!convTo(&rExpr.type, &owner->type))
                tkerr(crtTk, "the return type cannot be converted to the function's return type");
        } else {
            if (owner->type.tb != TB_VOID)
                tkerr(crtTk, "a non-void function must return a value");
        }
        if (!consume(SEMICOLON)) perr("expected ';' after return");
        return 1;
    }

    /* expr? SEMICOLON — empty statement is just `;` */
    {
        Token *startTk = crtTk;
        Ret rExpr;
        int hasExpr = expr(&rExpr);
        if (consume(SEMICOLON)) return 1;
        if (hasExpr) perr("expected ';' after expression");
        crtTk = startTk;
        return 0;
    }
}

/* stmCompound: LACC ( varDef | stm )* RACC
 * newDomain: 1 for a { } block statement (its own scope); 0 for a function
 * body (it reuses the scope opened by fnDef). */
static int stmCompound(int newDomain) {
    if (!consume(LACC)) return 0;
    if (newDomain) pushDomain();
    while (varDef() || stm()) { }
    if (!consume(RACC)) perr("expected '}' to close compound statement");
    if (newDomain) dropDomain();
    return 1;
}

/* expr: exprAssign */
static int expr(Ret *r) {
    return exprAssign(r);
}

/* exprAssign: exprOr ( ASSIGN exprAssign )?     // §4.3 — right-associative
 * L6: the destination must be a writable scalar l-value and the source
 * must be a scalar convertible to it. */
static int exprAssign(Ret *r) {
    if (!exprOr(r)) return 0;
    if (consume(ASSIGN)) {
        Ret src;
        if (!exprAssign(&src)) perr("expected expression after '='");
        if (!r->lval)        tkerr(crtTk, "the assignment destination must be a left-value");
        if (r->ct)           tkerr(crtTk, "the assignment destination cannot be constant");
        if (!canBeScalar(r)) tkerr(crtTk, "the assignment destination must be scalar");
        if (!canBeScalar(&src)) tkerr(crtTk, "the assignment source must be scalar");
        if (!convTo(&src.type, &r->type))
            tkerr(crtTk, "the assignment source cannot be converted to the destination type");
        /* result: the source's type, an rvalue */
        *r = src;
        r->lval = 0;
        r->ct = 1;
    }
    return 1;
}

/* The left-recursive logical/relational rules: result is always int.
 * Once an operator is consumed, the right operand is required. */
static int exprOr(Ret *r) {
    if (!exprAnd(r)) return 0;
    while (consume(OR)) {
        Ret right;
        Type dst;
        if (!exprAnd(&right)) perr("expected expression after '||'");
        if (!arithTypeTo(&r->type, &right.type, &dst))
            tkerr(crtTk, "invalid operand type for '||'");
        *r = (Ret){ createType(TB_INT, -1), 0, 1 };
    }
    return 1;
}

static int exprAnd(Ret *r) {
    if (!exprEq(r)) return 0;
    while (consume(AND)) {
        Ret right;
        Type dst;
        if (!exprEq(&right)) perr("expected expression after '&&'");
        if (!arithTypeTo(&r->type, &right.type, &dst))
            tkerr(crtTk, "invalid operand type for '&&'");
        *r = (Ret){ createType(TB_INT, -1), 0, 1 };
    }
    return 1;
}

static int exprEq(Ret *r) {
    if (!exprRel(r)) return 0;
    while (crtTk->code == EQUAL || crtTk->code == NOTEQ) {
        const char *op = crtTk->code == EQUAL ? "==" : "!=";
        consume(crtTk->code);
        Ret right;
        Type dst;
        if (!exprRel(&right)) perr("expected expression after '%s'", op);
        if (!arithTypeTo(&r->type, &right.type, &dst))
            tkerr(crtTk, "invalid operand type for '%s'", op);
        *r = (Ret){ createType(TB_INT, -1), 0, 1 };
    }
    return 1;
}

static int exprRel(Ret *r) {
    if (!exprAdd(r)) return 0;
    while (crtTk->code == LESS || crtTk->code == LESSEQ ||
           crtTk->code == GREATER || crtTk->code == GREATEREQ) {
        consume(crtTk->code);
        Ret right;
        Type dst;
        if (!exprAdd(&right)) perr("expected expression after a relational operator");
        if (!arithTypeTo(&r->type, &right.type, &dst))
            tkerr(crtTk, "invalid operand type for a relational operator");
        *r = (Ret){ createType(TB_INT, -1), 0, 1 };
    }
    return 1;
}

/* The left-recursive arithmetic rules: result is the promoted operand type
 * (double if either operand is double, else int). */
static int exprAdd(Ret *r) {
    if (!exprMul(r)) return 0;
    while (crtTk->code == ADD || crtTk->code == SUB) {
        consume(crtTk->code);
        Ret right;
        Type dst;
        if (!exprMul(&right)) perr("expected expression after '+' or '-'");
        if (!arithTypeTo(&r->type, &right.type, &dst))
            tkerr(crtTk, "invalid operand type for '+' or '-'");
        *r = (Ret){ dst, 0, 1 };
    }
    return 1;
}

static int exprMul(Ret *r) {
    if (!exprCast(r)) return 0;
    while (crtTk->code == MUL || crtTk->code == DIV) {
        consume(crtTk->code);
        Ret right;
        Type dst;
        if (!exprCast(&right)) perr("expected expression after '*' or '/'");
        if (!arithTypeTo(&r->type, &right.type, &dst))
            tkerr(crtTk, "invalid operand type for '*' or '/'");
        *r = (Ret){ dst, 0, 1 };
    }
    return 1;
}

/* exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
 * §4.2: peek past LPAR — a typeBase keyword means cast, else fall through.
 * L6: structs cannot take part in a cast; array-ness must be preserved. */
static int exprCast(Ret *r) {
    if (crtTk->code == LPAR) {
        int next = crtTk->next ? crtTk->next->code : END;
        if (next == INT || next == DOUBLE || next == CHAR || next == STRUCT) {
            Type t;
            Ret op;
            consume(LPAR);
            if (!typeBase(&t))  perr("expected type in cast");
            arrayDecl(&t);      /* optional */
            if (!consume(RPAR)) perr("expected ')' to close cast");
            if (!exprCast(&op)) perr("expected expression after cast");
            if (t.tb == TB_STRUCT)
                tkerr(crtTk, "cannot cast to a struct type");
            if (op.type.tb == TB_STRUCT)
                tkerr(crtTk, "cannot cast a struct value");
            if (op.type.n >= 0 && t.n < 0)
                tkerr(crtTk, "an array can be cast only to another array");
            if (op.type.n < 0 && t.n >= 0)
                tkerr(crtTk, "a scalar can be cast only to another scalar");
            *r = (Ret){ t, 0, 1 };
            return 1;
        }
    }
    return exprUnary(r);
}

/* exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
 * L6: the operand of a unary operator must be scalar. */
static int exprUnary(Ret *r) {
    if (consume(SUB) || consume(NOT)) {
        if (!exprUnary(r)) perr("expected expression after a unary operator");
        if (!canBeScalar(r)) tkerr(crtTk, "a unary operator requires a scalar operand");
        r->lval = 0;
        r->ct = 1;
        return 1;
    }
    return exprPostfix(r);
}

/* exprPostfix: exprPrimary ( LBRACKET expr RBRACKET | DOT ID )*
 * L6: only arrays may be indexed; '.' selects an existing struct field. */
static int exprPostfix(Ret *r) {
    if (!exprPrimary(r)) return 0;
    while (1) {
        if (consume(LBRACKET)) {
            Ret idx;
            if (!expr(&idx))        perr("expected index expression after '['");
            if (!consume(RBRACKET)) perr("expected ']' to close index");
            if (r->type.n < 0) tkerr(crtTk, "only an array can be indexed");
            Type tInt = createType(TB_INT, -1);
            if (!convTo(&idx.type, &tInt))
                tkerr(crtTk, "the array index is not convertible to int");
            r->type.n = -1;     /* indexing yields one element */
            r->lval = 1;
            r->ct = 0;
        } else if (consume(DOT)) {
            if (!consume(ID)) perr("expected field name after '.'");
            Token *tkName = consumedTk;
            if (r->type.tb != TB_STRUCT)
                tkerr(tkName, "a field can only be selected from a struct");
            Symbol *field = findSymbolIn(&r->type.s->members, tkName->text);
            if (!field)
                tkerr(tkName, "struct %s has no field named %s", r->type.s->name, tkName->text);
            *r = (Ret){ field->type, 1, field->type.n >= 0 };
        } else break;
    }
    return 1;
}

/* exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
 *            | CT_INT | CT_REAL | CT_CHAR | CT_STRING
 *            | LPAR expr RPAR
 * L6: an identifier must be defined; only a function may be called, and a
 * function may only be called; a call must match its signature. */
static int exprPrimary(Ret *r) {
    if (consume(ID)) {
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(tkName->text);
        if (!s) tkerr(tkName, "undefined symbol: %s", tkName->text);
        if (consume(LPAR)) {                   /* function call */
            if (s->kind != SK_FN)
                tkerr(tkName, "%s is not a function and cannot be called", tkName->text);
            int nParams = symbolsLen(&s->params);
            int argIdx = 0;
            if (!consume(RPAR)) {
                Ret arg;
                if (!expr(&arg)) perr("expected argument or ')'");
                for (;;) {
                    if (argIdx >= nParams)
                        tkerr(crtTk, "too many arguments in the call to %s", s->name);
                    if (!convTo(&arg.type, &s->params.begin[argIdx]->type))
                        tkerr(crtTk, "argument %d of %s has an incompatible type",
                              argIdx + 1, s->name);
                    argIdx++;
                    if (!consume(COMMA)) break;
                    if (!expr(&arg)) perr("expected expression after ','");
                }
                if (!consume(RPAR)) perr("expected ')' to close the argument list");
            }
            if (argIdx < nParams)
                tkerr(crtTk, "too few arguments in the call to %s", s->name);
            *r = (Ret){ s->type, 0, 1 };
        } else {
            if (s->kind == SK_FN)
                tkerr(tkName, "the function %s can only be called", tkName->text);
            *r = (Ret){ s->type, 1, s->type.n >= 0 };
        }
        return 1;
    }
    if (consume(CT_INT))    { *r = (Ret){ createType(TB_INT, -1), 0, 1 };    return 1; }
    if (consume(CT_REAL))   { *r = (Ret){ createType(TB_DOUBLE, -1), 0, 1 }; return 1; }
    if (consume(CT_CHAR))   { *r = (Ret){ createType(TB_CHAR, -1), 0, 1 };   return 1; }
    if (consume(CT_STRING)) { *r = (Ret){ createType(TB_CHAR, 0), 0, 1 };    return 1; }
    if (consume(LPAR)) {
        if (!expr(r))       perr("expected expression after '('");
        if (!consume(RPAR)) perr("expected ')' to close parenthesized expression");
        return 1;
    }
    return 0;
}

void parse(Token *tokenList) {
    crtTk = tokenList;
    consumedTk = NULL;
    owner = NULL;
    initDomains();        /* global domain + predefined functions */
    unit();
}
