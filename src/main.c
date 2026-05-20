#include "atomc.h"
#include "symbols.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *readFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) err("cannot open '%s'", path);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) err("ftell failed on '%s'", path);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc((size_t)len + 1);
    if (!buf) err("out of memory reading '%s'", path);
    size_t r = fread(buf, 1, (size_t)len, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s [--parse] <file.c>\n", argv[0]);
        return 1;
    }
    int doParse = 0;
    const char *path = argv[1];
    if (argc >= 3 && (strcmp(argv[1], "--parse") == 0 || strcmp(argv[1], "-p") == 0)) {
        doParse = 1;
        path = argv[2];
    }
    char *src = readFile(path);
    pCrtCh = src;
    line = 1;
    while (getNextToken() != END) { /* drive the lexer until EOF */ }
    if (doParse) {
        parse(tokens);      /* parse + domain analysis; exits non-zero on any error */
        dumpSymbols();      /* on success, print the symbol table (L5 deliverable) */
        freeSymbols();
    } else {
        showTokens();
    }
    done();
    free(src);
    return 0;
}
