/*
 * AtomC lexical analyzer (FSM implementation)
 *
 * State numbers below correspond 1:1 to diagrams/src/unified_td.dot.
 * Every case block is one TD state; transitions either consume a character
 * and change state, or fall through as the "(else)" transition.
 */
#include "atomc.h"
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

Token *tokens = NULL;
Token *lastToken = NULL;
int line = 1;
const char *pCrtCh = NULL;

/* Keyword table — iterated linearly after an ID is recognized */
static const struct { const char *name; int code; } KEYWORDS[] = {
    {"break",  BREAK},  {"char",   CHAR},   {"double", DOUBLE},
    {"else",   ELSE},   {"for",    FOR},    {"if",     IF},
    {"int",    INT},    {"return", RETURN}, {"struct", STRUCT},
    {"void",   VOID},   {"while",  WHILE},
    {NULL, 0}
};

/* Token name table — MUST stay in sync with the enum in atomc.h */
static const char *TOKEN_NAMES[] = {
    "ID",
    "BREAK","CHAR","DOUBLE","ELSE","FOR","IF","INT","RETURN","STRUCT","VOID","WHILE",
    "CT_INT","CT_REAL","CT_CHAR","CT_STRING",
    "COMMA","SEMICOLON","LPAR","RPAR","LBRACKET","RBRACKET","LACC","RACC","END",
    "ADD","SUB","MUL","DIV","DOT","AND","OR","NOT",
    "ASSIGN","EQUAL","NOTEQ","LESS","LESSEQ","GREATER","GREATEREQ"
};

const char *tokenName(int code) {
    int n = (int)(sizeof(TOKEN_NAMES) / sizeof(TOKEN_NAMES[0]));
    if (code < 0 || code >= n) return "?";
    return TOKEN_NAMES[code];
}

void err(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
    va_end(va);
    exit(-1);
}

void tkerr(const Token *tk, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "error in line %d: ", tk->line);
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
    va_end(va);
    exit(-1);
}

Token *addTk(int code) {
    Token *tk;
    SAFEALLOC(tk, Token);
    tk->code = code;
    tk->text = NULL;       /* also zeroes i and r (union) */
    tk->line = line;
    tk->next = NULL;
    if (lastToken) lastToken->next = tk;
    else           tokens = tk;
    lastToken = tk;
    return tk;
}

static char *createString(const char *start, const char *end) {
    size_t n = (size_t)(end - start);
    char *s = (char*)malloc(n + 1);
    if (!s) err("not enough memory");
    memcpy(s, start, n);
    s[n] = '\0';
    return s;
}

/*
 * Escape handling for CT_CHAR / CT_STRING.
 * The formal AtomC rules (references/AtomC - reguli lexicale.pdf) define
 *     CT_CHAR:   ['] [^'] [']
 *     CT_STRING: ["] [^"]* ["]
 * which literally forbid any backslash handling. The provided tests/8.c
 * uses "\"equal\"\t\t(h,o)" and '\\', so we extend the definitions with
 * the standard C escape set. Treat this as a practical relaxation of the
 * rules, not a change to the underlying TD shape.
 */
static char decodeEscape(char c) {
    switch (c) {
    case 'n':  return '\n';
    case 't':  return '\t';
    case 'r':  return '\r';
    case '0':  return '\0';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"':  return '"';
    default:   return c;  /* unknown escape: keep the literal char */
    }
}

static char *decodeString(const char *start, const char *end) {
    size_t n = (size_t)(end - start);
    char *s = (char*)malloc(n + 1);
    if (!s) err("not enough memory");
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (start[i] == '\\' && i + 1 < n) {
            s[j++] = decodeEscape(start[i + 1]);
            i++;
        } else {
            s[j++] = start[i];
        }
    }
    s[j] = '\0';
    return s;
}

static int idOrKeyword(const char *start, const char *end) {
    size_t n = (size_t)(end - start);
    for (int i = 0; KEYWORDS[i].name; i++) {
        if (strlen(KEYWORDS[i].name) == n &&
            memcmp(KEYWORDS[i].name, start, n) == 0)
            return KEYWORDS[i].code;
    }
    return ID;
}

/* Copy [start, end) into a bounded stack buffer so strtol/strtod can parse it. */
static void copyLiteral(char *buf, size_t bufSize, const char *start, const char *end) {
    size_t n = (size_t)(end - start);
    if (n >= bufSize) err("numeric literal too long");
    memcpy(buf, start, n);
    buf[n] = '\0';
}

static int emitInt(const char *start, const char *end) {
    char buf[64];
    Token *tk = addTk(CT_INT);
    copyLiteral(buf, sizeof(buf), start, end);
    /* base 0 => strtol auto-detects 0x (hex), leading 0 (octal), else decimal */
    tk->i = strtol(buf, NULL, 0);
    return CT_INT;
}

static int emitReal(const char *start, const char *end) {
    char buf[64];
    Token *tk = addTk(CT_REAL);
    copyLiteral(buf, sizeof(buf), start, end);
    tk->r = strtod(buf, NULL);
    return CT_REAL;
}

