#include <unistd.h>
#include <string>
#include "cic.h"

const char *infile;
static std::string outfile;
std::string table[] = {"indent", "keyword", "number", "char","string",
"eof", "!invalid", "min_cpp_token", "newline", "space", "macro"};

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
        if (tk->kind != TNEWLINE) {
            std::cout << tk->line << ": " << table[tk->kind] << std::endl;
        }
        if (tk->kind == TEOF) break;
    }
}
