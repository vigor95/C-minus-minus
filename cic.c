#include "cic.h"
#include "ast.h"

static int dump_ast;
static int dump_ir;

FILE *ast_file;
FILE *ir_file;
FILE *asm_file;

Heap current_heap;

HEAP(program_heap);

HEAP(file_heap);

HEAP(string_heap);

int warning_count;
int error_count;
Vector extra_white_space;
Vector extra_keywords;

static void Initialize() {
    current_heap = &file_heap;
    error_count = warning_count = 0;
    InitSymbolTable();
    ast_file = ir_file = asm_file = NULL;
}

static void Finalize() {
    FreeHeap(&file_heap);
}

static void Compile(char *file) {
    AstTransUnit trans_unit;
    Initialize();

    trans_unit = ParseTransUnit(file);

    CheckTransUnit(trans_unit);

    if (error_count != 0)
        goto exit;

    if (dump_ast)
        DumpTransUnit(trans_unit);

exit:
    Finalize();
}

static int ParseCmd(int argc, char *argv[]) {
    int i;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--ast"))
            dump_ast = 1;
        else if (strcmp(argv[i], "--ir"))
            dump_ir = 1;
        else
            return i;
    }

    return i;
}

int main(int argc, char *argv[]) {
    int i = 0;

    current_heap = &program_heap;
    argc--; argv++;

    i = ParseCmd(argc, argv);

    SetupLexer();
    SetupTypeSystem();

    while (i < argc)
        Compile(argv[i++]);

    return error_count != 0;
}
