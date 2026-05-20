/*
 * AtomC symbol table + domain (scope) stack + type-analysis helpers.
 *
 * The symbol table is a stack of "domains". A domain is one lexical scope:
 * the global scope, a function's parameter+local scope, a struct's field
 * scope, or a nested { } block. pushDomain() enters a scope, dropDomain()
 * leaves it.
 *
 * Memory model: every Symbol ever created is also recorded in the private
 * `allSymbols` list, which owns them. Domains only hold *views* (pointer
 * arrays); dropDomain() frees a domain's pointer array but never the
 * Symbol objects — so the parameter / member lists copied onto a function
 * or struct symbol stay valid after that scope is gone. freeSymbols()
 * releases everything at the end.
 */
#include "atomc.h"
#include "symbols.h"
#include <string.h>

/* --- Symbols: a growable Symbol* list ----------------------------------- */

void initSymbols(Symbols *ss) {
    ss->begin = ss->end = ss->after = NULL;
}

Symbol *addSymbolTo(Symbols *ss, Symbol *s) {
    if (ss->end == ss->after) {                 /* full — grow (double the room) */
        int count = (int)(ss->after - ss->begin);
        int n = count ? count * 2 : 1;
        Symbol **p = (Symbol**)realloc(ss->begin, (size_t)n * sizeof(Symbol*));
        if (!p) err("not enough memory");
        ss->begin = p;
        ss->end = p + count;
        ss->after = p + n;
    }
    *ss->end++ = s;
    return s;
}

/* Search right-to-left so the most recent definition shadows earlier ones. */
Symbol *findSymbolIn(Symbols *ss, const char *name) {
    for (Symbol **p = ss->end; p != ss->begin; ) {
        p--;
        if (strcmp((*p)->name, name) == 0) return *p;
    }
    return NULL;
}

int symbolsLen(Symbols *ss) {
    return (int)(ss->end - ss->begin);
}

/* --- the symbol table --------------------------------------------------- */

/* One lexical scope. Domains form a stack via `parent`. */
typedef struct _Domain {
    Symbols symbols;
    struct _Domain *parent;
} Domain;

static Domain *crtDomain = NULL;   /* innermost open scope */
static int crtDepth = -1;          /* depth of crtDomain; -1 = no domain yet */
static Symbols allSymbols;         /* owns every Symbol, for freeSymbols() */

Symbol *newSymbol(const char *name, int kind) {
    Symbol *s;
    SAFEALLOC(s, Symbol);
    s->name = name;
    s->kind = kind;
    s->type = createType(TB_INT, -1);
    s->depth = crtDepth < 0 ? 0 : crtDepth;
    s->isExtern = 0;
    s->owner = NULL;
    initSymbols(&s->params);
    initSymbols(&s->members);
    addSymbolTo(&allSymbols, s);
    return s;
}

void pushDomain(void) {
    Domain *d;
    SAFEALLOC(d, Domain);
    initSymbols(&d->symbols);
    d->parent = crtDomain;
    crtDomain = d;
    crtDepth++;
}

void dropDomain(void) {
    Domain *d = crtDomain;
    if (!d) return;
    crtDomain = d->parent;
    crtDepth--;
    free(d->symbols.begin);   /* the view only — Symbol objects belong to allSymbols */
    free(d);
}

Symbol *addSymbolToDomain(Symbol *s) {
    s->depth = crtDepth;
    return addSymbolTo(&crtDomain->symbols, s);
}

Symbol *findSymbolInDomain(const char *name) {
    return crtDomain ? findSymbolIn(&crtDomain->symbols, name) : NULL;
}

Symbol *findSymbol(const char *name) {
    for (Domain *d = crtDomain; d; d = d->parent) {
        Symbol *s = findSymbolIn(&d->symbols, name);
        if (s) return s;
    }
    return NULL;
}

/* --- predefined functions ----------------------------------------------- */
/*
 * AtomC programs may call put_s / get_i / ... without declaring them, so
 * the compiler seeds the global domain with their signatures. Without this,
 * domain/type analysis would reject every call as an undefined symbol.
 */
static Symbol *addExtFn(const char *name, Type ret) {
    Symbol *fn = newSymbol(name, SK_FN);
    fn->type = ret;
    fn->isExtern = 1;
    addSymbolToDomain(fn);
    return fn;
}

static void addExtParam(Symbol *fn, const char *name, Type t) {
    Symbol *p = newSymbol(name, SK_PARAM);
    p->type = t;
    addSymbolTo(&fn->params, p);
}

void initDomains(void) {
    initSymbols(&allSymbols);
    pushDomain();                                  /* the global domain (depth 0) */

    Symbol *s;
    s = addExtFn("put_s", createType(TB_VOID, -1));   addExtParam(s, "s", createType(TB_CHAR, 0));
    s = addExtFn("get_s", createType(TB_VOID, -1));   addExtParam(s, "s", createType(TB_CHAR, 0));
    s = addExtFn("put_i", createType(TB_VOID, -1));   addExtParam(s, "i", createType(TB_INT, -1));
    addExtFn("get_i", createType(TB_INT, -1));
    s = addExtFn("put_d", createType(TB_VOID, -1));   addExtParam(s, "d", createType(TB_DOUBLE, -1));
    addExtFn("get_d", createType(TB_DOUBLE, -1));
    s = addExtFn("put_c", createType(TB_VOID, -1));   addExtParam(s, "c", createType(TB_CHAR, -1));
    addExtFn("get_c", createType(TB_CHAR, -1));
    addExtFn("seconds", createType(TB_DOUBLE, -1));
}

