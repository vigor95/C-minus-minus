#include "cic.h"
#include "ast.h"
#include "expr.h"

static struct tokenOp token_ops[] = {
#define TOKENOP(tok, bop, uop) {bop, uop},
#include "tokenop.h"
#undef TOKENOP
};

static int Prec[] = {
#define OPINFO(op, prec, name, func, opcode) prec,
#include "opinfo.h"
    0
#undef OPINFO
};

AstExpression constant0;
char *op_names[] = {
#define OPINFO(op, prec, name, func, opcode) name,
#include "opinfo.h"
    NULL
#undef OPINFO
};
