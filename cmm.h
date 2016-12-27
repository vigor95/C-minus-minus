#pragma once

#include "common.h"
#include "lexer.h"

class CMM {
private:
    Token token;
    Lexer LEX;
    
    std::string out_file_name = "a.bc";
    bool emit_llvm_ir = true;
    
public:
    int argc;
    char **argv;
    CMM(int _argc, char **_argv): argc(_argc), argv(_argv) {}

    void set_out_file_name(std::string);
    void set_emit_llvm_ir(bool);

    int run();
    int run(std::string);
};
