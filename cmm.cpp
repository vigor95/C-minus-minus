#include "cmm.h"

int CMM::run(std::string source) {
    token = LEX.run(source);
    token.add_end_tok();
    std::cout << "Lexical analyse finished\n";
    token.show();
    return 0;
}

int CMM::run() {
    std::string outfile = "hello.bc", infile = "hello.c";
    bool emit_llvm_ir = 1;
    std::string source = [&]() -> std::string {
        std::ifstream ifs_src(infile);
        if (!ifs_src) {
            printf("file not found '%s'\n", infile.c_str());
            exit(0);
        }
        std::istreambuf_iterator<char> it(ifs_src), last;
        std::string src_all(it, last);
        return src_all;
    }();

    if (!outfile.empty())
        set_out_file_name(outfile);
    set_emit_llvm_ir(emit_llvm_ir);
    run(source);
    return 0;
}

void CMM::set_out_file_name(std::string name) {
    out_file_name = name;
}

void CMM::set_emit_llvm_ir(bool e) {
    emit_llvm_ir = e;
}

void error(const char *errs, ...) {
    va_list args;
    va_start(args, errs);
    vprintf(errs, args);
    puts("");
    va_end(args);
    exit(0);
}