void freeSymbols(void) {
    while (crtDomain) dropDomain();
    for (Symbol **p = allSymbols.begin; p != allSymbols.end; p++) {
        free((*p)->params.begin);
        free((*p)->members.begin);
        free(*p);
    }
    free(allSymbols.begin);
    initSymbols(&allSymbols);
}

/* --- symbol table dump (the L5 deliverable) ----------------------------- */

static void printType(const Type *t) {
    printf("%s", typeBaseName(t->tb));
    if (t->tb == TB_STRUCT && t->s) printf(" %s", t->s->name);
    if (t->n == 0)      printf("[]");
    else if (t->n > 0)  printf("[%d]", t->n);
}

static const char *kindName(int kind) {
    switch (kind) {
    case SK_VAR:    return "var";
    case SK_PARAM:  return "param";
    case SK_FN:     return "fn";
    case SK_STRUCT: return "struct";
    default:        return "?";
    }
}

void dumpSymbols(void) {
    for (Symbol **p = allSymbols.begin; p != allSymbols.end; p++) {
        Symbol *s = *p;
        if (s->kind == SK_PARAM) continue;            /* shown under their function */
        if (s->kind == SK_VAR && s->owner && s->owner->kind == SK_STRUCT)
            continue;                                 /* shown under their struct */
        printf("%-10s  %-7s  depth=%d  ", s->name, kindName(s->kind), s->depth);
        if (s->kind == SK_FN) {
            printf("(");
            for (Symbol **a = s->params.begin; a != s->params.end; a++) {
                if (a != s->params.begin) printf(", ");
                printType(&(*a)->type);
                printf(" %s", (*a)->name);
            }
            printf(") -> ");
            printType(&s->type);
            if (s->isExtern) printf("  [extern]");
        } else {
            printType(&s->type);
        }
        putchar('\n');
        if (s->kind == SK_STRUCT) {
            for (Symbol **m = s->members.begin; m != s->members.end; m++) {
                printf("    .%-8s  ", (*m)->name);
                printType(&(*m)->type);
                putchar('\n');
            }
        }
    }
}

/* --- L6 type-analysis helpers ------------------------------------------- */

Type createType(int tb, int n) {
    Type t;
    t.tb = tb;
    t.s = NULL;
    t.n = n;
    return t;
}

const char *typeBaseName(int tb) {
    switch (tb) {
    case TB_INT:    return "int";
    case TB_DOUBLE: return "double";
    case TB_CHAR:   return "char";
    case TB_STRUCT: return "struct";
    case TB_VOID:   return "void";
    default:        return "?";
    }
}

int typeEq(const Type *a, const Type *b) {
    if (a->tb != b->tb) return 0;
    if (a->tb == TB_STRUCT && a->s != b->s) return 0;
    return 1;
}

/* A scalar is a non-array int/double/char — the only thing a condition,
 * an arithmetic operand, or a unary operand may be. */
int canBeScalar(const Ret *r) {
    if (r->type.n >= 0) return 0;                 /* arrays are not scalar */
    switch (r->type.tb) {
    case TB_INT: case TB_DOUBLE: case TB_CHAR: return 1;
    default: return 0;                            /* struct, void */
    }
}

/* Can a value of type src be used where dst is expected? */
int convTo(const Type *src, const Type *dst) {
    if (src->n >= 0 || dst->n >= 0) {             /* an array is involved */
        if (src->n < 0 || dst->n < 0) return 0;   /* array <-> non-array: never */
        return src->tb == dst->tb;                /* array <-> array: same base only */
    }
    switch (src->tb) {
    case TB_CHAR: case TB_INT: case TB_DOUBLE:
        return dst->tb == TB_CHAR || dst->tb == TB_INT || dst->tb == TB_DOUBLE;
    case TB_STRUCT:
        return dst->tb == TB_STRUCT && src->s == dst->s;
    default:
        return 0;                                 /* void converts to nothing */
    }
}

/* The result type of an arithmetic/relational operation on t1 and t2.
 * Both must be scalar; char promotes to int; double wins over int. */
int arithTypeTo(const Type *t1, const Type *t2, Type *dst) {
    if (t1->n >= 0 || t2->n >= 0) return 0;       /* arrays have no arithmetic */
    int ok1 = t1->tb == TB_INT || t1->tb == TB_CHAR || t1->tb == TB_DOUBLE;
    int ok2 = t2->tb == TB_INT || t2->tb == TB_CHAR || t2->tb == TB_DOUBLE;
    if (!ok1 || !ok2) return 0;
    *dst = createType((t1->tb == TB_DOUBLE || t2->tb == TB_DOUBLE) ? TB_DOUBLE : TB_INT, -1);
    return 1;
}
