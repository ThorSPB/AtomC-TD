#ifndef SYMBOLS_H
#define SYMBOLS_H

/*
 * AtomC semantic-analysis vocabulary — shared by the L5 domain-analysis
 * and L6 type-analysis semantic actions in parser.c.
 *
 * Three things live here:
 *   - Type / Symbol     — what a declared name is.
 *   - the symbol table  — every Symbol, organised into a stack of domains
 *                         (scopes); domain operations are how L5 detects
 *                         redefinitions and resolves names.
 *   - Ret + type rules  — the synthesized attribute every expression
 *                         predicate carries in L6, plus the helpers that
 *                         decide what conversions are legal.
 */

/* --- Type --------------------------------------------------------------- */
/* The base type of a value. TB_VOID is only ever a function return type. */
enum { TB_INT, TB_DOUBLE, TB_CHAR, TB_STRUCT, TB_VOID };

typedef struct _Symbol Symbol;

typedef struct {
    int tb;        /* TB_* — the base type */
    Symbol *s;     /* the struct's symbol, used only when tb == TB_STRUCT */
    int n;         /* -1 = not an array; 0 = array without a size; >0 = array of n */
} Type;

/* --- Symbols: a growable Symbol* list ----------------------------------- */
typedef struct {
    Symbol **begin;   /* first symbol, or NULL when empty */
    Symbol **end;     /* one past the last symbol */
    Symbol **after;   /* one past the allocated space */
} Symbols;

/* What kind of thing a Symbol names. */
enum { SK_VAR, SK_PARAM, SK_FN, SK_STRUCT };

struct _Symbol {
    const char *name;   /* points into a Token's text — not owned by the Symbol */
    int kind;           /* SK_* */
    Type type;          /* for SK_FN this is the return type */
    int depth;          /* 0 = global, 1 = fn/struct scope, 2+ = nested blocks */
    int isExtern;       /* 1 for the predefined functions (put_s, get_i, ...) */
    Symbol *owner;      /* the enclosing fn/struct, or NULL at global scope */
    Symbols params;     /* SK_FN     — parameters; outlives the function's domain */
    Symbols members;    /* SK_STRUCT — fields; outlives the struct's domain */
};

/* --- Symbols list operations -------------------------------------------- */
void initSymbols(Symbols *ss);
Symbol *addSymbolTo(Symbols *ss, Symbol *s);          /* append s, return s */
Symbol *findSymbolIn(Symbols *ss, const char *name);  /* search a list; NULL if absent */
int symbolsLen(Symbols *ss);

/* --- symbol table / domain (scope) stack -------------------------------- */
Symbol *newSymbol(const char *name, int kind);   /* allocate + register for cleanup */
void pushDomain(void);                           /* enter a nested scope */
void dropDomain(void);                           /* leave the innermost scope */
Symbol *addSymbolToDomain(Symbol *s);            /* add s to the innermost domain */
Symbol *findSymbolInDomain(const char *name);    /* innermost domain only (redefinition check) */
Symbol *findSymbol(const char *name);            /* all domains, innermost outward (name resolution) */

void initDomains(void);   /* create the global domain + the predefined functions */
void freeSymbols(void);   /* release every Symbol and domain */
void dumpSymbols(void);   /* print the symbol table — the L5 deliverable */

/* --- L6 type analysis --------------------------------------------------- */
/*
 * Ret is the synthesized attribute every expression predicate returns.
 *   lval — the expression is a left-value (names a storage location)
 *   ct   — the expression is "const": not a writable assignment target
 *          even if it has an address (a literal, an arithmetic result, or
 *          an array name — you cannot assign to an array as a whole).
 */
typedef struct {
    Type type;
    int lval;
    int ct;
} Ret;

Type createType(int tb, int n);
int typeEq(const Type *a, const Type *b);
int canBeScalar(const Ret *r);                              /* int/double/char, not an array */
int convTo(const Type *src, const Type *dst);               /* is src convertible to dst? */
int arithTypeTo(const Type *t1, const Type *t2, Type *dst); /* result type of t1 <op> t2 */
const char *typeBaseName(int tb);                           /* "int", "double", ... — for messages */

#endif