int getNextToken(void) {
    int state = 0;
    const char *pStartCh = NULL;
    Token *tk;

    for (;;) {
        char ch = *pCrtCh;
        switch (state) {

        case 0: /* s0 — shared initial state for every sub-diagram */
            if (ch == ' ' || ch == '\r' || ch == '\t') { pCrtCh++; break; }
            if (ch == '\n')                            { line++; pCrtCh++; break; }
            if (ch == 0)                               { addTk(END); return END; }

            /* Numbers: leading '0' vs '1'..'9' split (for octal/hex) */
            if (ch == '0') { pStartCh = pCrtCh; pCrtCh++; state = 2; break; }
            if (ch >= '1' && ch <= '9') { pStartCh = pCrtCh; pCrtCh++; state = 1; break; }

            /* Identifier / keyword */
            if (isalpha((unsigned char)ch) || ch == '_') {
                pStartCh = pCrtCh; pCrtCh++; state = 11; break;
            }

            /* Char / string literals */
            if (ch == '\'') { pStartCh = pCrtCh; pCrtCh++; state = 12; break; }
            if (ch == '"')  { pStartCh = pCrtCh; pCrtCh++; state = 14; break; }

            /* Operators with a 1-character prefix */
            if (ch == '/') { pCrtCh++; state = 15; break; }
            if (ch == '!') { pCrtCh++; state = 17; break; }
            if (ch == '=') { pCrtCh++; state = 18; break; }
            if (ch == '<') { pCrtCh++; state = 19; break; }
            if (ch == '>') { pCrtCh++; state = 20; break; }
            if (ch == '&') { pCrtCh++; state = 21; break; }
            if (ch == '|') { pCrtCh++; state = 22; break; }

            /* Single-character tokens */
            if (ch == ',') { pCrtCh++; addTk(COMMA);     return COMMA; }
            if (ch == ';') { pCrtCh++; addTk(SEMICOLON); return SEMICOLON; }
            if (ch == '(') { pCrtCh++; addTk(LPAR);      return LPAR; }
            if (ch == ')') { pCrtCh++; addTk(RPAR);      return RPAR; }
            if (ch == '[') { pCrtCh++; addTk(LBRACKET);  return LBRACKET; }
            if (ch == ']') { pCrtCh++; addTk(RBRACKET);  return RBRACKET; }
            if (ch == '{') { pCrtCh++; addTk(LACC);      return LACC; }
            if (ch == '}') { pCrtCh++; addTk(RACC);      return RACC; }
            if (ch == '+') { pCrtCh++; addTk(ADD);       return ADD; }
            if (ch == '-') { pCrtCh++; addTk(SUB);       return SUB; }
            if (ch == '*') { pCrtCh++; addTk(MUL);       return MUL; }
            if (ch == '.') { pCrtCh++; addTk(DOT);       return DOT; }

            tkerr(addTk(END), "invalid character '%c' (0x%02x)", ch, (unsigned char)ch);
            break;

        case 1: /* decimal integer body (after first 1..9) */
            if (ch >= '0' && ch <= '9') { pCrtCh++; break; }
            if (ch == '.')              { pCrtCh++; state = 6; break; }
            if (ch == 'e' || ch == 'E') { pCrtCh++; state = 8; break; }
            return emitInt(pStartCh, pCrtCh);

        case 2: /* leading zero — could be 0, octal, hex, or real */
            if (ch == 'x' || ch == 'X')   { pCrtCh++; state = 3; break; }
            if (ch >= '0' && ch <= '7')   { pCrtCh++; state = 5; break; }
            if (ch == '.')                { pCrtCh++; state = 6; break; }
            if (ch == 'e' || ch == 'E')   { pCrtCh++; state = 8; break; }
            return emitInt(pStartCh, pCrtCh);

        case 3: /* after "0x" — at least one hex digit required */
            if (isxdigit((unsigned char)ch)) { pCrtCh++; state = 4; break; }
            tkerr(addTk(END), "hex digit expected after '0x'");
            break;

        case 4: /* hex digits continuation */
            if (isxdigit((unsigned char)ch)) { pCrtCh++; break; }
            return emitInt(pStartCh, pCrtCh);

        case 5: /* octal digits continuation */
            if (ch >= '0' && ch <= '7') { pCrtCh++; break; }
            return emitInt(pStartCh, pCrtCh);

        case 6: /* after '.' — at least one digit required for CT_REAL */
            if (ch >= '0' && ch <= '9') { pCrtCh++; state = 7; break; }
            tkerr(addTk(END), "digit expected after '.'");
            break;

        case 7: /* real fractional digits */
            if (ch >= '0' && ch <= '9') { pCrtCh++; break; }
            if (ch == 'e' || ch == 'E') { pCrtCh++; state = 8; break; }
            return emitReal(pStartCh, pCrtCh);

        case 8: /* after 'e'/'E' — sign or digit required */
            if (ch == '+' || ch == '-') { pCrtCh++; state = 9; break; }
            if (ch >= '0' && ch <= '9') { pCrtCh++; state = 10; break; }
            tkerr(addTk(END), "digit or sign expected in exponent");
            break;

        case 9: /* after exponent sign — digit required */
            if (ch >= '0' && ch <= '9') { pCrtCh++; state = 10; break; }
            tkerr(addTk(END), "digit expected in exponent");
            break;

        case 10: /* exponent digits */
            if (ch >= '0' && ch <= '9') { pCrtCh++; break; }
            return emitReal(pStartCh, pCrtCh);

        case 11: /* identifier body */
            if (isalnum((unsigned char)ch) || ch == '_') { pCrtCh++; break; }
            {
                int code = idOrKeyword(pStartCh, pCrtCh);
                tk = addTk(code);
                if (code == ID) tk->text = createString(pStartCh, pCrtCh);
                return code;
            }

        case 12: /* after opening ' — expect the single character (or escape) */
            if (ch == '\'') tkerr(addTk(END), "empty char constant");
            if (ch == 0)    tkerr(addTk(END), "unterminated char constant");
            if (ch == '\\') {               /* escape sequence */
                pCrtCh++;
                if (*pCrtCh == 0) tkerr(addTk(END), "unterminated char constant");
                pCrtCh++;
                state = 13;
                break;
            }
            if (ch == '\n') line++;
            pCrtCh++;
            state = 13;
            break;

        case 13: /* after ' x — expect closing ' */
            if (ch == '\'') {
                pCrtCh++;
                tk = addTk(CT_CHAR);
                /* pStartCh is at opening '. pStartCh[1] is either the literal
                 * content char or the backslash of an escape sequence. */
                if (pStartCh[1] == '\\')
                    tk->i = (unsigned char)decodeEscape(pStartCh[2]);
                else
                    tk->i = (unsigned char)pStartCh[1];
                return CT_CHAR;
            }
            tkerr(addTk(END), "missing closing '");
            break;

        case 14: /* string body — any char except unescaped " */
            if (ch == '"') {
                pCrtCh++;
                tk = addTk(CT_STRING);
                tk->text = decodeString(pStartCh + 1, pCrtCh - 1); /* strip quotes */
                return CT_STRING;
            }
            if (ch == 0) tkerr(addTk(END), "unterminated string");
            if (ch == '\\') {               /* consume backslash + next char as a pair */
                pCrtCh++;
                if (*pCrtCh == 0) tkerr(addTk(END), "unterminated string");
                if (*pCrtCh == '\n') line++;
                pCrtCh++;
                break;
            }
            if (ch == '\n') line++;
            pCrtCh++;
            break;

        case 15: /* after '/' — DIV or start of line comment */
            if (ch == '/') { pCrtCh++; state = 16; break; }
            addTk(DIV);
            return DIV;

        case 16: /* line comment body — consume until newline/EOF (no token emitted) */
            if (ch == '\n' || ch == '\r' || ch == 0) { state = 0; break; }
            pCrtCh++;
            break;

        case 17: /* after '!' */
            if (ch == '=') { pCrtCh++; addTk(NOTEQ); return NOTEQ; }
            addTk(NOT); return NOT;

        case 18: /* after '=' */
            if (ch == '=') { pCrtCh++; addTk(EQUAL); return EQUAL; }
            addTk(ASSIGN); return ASSIGN;

        case 19: /* after '<' */
            if (ch == '=') { pCrtCh++; addTk(LESSEQ); return LESSEQ; }
            addTk(LESS); return LESS;

        case 20: /* after '>' */
            if (ch == '=') { pCrtCh++; addTk(GREATEREQ); return GREATEREQ; }
            addTk(GREATER); return GREATER;

        case 21: /* after '&' — AtomC requires '&&' (no bitwise AND) */
            if (ch == '&') { pCrtCh++; addTk(AND); return AND; }
            tkerr(addTk(END), "'&' expected (AtomC has no bitwise AND)");
            break;

        case 22: /* after '|' — AtomC requires '||' (no bitwise OR) */
            if (ch == '|') { pCrtCh++; addTk(OR); return OR; }
            tkerr(addTk(END), "'|' expected (AtomC has no bitwise OR)");
            break;
        }
    }
}

void showTokens(void) {
    for (Token *tk = tokens; tk; tk = tk->next) {
        printf("%4d  %s", tk->line, TOKEN_NAMES[tk->code]);
        switch (tk->code) {
        case ID:
        case CT_STRING:
            printf(":%s", tk->text);
            break;
        case CT_INT:
            printf(":%ld", tk->i);
            break;
        case CT_CHAR:
            printf(":'%c'", (int)tk->i);
            break;
        case CT_REAL:
            printf(":%g", tk->r);
            break;
        default:
            break;
        }
        putchar('\n');
    }
}

void done(void) {
    Token *tk = tokens;
    while (tk) {
        Token *next = tk->next;
        if (tk->code == ID || tk->code == CT_STRING) free(tk->text);
        free(tk);
        tk = next;
    }
    tokens = lastToken = NULL;
}
