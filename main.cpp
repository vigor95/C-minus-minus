#include "cic.h"

// 输入源代码的文件名
chars filename;

int main(int argc, char** argv) {
    filename = "test.c";

    if (filename == "") {
        printf("no input file\n");
        return 10;
    }

    if (preprocess())
        return 1;
    std::cout << "preprocess ok\n";
    if (lex())
        return 2;
    std::cout << "lex ok\n";
    if (parse())
        return 3;
    std::cout << "parse ok\n";
    if (semantic())
        return 4;
    std::cout << "semantic ok\n";
    return 0;
}
