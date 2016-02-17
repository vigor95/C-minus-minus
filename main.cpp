#include <unistd.h>
#include <string>
#include "cic.h"

const char *infile;
static std::string outfile;

int main(int argc, char **argv) {
    printf("Hello world!\n");
    if (argc <= 1) {
        printf("Too less arguments!\n");
        exit(1);
    }
    infile = argv[1];
    lexInit(infile);
    while (1) {
        Token *tk = lex();
        if (tk->kind == TEOF) break;
        printf("%d %d\n", tk->line, tk->kind);
    }
}
