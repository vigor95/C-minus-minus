#include <unistd.h>
#include <string>
#include "cic.h"

chars filename;

int main(int argc, char **argv) {
    printf("Hello world!\n");
    if (argc <= 1) {
        printf("Too less arguments!\n");
        exit(1);
    }
    filename = argv[1];
    if (preprocess()) return 1;
    std::cout << "preprocess ok\n";
    if (lex()) return 2;
    std::cout << "lex ok\n";
    if (parse()) return 3;
    std::cout << "parse ok\n";
    return 0;
}
