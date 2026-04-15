#include "atomc.h"
#include <stdio.h>
#include <stdlib.h>

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
        fprintf(stderr, "usage: %s <file.c>\n", argv[0]);
        return 1;
    }
    char *src = readFile(argv[1]);
    pCrtCh = src;
    line = 1;
    while (getNextToken() != END) { /* drive the lexer until EOF */ }
    showTokens();
    done();
    free(src);
    return 0;
}
