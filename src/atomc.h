#ifndef ATOMC_H
#define ATOMC_H

#include <stdio.h>
#include <stdlib.h>

/* Token codes — order must match TOKEN_NAMES[] in lex.c */
enum {
    ID,
    /* Keywords (AtomC - reguli lexicale) */
    BREAK, CHAR, DOUBLE, ELSE, FOR, IF, INT, RETURN, STRUCT, VOID, WHILE,
    /* Constants */
    CT_INT, CT_REAL, CT_CHAR, CT_STRING,
    /* Delimiters */
    COMMA, SEMICOLON, LPAR, RPAR, LBRACKET, RBRACKET, LACC, RACC, END,
    /* Operators */
    ADD, SUB, MUL, DIV, DOT, AND, OR, NOT,
    ASSIGN, EQUAL, NOTEQ, LESS, LESSEQ, GREATER, GREATEREQ
};

typedef struct _Token {
    int code;
    union {
        char *text;     /* ID, CT_STRING — dynamically allocated */
        long int i;     /* CT_INT, CT_CHAR */
        double r;       /* CT_REAL */
    };
    int line;
    struct _Token *next;
} Token;

/* Globals (defined in lex.c) */
extern Token *tokens;
extern Token *lastToken;
extern int line;
extern const char *pCrtCh;

/* API */
void err(const char *fmt, ...);
void tkerr(const Token *tk, const char *fmt, ...);
Token *addTk(int code);
int getNextToken(void);
void showTokens(void);
void done(void);

#define SAFEALLOC(var, Type) \
    do { if (((var) = (Type*)malloc(sizeof(Type))) == NULL) err("not enough memory"); } while (0)

#endif
